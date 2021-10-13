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

#include "../Core/Config.h"

#ifdef _WINDOWS

#include <ctime>
#pragma comment(lib, "WinMM.lib")
#include <windowsx.h>
#include <ntverp.h>

#if !defined(XBOX)
#include <shlwapi.h>
#pragma comment(lib, "shlwapi.lib")
#endif

#include "../../ThirdParty/OpenSource/EASTL/vector.h"
#include "../../ThirdParty/OpenSource/EASTL/unordered_map.h"
#include "../../ThirdParty/OpenSource/rmem/inc/rmem.h"

#include "../Math/MathTypes.h"

#include "../Interfaces/IOperatingSystem.h"
#include "../Interfaces/ILog.h"
#include "../Interfaces/ITime.h"
#include "../Interfaces/IThread.h"
#include "../Interfaces/IProfiler.h"
#include "../Interfaces/IApp.h"
#include "../Interfaces/IFileSystem.h"
#include "../Interfaces/IScripting.h"
#include "../Interfaces/IFont.h"
#include "../Interfaces/IUI.h"
#include "../Interfaces/IMemory.h"

#include "../../Renderer/IRenderer.h"

#ifdef ENABLE_FORGE_STACKTRACE_DUMP
#include "WindowsStackTraceDump.h"
#endif

#define FORGE_WINDOW_CLASS L"The Forge"
#define MAX_KEYS 256

#define GETX(l) ((int)LOWORD(l))
#define GETY(l) ((int)HIWORD(l))

#define elementsOf(a) (sizeof(a) / sizeof((a)[0]))

static IApp*        pApp = nullptr;
static bool         gWindowClassInitialized = false;
static WNDCLASSW    gWindowClass;
static MonitorDesc* gMonitors = nullptr;
static uint32_t     gMonitorCount = 0;
static WindowsDesc* gWindow = nullptr;
static bool         gWindowIsResizing = false;
static bool         gCursorVisible = true;
static bool         gCursorInsideRectangle = false;

static uint8_t gResetScenario = RESET_SCENARIO_NONE;

/// UI
static UIComponent* pAPISwitchingWindow = NULL;
UIWidget* pSwitchWindowLabel = NULL;
UIWidget* pSelectApUIWidget = NULL; 

static uint32_t gSelectedApiIndex = 0; 
extern RendererApi gSelectedRendererApi; // Renderer.cpp
extern bool gD3D11Unsupported; // Renderer.cpp

static void adjustWindow(WindowsDesc* winDesc);
static void onResize(WindowsDesc* wnd, int32_t newSizeX, int32_t newSizeY);
static void onFocusChanged(bool focused);
static void UpdateWindowDescFullScreenRect(WindowsDesc* winDesc)
{
	HMONITOR      currentMonitor = MonitorFromWindow((HWND)pApp->pWindow->handle.window, MONITOR_DEFAULTTONEAREST);
	MONITORINFOEX info;
	info.cbSize = sizeof(MONITORINFOEX);
	bool infoRead = GetMonitorInfo(currentMonitor, &info);

	winDesc->fullscreenRect.left = info.rcMonitor.left;
	winDesc->fullscreenRect.top = info.rcMonitor.top;
	winDesc->fullscreenRect.right = info.rcMonitor.right;
	winDesc->fullscreenRect.bottom = info.rcMonitor.bottom;
}

static void UpdateWindowDescWindowedRect(WindowsDesc* winDesc)
{
	RECT windowedRect;
	HWND hwnd = (HWND)winDesc->handle.window;

	GetWindowRect(hwnd, &windowedRect);

	winDesc->windowedRect.left = windowedRect.left;
	winDesc->windowedRect.right = windowedRect.right;
	winDesc->windowedRect.top = windowedRect.top;
	winDesc->windowedRect.bottom = windowedRect.bottom;
}

static void UpdateWindowDescClientRect(WindowsDesc* winDesc)
{
	RECT clientRect;
	HWND hwnd = (HWND)winDesc->handle.window;

	GetClientRect(hwnd, &clientRect);

	winDesc->clientRect.left = clientRect.left;
	winDesc->clientRect.right = clientRect.right;
	winDesc->clientRect.top = clientRect.top;
	winDesc->clientRect.bottom = clientRect.bottom;
}

DWORD PrepareStyleMask(WindowsDesc* winDesc)
{
	DWORD windowStyle = WS_OVERLAPPEDWINDOW;
	if (winDesc->borderlessWindow)
	{
		windowStyle = WS_POPUP | WS_THICKFRAME;
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

void OffsetRectToDisplay(WindowsDesc* winDesc, LPRECT rect)
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

static CustomMessageProcessor sCustomProc = nullptr;
void                          setCustomMessageProcessor(CustomMessageProcessor proc) { sCustomProc = proc; }

// Window event handler - Use as less as possible
LRESULT CALLBACK WinProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (gWindow == nullptr)
	{
		return DefWindowProcW(hwnd, message, wParam, lParam);
	}

	switch (message)
	{
		case WM_NCPAINT:
		case WM_WINDOWPOSCHANGED:
		case WM_STYLECHANGED:
		{
			return DefWindowProcW(hwnd, message, wParam, lParam);
		}
		case WM_DISPLAYCHANGE:
		{
			adjustWindow(gWindow);
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
			GetClientRect(hwnd, &rc);
			FillRect(hdc, &rc, hbrWhite);
			break;
		}
		case WM_WINDOWPOSCHANGING:
		case WM_MOVE:
		{
			UpdateWindowDescFullScreenRect(gWindow);
			if (!gWindow->fullScreen)
				UpdateWindowDescWindowedRect(gWindow);
			break;
		}
		case WM_STYLECHANGING:
		{
			break;
		}
		case WM_SIZE:
		{
			switch (wParam)
			{
				case SIZE_RESTORED:
				case SIZE_MAXIMIZED:
					onFocusChanged(true);
					gWindow->minimized = false;
					if (!gWindow->fullScreen && !gWindowIsResizing)
					{
						UpdateWindowDescClientRect(gWindow);
						onResize(gWindow, getRectWidth(&gWindow->clientRect), getRectHeight(&gWindow->clientRect));
					}
					break;
				case SIZE_MINIMIZED:
					onFocusChanged(false);
					gWindow->minimized = true;
					break;
				case WM_SETFOCUS: onFocusChanged(true); break;
				case WM_KILLFOCUS: onFocusChanged(false); break;
				default: onFocusChanged(true); break;
			}

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
				UpdateWindowDescClientRect(gWindow);
				onResize(gWindow, getRectWidth(&gWindow->clientRect), getRectHeight(&gWindow->clientRect));
			}
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
		default:
		{
			if (sCustomProc != nullptr)
			{
				MSG msg = {};
				msg.hwnd = hwnd;
				msg.lParam = lParam;
				msg.message = message;
				msg.wParam = wParam;

				sCustomProc(gWindow, &msg);
			}

			return DefWindowProcW(hwnd, message, wParam, lParam);
		}
	}
	return 0;
}

struct MonitorInfo
{
	unsigned index;
	WCHAR    adapterName[32];
};

static BOOL CALLBACK monitorCallback(HMONITOR pMonitor, HDC pDeviceContext, LPRECT pRect, LPARAM pParam)
{
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
	int      size = 0;
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

				const float dpi = 96.0f;
				desc.dpi[0] = static_cast<UINT>(::GetDeviceCaps(dc, LOGPIXELSX) / dpi);
				desc.dpi[1] = static_cast<UINT>(::GetDeviceCaps(dc, LOGPIXELSY) / dpi);

				gMonitors[found] = (desc);
				MonitorInfo data = {};
				data.index = found;
				wcsncpy_s(data.adapterName, adapter.DeviceName, elementsOf(adapter.DeviceName));

				EnumDisplayMonitors(NULL, NULL, monitorCallback, (LPARAM)(&data));

				DeleteDC(dc);

				if ((adapter.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE) && displayIndex == 0)
				{
					MonitorDesc desc = gMonitors[0];
					gMonitors[0] = gMonitors[found];
					gMonitors[found] = desc;
				}

				found++;
			}
		}
	}
	else
	{
		LOGF(LogLevel::eDEBUG, "FallBack Option");
		//Fallback options incase enumeration fails
		//then default to the primary device
		monitorCount = 0;
		HMONITOR currentMonitor = NULL;
		currentMonitor = MonitorFromWindow(NULL, MONITOR_DEFAULTTOPRIMARY);
		if (currentMonitor)
		{
			monitorCount = 1;
			gMonitors = (MonitorDesc*)tf_calloc(monitorCount, sizeof(MonitorDesc));

			MONITORINFOEXW info;
			info.cbSize = sizeof(MONITORINFOEXW);
			bool        infoRead = GetMonitorInfoW(currentMonitor, &info);
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

		eastl::vector<Resolution> displays;
		DWORD                     current = 0;
		while (EnumDisplaySettingsW(pMonitor->adapterName, current++, &devMode))
		{
			bool duplicate = false;
			for (uint32_t i = 0; i < (uint32_t)displays.size(); ++i)
			{
				if (displays[i].mWidth == (uint32_t)devMode.dmPelsWidth && displays[i].mHeight == (uint32_t)devMode.dmPelsHeight)
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
			displays.emplace_back(videoMode);
		}
		qsort(displays.data(), displays.size(), sizeof(Resolution), [](const void* lhs, const void* rhs) {
			Resolution* pLhs = (Resolution*)lhs;
			Resolution* pRhs = (Resolution*)rhs;
			if (pLhs->mHeight == pRhs->mHeight)
				return (int)(pLhs->mWidth - pRhs->mWidth);

			return (int)(pLhs->mHeight - pRhs->mHeight);
		});

		pMonitor->resolutionCount = (uint32_t)displays.size();
		pMonitor->resolutions = (Resolution*)tf_calloc(pMonitor->resolutionCount, sizeof(Resolution));
		memcpy(pMonitor->resolutions, displays.data(), pMonitor->resolutionCount * sizeof(Resolution));
	}
}

void setResolution(const MonitorDesc* pMonitor, const Resolution* pMode)
{
	DEVMODEW devMode = {};
	devMode.dmSize = sizeof(DEVMODEW);
	devMode.dmPelsHeight = pMode->mHeight;
	devMode.dmPelsWidth = pMode->mWidth;
	devMode.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT;
	ChangeDisplaySettingsW(&devMode, CDS_FULLSCREEN);
}

void getRecommendedResolution(RectDesc* rect)
{
	*rect = { 0, 0, min(1920, (int)(GetSystemMetrics(SM_CXSCREEN) * 0.75)), min(1080, (int)(GetSystemMetrics(SM_CYSCREEN) * 0.75)) };
}

void requestShutdown() { PostQuitMessage(0); }

void onRequestReload()
{
	gResetScenario |= RESET_SCENARIO_RELOAD;
}

void onDeviceLost()
{
	gResetScenario |= RESET_SCENARIO_DEVICE_LOST;
}

void onAPISwitch()
{
	gResetScenario |= RESET_SCENARIO_API_SWITCH;
}

void onGpuModeSwitch()
{
	gResetScenario |= RESET_SCENARIO_GPU_MODE_SWITCH;
}

void initWindowClass()
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
			//Get the error message, if any.
			DWORD errorMessageID = ::GetLastError();

			if (errorMessageID != ERROR_CLASS_ALREADY_EXISTS)
			{
				LPSTR  messageBuffer = NULL;
				size_t size = FormatMessageA(
					FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, errorMessageID,
					MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

				char* message = (char*)tf_calloc(size + 1, sizeof(char));
				strcpy(message, messageBuffer);
				LOGF(eERROR, message);
				tf_free(message);
				return;
			}
			else
			{
				gWindowClassInitialized = false;
			}
		}
	}

	collectMonitorInfo();
}

void exitWindowClass()
{
	for (uint32_t i = 0; i < gMonitorCount; ++i)
		tf_free(gMonitors[i].resolutions);

	tf_free(gMonitors);
}

void openWindow(const char* app_name, WindowsDesc* winDesc)
{
	UpdateWindowDescFullScreenRect(winDesc);

	// Defer borderless window setting
	bool borderless = winDesc->borderlessWindow;
	winDesc->borderlessWindow = false;

	// Adjust windowed rect for windowed mode rendering.
	RECT  rect = { (LONG)winDesc->clientRect.left, (LONG)winDesc->clientRect.top,
                  (LONG)winDesc->clientRect.left + (LONG)winDesc->clientRect.right,
                  (LONG)winDesc->clientRect.top + (LONG)winDesc->clientRect.bottom };
	DWORD windowStyle = PrepareStyleMask(winDesc);

	AdjustWindowRect(&rect, windowStyle, FALSE);

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

	HWND hwnd = CreateWindowW(
		FORGE_WINDOW_CLASS, app, windowStyle, windowX, windowY, rect.right - windowX, rect.bottom - windowY, NULL, NULL,
		(HINSTANCE)GetModuleHandle(NULL), 0);

	if (hwnd != NULL)
	{
		winDesc->handle.window = hwnd;

		GetClientRect(hwnd, &rect);
		winDesc->clientRect = { (int)rect.left, (int)rect.top, (int)rect.right, (int)rect.bottom };

		GetWindowRect(hwnd, &rect);
		winDesc->windowedRect = { (int)rect.left, (int)rect.top, (int)rect.right, (int)rect.bottom };

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
				toggleBorderless(winDesc, getRectWidth(&winDesc->clientRect), getRectHeight(&winDesc->clientRect));
			}

			if (winDesc->centered)
			{
				centerWindow(winDesc);
			}

			if (fullscreen)
			{
				toggleFullscreen(winDesc);
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

void closeWindow(const WindowsDesc* winDesc)
{
	DestroyWindow((HWND)winDesc->handle.window);
	handleMessages();
}

void setWindowRect(WindowsDesc* winDesc, const RectDesc* rect)
{
	HWND hwnd = (HWND)winDesc->handle.window;

	// Adjust position to prevent the window from dancing around
	int clientWidthStart = (getRectWidth(&winDesc->windowedRect) - getRectWidth(&winDesc->clientRect)) >> 1;
	int clientHeightStart = getRectHeight(&winDesc->windowedRect) - getRectHeight(&winDesc->clientRect) - clientWidthStart;

	winDesc->clientRect = *rect;

	DWORD windowStyle = PrepareStyleMask(winDesc);
	SetWindowLong(hwnd, GWL_STYLE, windowStyle);

	if (winDesc->centered)
	{
		centerWindow(winDesc);
	}
	else
	{
		RECT clientRectStyleAdjusted = { (LONG)(rect->left + clientWidthStart), (LONG)(rect->top + clientHeightStart),
										 (LONG)(clientRectStyleAdjusted.left + getRectWidth(rect)),
										 (LONG)(clientRectStyleAdjusted.top + getRectHeight(rect)) };

		AdjustWindowRect(&clientRectStyleAdjusted, windowStyle, FALSE);

		winDesc->windowedRect = { (int32_t)clientRectStyleAdjusted.left, (int32_t)clientRectStyleAdjusted.top,
								  (int32_t)clientRectStyleAdjusted.right, (int32_t)clientRectStyleAdjusted.bottom };

		SetWindowPos(
			hwnd, HWND_NOTOPMOST, clientRectStyleAdjusted.left, clientRectStyleAdjusted.top,
			clientRectStyleAdjusted.right - clientRectStyleAdjusted.left, clientRectStyleAdjusted.bottom - clientRectStyleAdjusted.top,
			SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
	}
}

void setWindowSize(WindowsDesc* winDesc, unsigned width, unsigned height)
{
	RectDesc newClientRect = { newClientRect.left = winDesc->windowedRect.left, newClientRect.top = winDesc->windowedRect.top,
							   newClientRect.right = newClientRect.left + (int32_t)width,
							   newClientRect.bottom = newClientRect.top + (int32_t)height };

	setWindowRect(winDesc, &newClientRect);
}

void adjustWindow(WindowsDesc* winDesc)
{
	HWND hwnd = (HWND)winDesc->handle.window;

	if (winDesc->fullScreen)
	{
		RECT windowedRect = {};
		// Save the old window rect so we can restore it when exiting fullscreen mode.
		GetWindowRect(hwnd, &windowedRect);
		winDesc->windowedRect = { (int)windowedRect.left, (int)windowedRect.top, (int)windowedRect.right, (int)windowedRect.bottom };

		// Make the window borderless so that the client area can fill the screen.
		SetWindowLong(hwnd, GWL_STYLE, WS_SYSMENU | WS_POPUP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VISIBLE);

		// Get the settings of the durrent display index. We want the app to go into
		// fullscreen mode on the display that supports Independent Flip.
		HMONITOR      currentMonitor = MonitorFromWindow((HWND)pApp->pWindow->handle.window, MONITOR_DEFAULTTOPRIMARY);
		MONITORINFOEX info;
		info.cbSize = sizeof(MONITORINFOEX);
		bool infoRead = GetMonitorInfo(currentMonitor, &info);

		pApp->mSettings.mWindowX = info.rcMonitor.left;
		pApp->mSettings.mWindowY = info.rcMonitor.top;

		SetWindowPos(
			hwnd, HWND_NOTOPMOST, info.rcMonitor.left, info.rcMonitor.top, info.rcMonitor.right - info.rcMonitor.left,
			info.rcMonitor.bottom - info.rcMonitor.top, SWP_FRAMECHANGED | SWP_NOACTIVATE);

		ShowWindow(hwnd, SW_MAXIMIZE);

		onResize(winDesc, info.rcMonitor.right - info.rcMonitor.left, info.rcMonitor.bottom - info.rcMonitor.top);
	}
	else
	{
		DWORD windowStyle = PrepareStyleMask(winDesc);
		SetWindowLong(hwnd, GWL_STYLE, windowStyle);

		SetWindowPos(
			hwnd, HWND_NOTOPMOST, winDesc->windowedRect.left, winDesc->windowedRect.top, getRectWidth(&winDesc->windowedRect),
			getRectHeight(&winDesc->windowedRect), SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOOWNERZORDER);

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

void toggleFullscreen(WindowsDesc* winDesc)
{
	winDesc->fullScreen = !winDesc->fullScreen;
	adjustWindow(winDesc);
}

void toggleBorderless(WindowsDesc* winDesc, unsigned width, unsigned height)
{
	if (!winDesc->fullScreen)
	{
		winDesc->borderlessWindow = !winDesc->borderlessWindow;

		bool centered = winDesc->centered;
		winDesc->centered = false;
		setWindowSize(winDesc, width, height);
		winDesc->centered = centered;
	}
}

void showWindow(WindowsDesc* winDesc)
{
	winDesc->hide = false;
	ShowWindow((HWND)winDesc->handle.window, SW_SHOW);
}

void hideWindow(WindowsDesc* winDesc)
{
	winDesc->hide = true;
	ShowWindow((HWND)winDesc->handle.window, SW_HIDE);
}

void maximizeWindow(WindowsDesc* winDesc)
{
	winDesc->maximized = true;
	ShowWindow((HWND)winDesc->handle.window, SW_MAXIMIZE);
}

void minimizeWindow(WindowsDesc* winDesc)
{
	winDesc->maximized = false;
	ShowWindow((HWND)winDesc->handle.window, SW_MINIMIZE);
}

void centerWindow(WindowsDesc* winDesc)
{
	UpdateWindowDescFullScreenRect(winDesc);

	uint32_t fsHalfWidth = getRectWidth(&winDesc->fullscreenRect) >> 1;
	uint32_t fsHalfHeight = getRectHeight(&winDesc->fullscreenRect) >> 1;
	uint32_t windowWidth = getRectWidth(&winDesc->clientRect);
	uint32_t windowHeight = getRectHeight(&winDesc->clientRect);
	uint32_t windowHalfWidth = windowWidth >> 1;
	uint32_t windowHalfHeight = windowHeight >> 1;

	uint32_t X = fsHalfWidth - windowHalfWidth;
	uint32_t Y = fsHalfHeight - windowHalfHeight;

	RECT rect = { (LONG)(X), (LONG)(Y), (LONG)(X + windowWidth), (LONG)(Y + windowHeight) };

	DWORD windowStyle = PrepareStyleMask(winDesc);

	AdjustWindowRect(&rect, windowStyle, FALSE);

	OffsetRectToDisplay(winDesc, &rect);

	SetWindowPos(
		(HWND)winDesc->handle.window, HWND_NOTOPMOST, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
		SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOOWNERZORDER);

	winDesc->windowedRect = { (int32_t)rect.left, (int32_t)rect.top, (int32_t)rect.right, (int32_t)rect.bottom };
}

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

bool isCursorInsideTrackingArea() { return gCursorInsideRectangle; }

void setMousePositionRelative(const WindowsDesc* winDesc, int32_t x, int32_t y)
{
	POINT point = { (LONG)x, (LONG)y };
	ClientToScreen((HWND)winDesc->handle.window, &point);

	SetCursorPos(point.x, point.y);
}

void setMousePositionAbsolute(int32_t x, int32_t y) { SetCursorPos(x, y); }

MonitorDesc* getMonitor(uint32_t index)
{
	ASSERT(gMonitorCount > index);
	return &gMonitors[index];
}

uint32_t getMonitorCount() { return gMonitorCount; }

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
#if (WINVER >= 0x0605)
		float systemDpi = ::GetDpiForSystem() / 96.0f;
		array[0] = systemDpi;
		array[1] = systemDpi;
#else
		array[0] = 1.f;
		array[1] = 1.f;
#endif
	}
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

static inline float CounterToSecondsElapsed(int64_t start, int64_t end) { return (float)(end - start) / (float)1e6; }
/************************************************************************/
// App Entrypoint
/************************************************************************/
static void onResize(WindowsDesc* wnd, int32_t newSizeX, int32_t newSizeY)
{
	if (pApp == nullptr || !pApp->mSettings.mInitialized)
	{
		return;
	}

	pApp->mSettings.mFullScreen = wnd->fullScreen;
	if (pApp->mSettings.mWidth == newSizeX && pApp->mSettings.mHeight == newSizeY)
	{
		return;
	}

	pApp->mSettings.mWidth = newSizeX;
	pApp->mSettings.mHeight = newSizeY;

	onRequestReload();
}

static void onFocusChanged(bool focused)
{
	if (pApp == nullptr || !pApp->mSettings.mInitialized)
	{
		return;
	}

	pApp->mSettings.mFocused = focused;
}

void errorMessagePopup(const char* title, const char* msg, void* windowHandle)
{
#ifndef AUTOMATED_TESTING
	MessageBoxA((HWND)windowHandle, msg, title, MB_OK);
#endif
}

bool initBaseSubsystems()
{
	// Not exposed in the interface files / app layer
	extern bool platformInitFontSystem(); 
	extern bool platformInitUserInterface();
	extern void platformInitLuaScriptingSystem();

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

void setupAPISwitchingUI(int32_t width, int32_t height)
{
	gSelectedApiIndex = gSelectedRendererApi;

#ifdef ENABLE_FORGE_UI
	UIComponentDesc UIComponentDesc = {};
	UIComponentDesc.mStartPosition = vec2(width * 0.4f, height * 0.01f);
	uiCreateComponent("API Switching", &UIComponentDesc, &pAPISwitchingWindow);

	static const char* pApiNames[] =
	{
	#if defined(DIRECT3D12)
		"D3D12",
	#endif
	#if defined(VULKAN)
		"Vulkan",
	#endif
	#if defined(DIRECT3D11)
		"D3D11",
	#endif
	};

	// Select Api 
	DropdownWidget selectApUIWidget;
	selectApUIWidget.pData = &gSelectedApiIndex;

	uint32_t apiCount = RENDERER_API_COUNT;
#ifdef DIRECT3D11
	if (gD3D11Unsupported) --apiCount; 
#endif
	for (uint32_t i = 0; i < apiCount; ++i)
	{
		selectApUIWidget.mNames.push_back((char*)pApiNames[i]);
		selectApUIWidget.mValues.push_back(i);
	}

	pSelectApUIWidget = uiCreateComponentWidget(pAPISwitchingWindow, "Select API", &selectApUIWidget, WIDGET_TYPE_DROPDOWN);
	pSelectApUIWidget->pOnEdited = onAPISwitch;

#ifdef ENABLE_FORGE_SCRIPTING
	luaRegisterWidget(pSelectApUIWidget);
	LuaScriptDesc apiScriptDesc = {};
	apiScriptDesc.pScriptFileName = "Test_API_Switching.lua";
	luaDefineScripts(&apiScriptDesc, 1);
#endif
#endif
}

int WindowsMain(int argc, char** argv, IApp* app)
{
	if (!initMemAlloc(app->GetName()))
		return EXIT_FAILURE;

	FileSystemInitDesc fsDesc = {};
	fsDesc.pAppName = app->GetName();
	if (!initFileSystem(&fsDesc))
		return EXIT_FAILURE;

	fsSetPathForResourceDir(pSystemFileIO, RM_DEBUG, RD_LOG, "");

#ifdef ENABLE_MTUNER
	rmemInit(0);
#endif

	initLog(app->GetName(), DEFAULT_LOG_LEVEL);

#ifdef ENABLE_FORGE_STACKTRACE_DUMP
	if (!WindowsStackTrace::Init())
		return EXIT_FAILURE;
#endif

	pApp = app;

	initWindowClass();

	//Used for automated testing, if enabled app will exit after 120 frames
#if defined(AUTOMATED_TESTING)
	uint32_t frameCounter = 0;
	uint32_t targetFrameCount = 120;
#endif

	IApp::Settings* pSettings = &pApp->mSettings;
	WindowsDesc     window = {};
	gWindow = &window;
	pApp->pWindow = &window;

	if (pSettings->mMonitorIndex < 0 || pSettings->mMonitorIndex >= (int)gMonitorCount)
	{
		pSettings->mMonitorIndex = 0;
	}

	if (pSettings->mWidth <= 0 || pSettings->mHeight <= 0)
	{
		RectDesc rect = {};

		getRecommendedResolution(&rect);
		pSettings->mWidth = getRectWidth(&rect);
		pSettings->mHeight = getRectHeight(&rect);
	}

	MonitorDesc* monitor = getMonitor(pSettings->mMonitorIndex);
	ASSERT(monitor != nullptr);

	gWindow->clientRect = { (int)pSettings->mWindowX + monitor->monitorRect.left, (int)pSettings->mWindowY + monitor->monitorRect.top,
							(int)pSettings->mWidth, (int)pSettings->mHeight };

	gWindow->windowedRect = gWindow->clientRect;
	gWindow->fullScreen = pSettings->mFullScreen;
	gWindow->maximized = false;
	gWindow->noresizeFrame = !pSettings->mDragToResize;
	gWindow->borderlessWindow = pSettings->mBorderlessWindow;
	gWindow->centered = pSettings->mCentered;
	gWindow->forceLowDPI = pSettings->mForceLowDPI;
	gWindow->overrideDefaultPosition = true;

	if (!pSettings->mExternalWindow)
		openWindow(pApp->GetName(), pApp->pWindow);
	gWindow->handle = pApp->pWindow->handle;

	pSettings->mWidth = pApp->pWindow->fullScreen ? getRectWidth(&pApp->pWindow->fullscreenRect) : getRectWidth(&pApp->pWindow->clientRect);
	pSettings->mHeight =
		pApp->pWindow->fullScreen ? getRectHeight(&pApp->pWindow->fullscreenRect) : getRectHeight(&pApp->pWindow->clientRect);

	pApp->pCommandLine = GetCommandLineA();

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

	{
		if (!initBaseSubsystems())
			return EXIT_FAILURE; 

		Timer t;
		initTimer(&t);
		if (!pApp->Init())
			return EXIT_FAILURE;

		setupAPISwitchingUI(pSettings->mWidth, pSettings->mHeight);
		pSettings->mInitialized = true;

		if (!pApp->Load())
			return EXIT_FAILURE;
		LOGF(LogLevel::eINFO, "Application Init+Load %f", getTimerMSec(&t, false) / 1000.0f);
	}

#ifdef AUTOMATED_TESTING
	if (pSettings->mBenchmarking) setAggregateFrames(targetFrameCount / 2);
#endif

	bool quit = false;
	int64_t lastCounter = getUSec();
	while (!quit)
	{
		if (gResetScenario != RESET_SCENARIO_NONE)
		{
			if (gResetScenario & RESET_SCENARIO_RELOAD)
			{
				pApp->Unload();

				if (!pApp->Load())
					return EXIT_FAILURE;

				gResetScenario &= ~RESET_SCENARIO_RELOAD;
				continue;
			}

			if (gResetScenario & RESET_SCENARIO_DEVICE_LOST)
			{
				errorMessagePopup(
					"Graphics Device Lost",
					"Connection to the graphics device has been lost.\nPlease verify the integrity of your graphics drivers.\nCheck the "
					"logs for further details.",
					&pApp->pWindow->handle.window);
			}

			pApp->Unload();
			pApp->Exit();

			exitBaseSubsystems(); 

			gSelectedRendererApi = (RendererApi)gSelectedApiIndex;
			pSettings->mInitialized = false;

			closeWindow(app->pWindow);
			openWindow(app->GetName(), app->pWindow);

			{
				if (!initBaseSubsystems())
					return EXIT_FAILURE;

				Timer t;
				initTimer(&t);
				if (!pApp->Init())
					return EXIT_FAILURE;

				setupAPISwitchingUI(pSettings->mWidth, pSettings->mHeight);
				pSettings->mInitialized = true;

				if (!pApp->Load())
					return EXIT_FAILURE;

				LOGF(LogLevel::eINFO, "Application Reset %f", getTimerMSec(&t, false) / 1000.0f);
			}

			gResetScenario = RESET_SCENARIO_NONE;
			continue;
		}

		int64_t counter = getUSec();
		float   deltaTime = CounterToSecondsElapsed(lastCounter, counter);
		lastCounter = counter;

		// if framerate appears to drop below about 6, assume we're at a breakpoint and simulate 20fps.
		if (deltaTime > 0.15f)
			deltaTime = 0.05f;

		bool lastMinimized = gWindow->minimized;

		quit = handleMessages() || pSettings->mQuit;

		// If window is minimized let other processes take over
		if (gWindow->minimized)
		{
            // Call update once after minimize so app can react.
			if (lastMinimized != gWindow->minimized)
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
		
#if defined(AUTOMATED_TESTING)
		if ((pSettings->mDefaultAutomatedTesting) && frameCounter > targetFrameCount)
		{
			quit = true;
		}
		frameCounter++;
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
	pApp->Unload();
	pApp->Exit();

	exitWindowClass();

#ifdef ENABLE_FORGE_STACKTRACE_DUMP
	WindowsStackTrace::Exit();
#endif

	exitLog();

	exitBaseSubsystems(); 

	exitFileSystem();

#ifdef ENABLE_MTUNER
	rmemUnload();
	rmemShutDown();
#endif

	exitMemAlloc();

	pApp->pWindow = gWindow = NULL;
	return 0;
}
#endif
