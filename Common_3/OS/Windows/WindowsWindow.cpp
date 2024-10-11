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

#include "../../Application/Config.h"

#include "../../Utilities/ThirdParty/OpenSource/Nothings/stb_ds.h"
#include "../../Utilities/ThirdParty/OpenSource/bstrlib/bstrlib.h"

#include "../../Application/Interfaces/IApp.h"
#include "../../OS/Interfaces/IOperatingSystem.h"
#include "../../Utilities/Interfaces/ILog.h"

#include "../../Utilities/Math/Algorithms.h"
#include "../../Utilities/Math/MathTypes.h"

#pragma comment(lib, "WinMM.lib")
#include <windowsx.h>

#define elementsOf(a)      (sizeof(a) / sizeof((a)[0]))

#define LEFTEXTENDWIDTH    6
#define RIGHTEXTENDWIDTH   6
#define BOTTOMEXTENDWIDTH  20
#define TOPEXTENDWIDTH     27

#define FORGE_WINDOW_CLASS L"The Forge"

IApp* pWindowAppRef = NULL;

WindowDesc* gWindow = nullptr;
bool        gWindowIsResizing = false;

bool      gWindowClassInitialized = false;
WNDCLASSW gWindowClass;

bool gCursorVisible = true;
bool gCursorInsideRectangle = false;

MonitorDesc* gMonitors = nullptr;
uint32_t     gMonitorCount = 0;

// WindowsBase.cpp
extern CustomMessageProcessor sCustomProc;

//------------------------------------------------------------------------
// STATIC STRUCTS
//------------------------------------------------------------------------

struct MonitorInfo
{
    unsigned index;
    WCHAR    adapterName[32];
};

//------------------------------------------------------------------------
// STATIC HELPER FUNCTIONS
//------------------------------------------------------------------------

RECT         getCenteredWindowRect(WindowDesc* winDesc);
static DWORD PrepareStyleMask(WindowDesc* winDesc);

// Same as GetClientRect but applies screen space offset
// Allows us to keep track of client rect instead of window rect
// GetClientRect docs: https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getclientrect
static BOOL GetClientRectWithOffset(HWND hWnd, LPRECT lpRect)
{
    RECT clientRect = {};
    if (GetClientRect(hWnd, &clientRect) == FALSE)
    {
        return FALSE;
    }

    POINT offset = {};
    if (ClientToScreen(hWnd, &offset) == FALSE)
    {
        return FALSE;
    }

    lpRect->left = clientRect.left + offset.x;
    lpRect->right = clientRect.right + offset.x;
    lpRect->top = clientRect.top + offset.y;
    lpRect->bottom = clientRect.bottom + offset.y;
    return TRUE;
}

// Helper function to convert window rect to client rect
static void WindowRectFromClient(RectDesc* rectDescOut, const RectDesc* clientRect, DWORD windowStyle, bool bMenu)
{
    RECT windowedRect = {};
    windowedRect.left = clientRect->left;
    windowedRect.right = clientRect->right;
    windowedRect.top = clientRect->top;
    windowedRect.bottom = clientRect->bottom;

    AdjustWindowRect(&windowedRect, windowStyle, bMenu);

    rectDescOut->left = windowedRect.left;
    rectDescOut->right = windowedRect.right;
    rectDescOut->top = windowedRect.top;
    rectDescOut->bottom = windowedRect.bottom;
}

// Fetches the fullscreen rectangle from the system and assigns it to WindowDesc
static void ResetWindowDescFullScreenRect(WindowDesc* winDesc)
{
    HMONITOR      currentMonitor = MonitorFromWindow((HWND)winDesc->handle.window, MONITOR_DEFAULTTONEAREST);
    MONITORINFOEX info;
    info.cbSize = sizeof(MONITORINFOEX);
    bool infoRead = GetMonitorInfo(currentMonitor, &info);
    ASSERT(infoRead);

    winDesc->fullscreenRect.left = info.rcMonitor.left;
    winDesc->fullscreenRect.top = info.rcMonitor.top;
    winDesc->fullscreenRect.right = info.rcMonitor.right;
    winDesc->fullscreenRect.bottom = info.rcMonitor.bottom;
}

// Fetches the client rectangle from the system and assigns it to WindowDesc
static void ResetWindowDescClientRect(WindowDesc* winDesc)
{
    RECT clientRect;
    HWND hwnd = (HWND)winDesc->handle.window;

    GetClientRectWithOffset(hwnd, &clientRect);

    winDesc->clientRect.left = clientRect.left;
    winDesc->clientRect.right = clientRect.right;
    winDesc->clientRect.top = clientRect.top;
    winDesc->clientRect.bottom = clientRect.bottom;

    winDesc->mWndX = clientRect.left;
    winDesc->mWndY = clientRect.top;
    winDesc->mWndW = getRectWidth(&winDesc->clientRect);
    winDesc->mWndH = getRectHeight(&winDesc->clientRect);
}

// Fetches the window rectangle from the system and assigns it to WindowDesc
static void ResetWindowDescWindowedRect(WindowDesc* winDesc)
{
    RECT windowedRect;
    HWND hwnd = (HWND)winDesc->handle.window;

    GetWindowRect(hwnd, &windowedRect);

    if (windowedRect.left != winDesc->windowedRect.left || windowedRect.right != winDesc->windowedRect.right ||
        windowedRect.top != winDesc->windowedRect.top || windowedRect.bottom != winDesc->windowedRect.bottom)
    {
        winDesc->windowedRect.left = windowedRect.left;
        winDesc->windowedRect.right = windowedRect.right;
        winDesc->windowedRect.top = windowedRect.top;
        winDesc->windowedRect.bottom = windowedRect.bottom;

        ResetWindowDescClientRect(winDesc);
    }
}

// Sets the size of the client rectangle and all fields associated with it
// Used when processing resize messages
static void UpdateWindowDescClientSize(WindowDesc* winDesc, int32_t clientWidth, int32_t clientHeight)
{
    winDesc->clientRect.right = winDesc->clientRect.left + clientWidth;
    winDesc->clientRect.bottom = winDesc->clientRect.top + clientHeight;

    if (!winDesc->fullScreen)
    {
        WindowRectFromClient(&winDesc->windowedRect, &winDesc->clientRect, PrepareStyleMask(winDesc), FALSE);
    }

    winDesc->mWndW = clientWidth;
    winDesc->mWndH = clientHeight;
}

static void UpdateWindowDescClientPos(WindowDesc* winDesc, int32_t xPos, int32_t yPos)
{
    int32_t dx = xPos - winDesc->clientRect.left;
    int32_t dy = yPos - winDesc->clientRect.top;
    winDesc->clientRect.left += dx;
    winDesc->clientRect.top += dy;
    winDesc->clientRect.right += dx;
    winDesc->clientRect.bottom += dy;
    winDesc->mWndX = xPos;
    winDesc->mWndY = yPos;
}

// Sets the position of the client rectangle and all fields associated with it
// Used when processing movement messages
static void UpdateWindowDescWindowedPos(WindowDesc* winDesc, int32_t xPos, int32_t yPos)
{
    int32_t dx = xPos - winDesc->clientRect.left;
    int32_t dy = yPos - winDesc->clientRect.top;
    winDesc->windowedRect.left += dx;
    winDesc->windowedRect.top += dy;
    winDesc->windowedRect.right += dx;
    winDesc->windowedRect.bottom += dy;
    UpdateWindowDescClientPos(winDesc, xPos, yPos);
}

static DWORD PrepareStyleMask(WindowDesc* winDesc)
{
    DWORD windowStyle = WS_OVERLAPPEDWINDOW;
    // Note: we want to have WS_THICKFRAME here to enable resize.
    // However, for Windows 10 and 11 window without title is always resizable.
    // Additionally, when sending WS_THICKFRAME to AdjustWindowRect or AdjustWindowRectEx
    // these functions calculate the size as if the window has a title bar.
    // Which results in a wrong window size.
    // Take this note into account in case a need of refactoring arises.
    if (winDesc->borderlessWindow)
    {
        windowStyle = WS_POPUP;
    }

    if (winDesc->noresizeFrame)
    {
        windowStyle ^= WS_THICKFRAME | WS_MAXIMIZEBOX;
    }

    if (!winDesc->hide)
    {
        windowStyle |= WS_VISIBLE;
    }

    return windowStyle;
}

static void OffsetRectToDisplay(WindowDesc* winDesc, LPRECT rect)
{
    int32_t displayOffsetX = winDesc->fullscreenRect.left;
    int32_t displayOffsetY = winDesc->fullscreenRect.top;

    // Adjust for display coordinates in absolute virtual
    // display space.
    rect->left += (LONG)displayOffsetX;
    rect->top += (LONG)displayOffsetY;
    rect->right += (LONG)displayOffsetX;
    rect->bottom += (LONG)displayOffsetY;
}

static void onResize(WindowDesc* wnd, int32_t newSizeX, int32_t newSizeY)
{
    if (pWindowAppRef == nullptr || !pWindowAppRef->mSettings.mInitialized)
    {
        return;
    }

    pWindowAppRef->mSettings.mFullScreen = wnd->fullScreen;
    if (pWindowAppRef->mSettings.mWidth == newSizeX && pWindowAppRef->mSettings.mHeight == newSizeY)
    {
        return;
    }

    pWindowAppRef->mSettings.mWidth = newSizeX;
    pWindowAppRef->mSettings.mHeight = newSizeY;

    ReloadDesc reloadDesc;
    reloadDesc.mType = RELOAD_TYPE_RESIZE;
    requestReload(&reloadDesc);
}

// Sets index for current resolution of the monitor
static void updateMonitorResolution(MonitorDesc* pMonitor, uint32_t newWidth, uint32_t newHeight)
{
    pMonitor->currentResolution = UINT32_MAX;
    for (size_t i = 0; i < arrlenu(pMonitor->resolutions); ++i)
    {
        if (newWidth == pMonitor->resolutions[i].mWidth && newHeight == pMonitor->resolutions[i].mHeight)
        {
            pMonitor->currentResolution = (uint32_t)i;
            break;
        }
    }
    ASSERT(pMonitor->currentResolution != UINT32_MAX);
}

void adjustWindow(WindowDesc* winDesc, bool isDisplayChange)
{
    HWND hwnd = (HWND)winDesc->handle.window;

    if (isDisplayChange)
    {
        // Note: On windows 10 cursor position becomes invalid
        // until the window is moved or resized.
        // Minimizing the window before resizing fixes the issue
        ShowWindow(hwnd, SW_MINIMIZE);
    }

    if (winDesc->fullScreen)
    {
        // Make the window borderless so that the client area can fill the screen.
        SetWindowLong(hwnd, GWL_STYLE, WS_SYSMENU | WS_POPUP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VISIBLE);

        // Get the settings of the durrent display index. We want the app to go into
        // fullscreen mode on the display that supports Independent Flip.
        HMONITOR      currentMonitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY);
        MONITORINFOEX info;
        info.cbSize = sizeof(MONITORINFOEX);
        bool infoRead = GetMonitorInfo(currentMonitor, &info);
        ASSERT(infoRead);

        pWindowAppRef->mSettings.mWindowX = info.rcMonitor.left;
        pWindowAppRef->mSettings.mWindowY = info.rcMonitor.top;

        SetWindowPos(hwnd, HWND_NOTOPMOST, info.rcMonitor.left, info.rcMonitor.top, info.rcMonitor.right - info.rcMonitor.left,
                     info.rcMonitor.bottom - info.rcMonitor.top, SWP_FRAMECHANGED | SWP_NOACTIVATE);

        ShowWindow(hwnd, SW_MAXIMIZE);
    }
    else
    {
        DWORD windowStyle = PrepareStyleMask(winDesc);
        SetWindowLong(hwnd, GWL_STYLE, windowStyle);

        if (!isDisplayChange)
        {
            SetWindowPos(hwnd, HWND_NOTOPMOST, winDesc->windowedRect.left, winDesc->windowedRect.top, getRectWidth(&winDesc->windowedRect),
                         getRectHeight(&winDesc->windowedRect), SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
        }

        if (winDesc->maximized)
        {
            ShowWindow(hwnd, SW_MAXIMIZE);
        }
        else
        {
            ShowWindow(hwnd, SW_NORMAL);
        }
    }
}

static BOOL CALLBACK monitorCallback(HMONITOR pMonitor, HDC pDeviceContext, LPRECT pRect, LPARAM pParam)
{
    UNREF_PARAM(pDeviceContext);
    UNREF_PARAM(pRect);
    MONITORINFOEXW info;
    info.cbSize = sizeof(info);
    GetMonitorInfoW(pMonitor, &info);
    MonitorInfo* data = (MonitorInfo*)pParam;
    unsigned     index = data->index;

    if (wcscmp(info.szDevice, data->adapterName) == 0)
    {
        gMonitors[index].monitorRect = { (int)info.rcMonitor.left, (int)info.rcMonitor.top, (int)info.rcMonitor.right,
                                         (int)info.rcMonitor.bottom };
        gMonitors[index].workRect = { (int)info.rcWork.left, (int)info.rcWork.top, (int)info.rcWork.right, (int)info.rcWork.bottom };
    }
    return TRUE;
}

// @Konstantin: This needs to be rethought, because this should happen
// very early. For Hades this happens too late (after init) which is
// unacceptable and causes issues.
void collectMonitorInfo()
{
    if (gMonitors != nullptr)
    {
        return;
    }

    DISPLAY_DEVICEW adapter;
    adapter.cb = sizeof(adapter);

    int      found = 0;
    uint32_t monitorCount = 0;

    for (int adapterIndex = 0;; ++adapterIndex)
    {
        if (!EnumDisplayDevicesW(NULL, adapterIndex, &adapter, 0))
            break;

        if (!(adapter.StateFlags & DISPLAY_DEVICE_ACTIVE))
            continue;

        for (int displayIndex = 0;; displayIndex++)
        {
            DISPLAY_DEVICEW display;
            display.cb = sizeof(display);

            if (!EnumDisplayDevicesW(adapter.DeviceName, displayIndex, &display, 0))
                break;

            ++monitorCount;
        }
    }

    if (monitorCount)
    {
        gMonitorCount = monitorCount;
        gMonitors = (MonitorDesc*)tf_calloc(monitorCount, sizeof(MonitorDesc));
        for (int adapterIndex = 0;; ++adapterIndex)
        {
            if (!EnumDisplayDevicesW(NULL, adapterIndex, &adapter, 0))
                break;

            if (!(adapter.StateFlags & DISPLAY_DEVICE_ACTIVE))
                continue;

            for (int displayIndex = 0;; displayIndex++)
            {
                DISPLAY_DEVICEW display;
                HDC             dc;

                display.cb = sizeof(display);

                if (!EnumDisplayDevicesW(adapter.DeviceName, displayIndex, &display, 0))
                    break;

                dc = CreateDCW(L"DISPLAY", adapter.DeviceName, NULL, NULL);

                MonitorDesc desc;
                desc.modesPruned = (adapter.StateFlags & DISPLAY_DEVICE_MODESPRUNED) != 0;

                wcsncpy_s(desc.adapterName, adapter.DeviceName, elementsOf(adapter.DeviceName));
                wcsncpy_s(desc.publicAdapterName, adapter.DeviceString, elementsOf(adapter.DeviceString));
                wcsncpy_s(desc.displayName, display.DeviceName, elementsOf(display.DeviceName));
                wcsncpy_s(desc.publicDisplayName, display.DeviceString, elementsOf(display.DeviceString));

                desc.physicalSize[0] = GetDeviceCaps(dc, HORZSIZE);
                desc.physicalSize[1] = GetDeviceCaps(dc, VERTSIZE);

                desc.dpi[0] = static_cast<UINT>(::GetDeviceCaps(dc, LOGPIXELSX));
                desc.dpi[1] = static_cast<UINT>(::GetDeviceCaps(dc, LOGPIXELSY));

                gMonitors[found] = (desc);
                MonitorInfo data = {};
                data.index = found;
                wcsncpy_s(data.adapterName, adapter.DeviceName, elementsOf(adapter.DeviceName));

                EnumDisplayMonitors(NULL, NULL, monitorCallback, (LPARAM)(&data));

                DeleteDC(dc);

                if ((adapter.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE) && displayIndex == 0)
                {
                    MonitorDesc mDesc = gMonitors[0];
                    gMonitors[0] = gMonitors[found];
                    gMonitors[found] = mDesc;
                }

                found++;
            }
        }
    }
    else
    {
        LOGF(LogLevel::eDEBUG, "FallBack Option");
        // Fallback options incase enumeration fails
        // then default to the primary device
        monitorCount = 0;
        HMONITOR currentMonitor = NULL;
        currentMonitor = MonitorFromWindow(NULL, MONITOR_DEFAULTTOPRIMARY);
        if (currentMonitor)
        {
            monitorCount = 1;
            gMonitors = (MonitorDesc*)tf_calloc(monitorCount, sizeof(MonitorDesc));

            MONITORINFOEXW info;
            info.cbSize = sizeof(MONITORINFOEXW);
            bool infoRead = GetMonitorInfoW(currentMonitor, &info);
            ASSERT(infoRead);
            MonitorDesc desc = {};

            wcsncpy_s(desc.adapterName, info.szDevice, elementsOf(info.szDevice));
            wcsncpy_s(desc.publicAdapterName, info.szDevice, elementsOf(info.szDevice));
            wcsncpy_s(desc.displayName, info.szDevice, elementsOf(info.szDevice));
            wcsncpy_s(desc.publicDisplayName, info.szDevice, elementsOf(info.szDevice));
            desc.monitorRect = { (int)info.rcMonitor.left, (int)info.rcMonitor.top, (int)info.rcMonitor.right, (int)info.rcMonitor.bottom };
            desc.workRect = { (int)info.rcWork.left, (int)info.rcWork.top, (int)info.rcWork.right, (int)info.rcWork.bottom };
            gMonitors[0] = (desc);
            gMonitorCount = monitorCount;
        }
    }

    for (uint32_t monitor = 0; monitor < monitorCount; ++monitor)
    {
        MonitorDesc* pMonitor = &gMonitors[monitor];
        DEVMODEW     devMode = {};
        devMode.dmSize = sizeof(DEVMODEW);
        devMode.dmFields = DM_PELSHEIGHT | DM_PELSWIDTH;

        EnumDisplaySettingsW(pMonitor->adapterName, ENUM_CURRENT_SETTINGS, &devMode);
        pMonitor->defaultResolution.mHeight = devMode.dmPelsHeight;
        pMonitor->defaultResolution.mWidth = devMode.dmPelsWidth;

        pMonitor->dpi[0] = (uint32_t)((pMonitor->defaultResolution.mWidth * 25.4) / pMonitor->physicalSize[0]);
        pMonitor->dpi[1] = (uint32_t)((pMonitor->defaultResolution.mHeight * 25.4) / pMonitor->physicalSize[1]);

        Resolution* resolutions = NULL;
        DWORD       current = 0;

        arrsetcap(resolutions, 8);

        while (EnumDisplaySettingsW(pMonitor->adapterName, current++, &devMode))
        {
            bool duplicate = false;
            for (ptrdiff_t i = 0; i < arrlen(resolutions); ++i)
            {
                if (resolutions[i].mWidth == (uint32_t)devMode.dmPelsWidth && resolutions[i].mHeight == (uint32_t)devMode.dmPelsHeight)
                {
                    duplicate = true;
                    break;
                }
            }

            if (duplicate)
                continue;

            Resolution videoMode = {};
            videoMode.mHeight = devMode.dmPelsHeight;
            videoMode.mWidth = devMode.dmPelsWidth;
            arrpush(resolutions, videoMode);
        }
        sort(
            resolutions, arrlenu(resolutions), sizeof(Resolution),
            +[](const void* lhs, const void* rhs, void* pUser)
            {
                UNREF_PARAM(pUser);
                Resolution* pLhs = (Resolution*)lhs;
                Resolution* pRhs = (Resolution*)rhs;
                if (pLhs->mHeight == pRhs->mHeight)
                    return pLhs->mWidth < pRhs->mWidth;

                return pLhs->mHeight < pRhs->mHeight;
            },
            NULL);

        pMonitor->resolutions = resolutions;
        updateMonitorResolution(pMonitor, pMonitor->defaultResolution.mWidth, pMonitor->defaultResolution.mHeight);
    }
}

static void onFocusChanged(bool focused)
{
    if (pWindowAppRef == nullptr || !pWindowAppRef->mSettings.mInitialized)
    {
        return;
    }

    pWindowAppRef->mSettings.mFocused = focused;
}

// Hit test the frame for resizing and moving.
static LRESULT HitTestNCA(HWND hWnd, WPARAM /*wParam*/, LPARAM lParam)
{
    // Get the point coordinates for the hit test.
    POINT ptMouse = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };

    // Get the window rectangle.
    RECT rcWindow;
    GetWindowRect(hWnd, &rcWindow);

    // Get the frame rectangle, adjusted for the style without a caption.
    RECT rcFrame = { 0 };
    AdjustWindowRectEx(&rcFrame, WS_OVERLAPPEDWINDOW & ~WS_CAPTION, FALSE, NULL);

    // Determine if the hit test is for resizing. Default middle (1,1).
    USHORT uRow = 1;
    USHORT uCol = 1;
    bool   fOnResizeBorder = false;

    if (::IsZoomed(hWnd)) // if maximized, only the frame title remains
    {
        // Determine if the point is at the top or bottom of the window.
        if ((ptMouse.y >= 0) && (ptMouse.y < TOPEXTENDWIDTH))
        {
            uRow = 0;
        }
    }
    else
    {
        // Determine if the point is at the top or bottom of the window.
        if (ptMouse.y >= rcWindow.top && ptMouse.y < rcWindow.top + TOPEXTENDWIDTH)
        {
            fOnResizeBorder = (ptMouse.y < (rcWindow.top - rcFrame.top));
            uRow = 0;
        }
        else if (ptMouse.y < rcWindow.bottom && ptMouse.y >= rcWindow.bottom - BOTTOMEXTENDWIDTH)
        {
            uRow = 2;
        }

        // Determine if the point is at the left or right of the window.
        if (ptMouse.x >= rcWindow.left && ptMouse.x < rcWindow.left + LEFTEXTENDWIDTH)
        {
            uCol = 0; // left side
        }
        else if (ptMouse.x < rcWindow.right && ptMouse.x >= rcWindow.right - RIGHTEXTENDWIDTH)
        {
            uCol = 2; // right side
        }
    }

    // Hit test (HTTOPLEFT, ... HTBOTTOMRIGHT)
    LRESULT hitTests[3][3] = {
        { fOnResizeBorder ? HTTOPLEFT : HTLEFT, fOnResizeBorder ? HTTOP : HTCAPTION, fOnResizeBorder ? HTTOPRIGHT : HTRIGHT },
        { HTLEFT, HTNOWHERE, HTRIGHT },
        { HTBOTTOMLEFT, HTBOTTOM, HTBOTTOMRIGHT },
    };

    return hitTests[uRow][uCol];
}

//------------------------------------------------------------------------
// WINDOW HANDLING INTERFACE FUNCTIONS
//------------------------------------------------------------------------

void openWindow(const char* app_name, WindowDesc* winDesc)
{
    ResetWindowDescFullScreenRect(winDesc);

    // Defer borderless window setting
    bool borderless = winDesc->borderlessWindow;
    winDesc->borderlessWindow = false;

    // Adjust windowed rect for windowed mode rendering.
    RectDesc rect = winDesc->clientRect;
    DWORD    windowStyle = PrepareStyleMask(winDesc);

    WindowRectFromClient(&rect, &winDesc->clientRect, windowStyle, FALSE);

    // Due to windows style and border, the rect can end up with negative values
    const int intMinPlusOne = INT_MIN + 1;
    if (rect.left < 0)
    {
        if (rect.left == INT_MIN)
            rect.left = intMinPlusOne;

        const int32_t offset = abs(rect.left);
        rect.left += offset;
        rect.right += offset;
        winDesc->clientRect.left += offset;
        winDesc->clientRect.right += offset;
    }

    if (rect.top < 0)
    {
        if (rect.top == INT_MIN)
            rect.top = intMinPlusOne;

        const int32_t offset = abs(rect.top);
        rect.top += offset;
        rect.bottom += offset;
        winDesc->clientRect.top += offset;
        winDesc->clientRect.bottom += offset;
    }

    WCHAR  app[FS_MAX_PATH] = {};
    size_t charConverted = 0;
    mbstowcs_s(&charConverted, app, app_name, FS_MAX_PATH);

    int windowY = rect.top;
    int windowX = rect.left;

    if (!winDesc->overrideDefaultPosition)
    {
        windowX = windowY = CW_USEDEFAULT;
    }

    // Defer fullscreen. We always create in windowed, and
    // switch to fullscreen after creation.
    bool fullscreen = winDesc->fullScreen;
    winDesc->fullScreen = false;

    HWND hwnd = CreateWindowW(FORGE_WINDOW_CLASS, app, windowStyle, windowX, windowY, rect.right - windowX, rect.bottom - windowY, NULL,
                              NULL, (HINSTANCE)GetModuleHandle(NULL), 0);

    if (hwnd != NULL)
    {
        winDesc->handle.type = WINDOW_HANDLE_TYPE_WIN32;
        winDesc->handle.window = hwnd;

        if (!winDesc->hide)
        {
            if (winDesc->maximized)
            {
                ShowWindow(hwnd, SW_MAXIMIZE);
            }
            else if (winDesc->minimized)
            {
                ShowWindow(hwnd, SW_MINIMIZE);
            }

            if (borderless)
            {
                setBorderless(winDesc);
            }

            if (fullscreen)
            {
                setFullscreen(winDesc);
            }
        }

        LOGF(LogLevel::eINFO, "Created window app %s", app_name);
    }
    else
    {
        LOGF(LogLevel::eERROR, "Failed to create window app %s", app_name);
    }

    setMousePositionRelative(winDesc, getRectWidth(&winDesc->windowedRect) >> 1, getRectHeight(&winDesc->windowedRect) >> 1);
}

bool handleMessages()
{
    MSG msg;
    msg.message = NULL;
    bool quit = false;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);

        if (WM_CLOSE == msg.message || WM_QUIT == msg.message)
            quit = true;
    }

    return quit;
}

void closeWindow(WindowDesc* winDesc)
{
    DestroyWindow((HWND)winDesc->handle.window);
    winDesc->handle = {};
    handleMessages();
}

void setWindowRect(WindowDesc* winDesc, const RectDesc* rect)
{
    ASSERT(winDesc);
    ASSERT(rect);
    if (winDesc->fullScreen)
    {
        return;
    }

    HWND hwnd = (HWND)winDesc->handle.window;

    DWORD windowStyle = PrepareStyleMask(winDesc);
    SetWindowLong(hwnd, GWL_STYLE, windowStyle);

    winDesc->windowedRect = *rect;

    SetWindowPos(hwnd, HWND_NOTOPMOST, winDesc->windowedRect.left, winDesc->windowedRect.top,
                 winDesc->windowedRect.right - winDesc->windowedRect.left, winDesc->windowedRect.bottom - winDesc->windowedRect.top,
                 SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
}

void setWindowSize(WindowDesc* winDesc, unsigned width, unsigned height)
{
    RectDesc newWindowRect;
    newWindowRect.left = winDesc->windowedRect.left;
    newWindowRect.top = winDesc->windowedRect.top;
    newWindowRect.right = newWindowRect.left + (int32_t)width;
    newWindowRect.bottom = newWindowRect.top + (int32_t)height;

    setWindowRect(winDesc, &newWindowRect);
}

void setWindowClientRect(WindowDesc* winDesc, const RectDesc* rect)
{
    ASSERT(winDesc);
    ASSERT(rect);

    if (winDesc->fullScreen)
    {
        return;
    }

    RectDesc windowRect = {};
    WindowRectFromClient(&windowRect, rect, PrepareStyleMask(winDesc), FALSE);

    return setWindowRect(winDesc, &windowRect);
}
FORGE_API void setWindowClientSize(WindowDesc* winDesc, unsigned width, unsigned height)
{
    RectDesc newWindowRect = {};
    newWindowRect.left = winDesc->clientRect.left;
    newWindowRect.top = winDesc->clientRect.top;
    newWindowRect.right = newWindowRect.left + (int32_t)width;
    newWindowRect.bottom = newWindowRect.top + (int32_t)height;

    setWindowClientRect(winDesc, &newWindowRect);
}

static void ToggleBorderless(WindowDesc* winDesc)
{
    if (!winDesc->fullScreen)
    {
        winDesc->borderlessWindow = !winDesc->borderlessWindow;
        winDesc->mWindowMode = winDesc->borderlessWindow ? WM_BORDERLESS : WM_WINDOWED;

        setWindowClientSize(winDesc, getRectWidth(&winDesc->clientRect), getRectHeight(&winDesc->clientRect));
    }
}

void ToggleFullscreen(WindowDesc* winDesc)
{
    winDesc->fullScreen = !winDesc->fullScreen;
    adjustWindow(winDesc, false);
    if (winDesc->fullScreen)
    {
        winDesc->mWindowMode = WM_FULLSCREEN;
    }
    else
    {
        winDesc->mWindowMode = winDesc->borderlessWindow ? WM_BORDERLESS : WM_WINDOWED;
    }
}

void setWindowed(WindowDesc* winDesc)
{
    winDesc->maximized = false;

    if (winDesc->fullScreen)
    {
        ToggleFullscreen(winDesc);
    }
    if (winDesc->borderlessWindow)
    {
        ToggleBorderless(winDesc);
    }
    winDesc->mWindowMode = WindowMode::WM_WINDOWED;
}

void setBorderless(WindowDesc* winDesc)
{
    if (winDesc->fullScreen)
    {
        ToggleFullscreen(winDesc);
    }
    if (!winDesc->borderlessWindow)
    {
        ToggleBorderless(winDesc);
    }
    winDesc->mWindowMode = WindowMode::WM_BORDERLESS;
}

void toggleFullscreen(WindowDesc* pWindow)
{
    if (pWindow->fullScreen)
    {
        pWindow->borderlessWindow ? setBorderless(pWindow) : setWindowed(pWindow);
    }
    else
    {
        setFullscreen(pWindow);
    }
}

void setFullscreen(WindowDesc* winDesc)
{
    if (!winDesc->fullScreen)
    {
        ToggleFullscreen(winDesc);
        winDesc->mWindowMode = WindowMode::WM_FULLSCREEN;
    }
}

void showWindow(WindowDesc* winDesc)
{
    winDesc->hide = false;
    ShowWindow((HWND)winDesc->handle.window, SW_SHOW);
}

void hideWindow(WindowDesc* winDesc)
{
    winDesc->hide = true;
    ShowWindow((HWND)winDesc->handle.window, SW_HIDE);
}

void maximizeWindow(WindowDesc* winDesc)
{
    winDesc->maximized = true;
    ShowWindow((HWND)winDesc->handle.window, SW_MAXIMIZE);
}

void minimizeWindow(WindowDesc* winDesc)
{
    winDesc->maximized = false;
    ShowWindow((HWND)winDesc->handle.window, SW_MINIMIZE);
}

RECT getCenteredWindowRect(WindowDesc* winDesc)
{
    uint32_t fsHalfWidth = getRectWidth(&winDesc->fullscreenRect) >> 1;
    uint32_t fsHalfHeight = getRectHeight(&winDesc->fullscreenRect) >> 1;
    uint32_t windowWidth = getRectWidth(&winDesc->clientRect);
    uint32_t windowHeight = getRectHeight(&winDesc->clientRect);
    uint32_t windowHalfWidth = windowWidth >> 1;
    uint32_t windowHalfHeight = windowHeight >> 1;

    uint32_t X = fsHalfWidth - windowHalfWidth;
    uint32_t Y = fsHalfHeight - windowHalfHeight;

    RECT rect = { (LONG)(X), (LONG)(Y), (LONG)(X + windowWidth), (LONG)(Y + windowHeight) };
    return rect;
}

void centerWindow(WindowDesc* winDesc)
{
    RECT rect = getCenteredWindowRect(winDesc);

    DWORD windowStyle = PrepareStyleMask(winDesc);

    AdjustWindowRect(&rect, windowStyle, FALSE);

    OffsetRectToDisplay(winDesc, &rect);

    SetWindowPos((HWND)winDesc->handle.window, HWND_NOTOPMOST, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
                 SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOOWNERZORDER);

    winDesc->windowedRect = { (int32_t)rect.left, (int32_t)rect.top, (int32_t)rect.right, (int32_t)rect.bottom };
}

//------------------------------------------------------------------------
// CURSOR AND MOUSE HANDLING INTERFACE FUNCTIONS
//------------------------------------------------------------------------

void* createCursor(const char* path) { return LoadCursorFromFileA(path); }

void setCursor(void* cursor)
{
    HCURSOR windowsCursor = (HCURSOR)cursor;
    SetCursor(windowsCursor);
}

void showCursor()
{
    if (!gCursorVisible)
    {
        ShowCursor(TRUE);
        gCursorVisible = true;
    }
}

void hideCursor()
{
    if (gCursorVisible)
    {
        ShowCursor(FALSE);
        gCursorVisible = false;
    }
}

void captureCursor(WindowDesc* winDesc, bool bEnable)
{
    ASSERT(winDesc);

    static int32_t lastCursorPosX = 0;
    static int32_t lastCursorPosY = 0;

    if (winDesc->cursorCaptured != bEnable)
    {
        if (bEnable)
        {
            POINT lastCursorPoint;
            GetCursorPos(&lastCursorPoint);
            lastCursorPosX = lastCursorPoint.x;
            lastCursorPosY = lastCursorPoint.y;

            HWND handle = (HWND)winDesc->handle.window;
            SetCapture(handle);

            RECT clientRect;
            GetClientRectWithOffset(handle, &clientRect);
            // convert screen rect to client coordinates.
            POINT ptClientUL = { clientRect.left, clientRect.top };
            // Add one to the right and bottom sides, because the
            // coordinates retrieved by GetClientRect do not
            // include the far left and lowermost pixels.
            POINT ptClientLR = { clientRect.right + 1, clientRect.bottom + 1 };
            ClientToScreen(handle, &ptClientUL);
            ClientToScreen(handle, &ptClientLR);

            // Copy the client coordinates of the client area
            // to the rcClient structure. Confine the mouse cursor
            // to the client area by passing the rcClient structure
            // to the ClipCursor function.
            SetRect(&clientRect, ptClientUL.x, ptClientUL.y, ptClientLR.x, ptClientLR.y);
            ClipCursor(&clientRect);
            ShowCursor(FALSE);
        }
        else
        {
            ClipCursor(NULL);
            ShowCursor(TRUE);
            ReleaseCapture();
            SetCursorPos(lastCursorPosX, lastCursorPosY);
        }

        winDesc->cursorCaptured = bEnable;
    }
}

bool isCursorInsideTrackingArea() { return gCursorInsideRectangle; }

void setMousePositionRelative(const WindowDesc* winDesc, int32_t x, int32_t y)
{
    POINT point = { (LONG)x, (LONG)y };
    ClientToScreen((HWND)winDesc->handle.window, &point);

    SetCursorPos(point.x, point.y);
}

void setMousePositionAbsolute(int32_t x, int32_t y) { SetCursorPos(x, y); }

//------------------------------------------------------------------------
// MONITOR AND RESOLUTION HANDLING INTERFACE FUNCTIONS
//------------------------------------------------------------------------

void getRecommendedWindowRect(WindowDesc* winDesc, RectDesc* rect)
{
    getRecommendedResolution(rect);

    if (winDesc->borderlessWindow == false && winDesc->fullScreen == false)
    {
        RECT winRect;
        winRect.top = rect->top;
        winRect.bottom = rect->bottom;
        winRect.left = rect->left;
        winRect.right = rect->right;

        DWORD windowStyle = PrepareStyleMask(winDesc);
        AdjustWindowRect(&winRect, windowStyle, FALSE);

        if (winRect.top < 0)
        {
            winRect.bottom -= winRect.top;
            winRect.top -= winRect.top;
        }
        if (winRect.left < 0)
        {
            winRect.right -= winRect.left;
            winRect.left -= winRect.left;
        }

        rect->top = winRect.top;
        rect->bottom = winRect.bottom;
        rect->left = winRect.left;
        rect->right = winRect.right;
    }
}

void setResolution(const MonitorDesc* pMonitor, const Resolution* pMode)
{
    DEVMODEW devMode = {};
    devMode.dmSize = sizeof(DEVMODEW);
    devMode.dmPelsHeight = pMode->mHeight;
    devMode.dmPelsWidth = pMode->mWidth;
    devMode.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT;

    ChangeDisplaySettingsExW(pMonitor->adapterName, &devMode, NULL, CDS_FULLSCREEN, NULL);
}

void getDpiScale(float array[2])
{
    HDC hdc = ::GetDC(NULL);
    array[0] = 0.f;
    array[1] = 0.f;
    const float dpi = 96.0f;
    if (hdc)
    {
        array[0] = (UINT)(::GetDeviceCaps(hdc, LOGPIXELSX)) / dpi;
        array[1] = static_cast<UINT>(::GetDeviceCaps(hdc, LOGPIXELSY)) / dpi;
        ::ReleaseDC(NULL, hdc);
    }
    else
    {
        typedef UINT (*GetDpiForSystemFn)();
        static const GetDpiForSystemFn getDpiForSystem =
            (GetDpiForSystemFn)GetProcAddress(GetModuleHandle(TEXT("User32.dll")), "GetDpiForSystem");

        float dpiScale;
        if (getDpiForSystem)
            dpiScale = getDpiForSystem() / 96.0f;
        else
            dpiScale = 1.0f;

        array[0] = dpiScale;
        array[1] = dpiScale;
    }
}

void getMonitorDpiScale(uint32_t monitorIndex, float dpiScale[2])
{
    MonitorDesc* monitor = getMonitor(monitorIndex);
    RECT         rect = { monitor->monitorRect.left, monitor->monitorRect.top, monitor->monitorRect.right, monitor->monitorRect.bottom };

    HMONITOR currentMonitor = MonitorFromRect(&rect, MONITOR_DEFAULTTONEAREST);

    typedef HRESULT (*GetDpiForMonitorFn)(HMONITOR hmonitor, int32_t dpiType, uint32_t * dpiX, uint32_t * dpiY);
    static const GetDpiForMonitorFn getDpiForMonitor =
        (GetDpiForMonitorFn)GetProcAddress(GetModuleHandle(TEXT("shcore.dll")), "GetDpiForMonitor");

    if (currentMonitor && getDpiForMonitor)
    {
        uint32_t dpiX = 0;
        uint32_t dpiY = 0;
        if (SUCCEEDED(getDpiForMonitor(currentMonitor, 0, &dpiX, &dpiY)))
        {
            dpiScale[0] = (float)dpiX / 96.0f;
            dpiScale[1] = (float)dpiY / 96.0f;
        }
    }
    else
    {
        getDpiScale(dpiScale);
    }

    LOGF(LogLevel::eDEBUG, "Monitor index [%u] DPI scale = (%.2f, %.2f)", monitorIndex, dpiScale[0], dpiScale[1]);
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
    ASSERT(gMonitorCount > index);
    return &gMonitors[index];
}

uint32_t getMonitorCount() { return gMonitorCount; }

uint32_t getActiveMonitorIdx()
{
    HMONITOR       currentMonitor = MonitorFromWindow((HWND)gWindow->handle.window, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFOEXW info = {};
    info.cbSize = sizeof(info);
    GetMonitorInfoW(currentMonitor, &info);

    for (uint32_t i = 0; i < gMonitorCount; ++i)
    {
        if (wcscmp(info.szDevice, gMonitors[i].adapterName) == 0)
            return i;
    }

    return 0;
}

//------------------------------------------------------------------------
// MONITOR AND RESOLUTION HANDLING INTERFACE FUNCTIONS
//------------------------------------------------------------------------

// Window event handler - Use as less as possible
LRESULT CALLBACK WinProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (gWindow == nullptr)
    {
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
    if (gWindow->borderlessWindow)
    {
        switch (message)
        {
        case WM_NCHITTEST:
        {
            auto lr = HitTestNCA(hwnd, wParam, lParam);
            if (lr == HTNOWHERE)
                return DefWindowProcW(hwnd, message, wParam, lParam);

            return lr;
        }
        case WM_NCCALCSIZE:
        {
            return 0;
        }
        }
    } // if (gWindow->borderlessWindow)

    bool maximized = false;
    switch (message)
    {
    case WM_NCPAINT:
    case WM_WINDOWPOSCHANGING:
    case WM_WINDOWPOSCHANGED:
    case WM_STYLECHANGED:
    {
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
    case WM_DISPLAYCHANGE:
    {
        updateMonitorResolution(getMonitor(getActiveMonitorIdx()), (uint32_t)LOWORD(lParam), (uint32_t)HIWORD(lParam));
        adjustWindow(gWindow, true);
        break;
    }
    case WM_GETMINMAXINFO:
    {
        LPMINMAXINFO lpMMI = (LPMINMAXINFO)lParam;

        // Prevent window from collapsing
        if (!gWindow->fullScreen)
        {
            LONG zoomOffset = 128;
            lpMMI->ptMinTrackSize.x = zoomOffset;
            lpMMI->ptMinTrackSize.y = zoomOffset;
        }
        break;
    }
    case WM_ERASEBKGND:
    {
        // Make sure to keep consistent background color when resizing.
        HDC    hdc = (HDC)wParam;
        RECT   rc;
        HBRUSH hbrWhite = CreateSolidBrush(0x00000000);
        GetClientRectWithOffset(hwnd, &rc);
        FillRect(hdc, &rc, hbrWhite);
        break;
    }
    case WM_MOVE:
    {
        ResetWindowDescFullScreenRect(gWindow);
        // Update the position of window
        // Note: Avoid resetting window or client size while processing WM_SIZE.
        // It can result in incorrect rectangle size returned by the OS.
        if (!gWindow->fullScreen)
            UpdateWindowDescWindowedPos(gWindow, (int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam));
        else
            UpdateWindowDescClientPos(gWindow, (int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam));
        break;
    }
    case WM_STYLECHANGING:
    {
        break;
    }
    case WM_SIZE:
    {
        // Update the position of window
        // Note: Avoid resetting window or client size while processing WM_SIZE.
        // It can result in incorrect rectangle size returned by the OS.
        UINT width = LOWORD(lParam);
        UINT height = HIWORD(lParam);
        switch (wParam)
        {
        case SIZE_MAXIMIZED:
            maximized = true;
            // fall through
        case SIZE_RESTORED:
            onFocusChanged(true);
            gWindow->maximized = maximized;
            gWindow->minimized = false;
            UpdateWindowDescClientSize(gWindow, width, height);
            onResize(gWindow, getRectWidth(&gWindow->clientRect), getRectHeight(&gWindow->clientRect));
            break;
        case SIZE_MINIMIZED:
            onFocusChanged(false);
            gWindow->minimized = true;
            break;
        }
        break;
    }
    case WM_SETFOCUS:
    {
        onFocusChanged(true);
        break;
    }
    case WM_KILLFOCUS:
    {
        onFocusChanged(false);
        break;
    }
    case WM_ENTERSIZEMOVE:
    {
        gWindowIsResizing = true;
        break;
    }
    case WM_EXITSIZEMOVE:
    {
        onFocusChanged(true);
        gWindowIsResizing = false;
        if (!gWindow->fullScreen)
        {
            gWindow->maximized = false;
            ResetWindowDescWindowedRect(gWindow);
        }
        onResize(gWindow, getRectWidth(&gWindow->clientRect), getRectHeight(&gWindow->clientRect));
        break;
    }
    case WM_SETCURSOR:
    {
        if (LOWORD(lParam) == HTCLIENT)
        {
            if (!gCursorInsideRectangle)
            {
                HCURSOR cursor = LoadCursor(NULL, IDC_ARROW);
                SetCursor(cursor);

                gCursorInsideRectangle = true;
            }
        }
        else
        {
            gCursorInsideRectangle = false;
            return DefWindowProcW(hwnd, message, wParam, lParam);
        }
        break;
    }
    case WM_DESTROY:
    case WM_CLOSE:
    {
        PostQuitMessage(0);
        break;
    }
    case WM_GETTEXT:
    {
        break;
    }
    default:
    {
        MSG msg = {};
        msg.hwnd = hwnd;
        msg.lParam = lParam;
        msg.message = message;
        msg.wParam = wParam;
        extern void platformInputEvent(const MSG* msg);
        platformInputEvent(&msg);

        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
    }
    return 0;
}

bool initWindowSystem()
{
    if (!gWindowClassInitialized)
    {
        HINSTANCE instance = (HINSTANCE)GetModuleHandle(NULL);
        memset(&gWindowClass, 0, sizeof(gWindowClass));
        gWindowClass.style = 0;
        gWindowClass.lpfnWndProc = WinProc;
        gWindowClass.hInstance = instance;
        gWindowClass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
        gWindowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
        gWindowClass.lpszClassName = FORGE_WINDOW_CLASS;

        bool success = RegisterClassW(&gWindowClass) != 0;

        if (!success)
        {
            // Get the error message, if any.
            DWORD errorMessageID = ::GetLastError();

            if (errorMessageID != ERROR_CLASS_ALREADY_EXISTS)
            {
                LPSTR messageBuffer = NULL;
                FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
                               errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

                LOGF(eERROR, "%s", messageBuffer);
                LocalFree(messageBuffer);
                return false;
            }
            else
            {
                gWindowClassInitialized = false;
            }
        }
    }

    collectMonitorInfo();
    return true;
}

void exitWindowSystem()
{
    for (uint32_t i = 0; i < gMonitorCount; ++i)
        arrfree(gMonitors[i].resolutions);

    tf_free(gMonitors);
}
