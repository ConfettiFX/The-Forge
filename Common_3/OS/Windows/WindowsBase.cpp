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

#ifdef _WIN32

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

#include "../Interfaces/IOperatingSystem.h"
#include "../Interfaces/ILog.h"
#include "../Interfaces/ITime.h"
#include "../Interfaces/IThread.h"
#include "../Interfaces/IApp.h"
#include "../Interfaces/IFileSystem.h"
#include "../Interfaces/IMemory.h"

#ifdef FORGE_STACKTRACE_DUMP
#include "WindowsStackTraceDump.h"
#endif

#define FORGE_WINDOW_CLASS L"The Forge"
#define MAX_KEYS 256

#define GETX(l) ((int)LOWORD(l))
#define GETY(l) ((int)HIWORD(l))

#define elementsOf(a) (sizeof(a) / sizeof((a)[0]))

static IApp*                                     pApp = NULL;
static bool		                                 gWindowClassInitialized = false;
static WNDCLASSW	                             gWindowClass;
static MonitorDesc*                              gMonitors;
static uint32_t                                  gMonitorCount = 0;
static WindowsDesc*                              pCurrentWindow;

static void adjustWindow(WindowsDesc* winDesc);

// Window event handler - Use as less as possible
LRESULT CALLBACK WinProc(HWND _hwnd, UINT _id, WPARAM wParam, LPARAM lParam)
{
	if (!pCurrentWindow || _hwnd != pCurrentWindow->handle.window)
		return DefWindowProcW(_hwnd, _id, wParam, lParam);

	ASSERT(pCurrentWindow);

	switch (_id)
	{
	case WM_DISPLAYCHANGE:
	{
		adjustWindow(pCurrentWindow);
		break;
	}
	case WM_GETMINMAXINFO:
	{
		LPMINMAXINFO lpMMI = (LPMINMAXINFO)lParam;
		lpMMI->ptMinTrackSize.x = 128;
		lpMMI->ptMinTrackSize.y = 128;
		break;
	}
	case WM_ERASEBKGND:
	{
		// Make sure to keep consistent background color when resizing.
		HDC hdc = (HDC)wParam;
		RECT rc;
		HBRUSH hbrWhite = CreateSolidBrush(0x00000000);
		GetClientRect(_hwnd, &rc);
		FillRect(hdc, &rc, hbrWhite);
		break;
	}
	case WM_SIZE:
	{
		if (wParam == SIZE_MINIMIZED)
		{
			pCurrentWindow->minimized = true;
		}
		else
		{
			pCurrentWindow->minimized = false;
		}
		RectDesc rect = { 0 };
		if (pCurrentWindow->fullScreen)
		{
			pCurrentWindow->fullscreenRect = { 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
			rect = pCurrentWindow->fullscreenRect;
		}
		else
		{
			if (IsIconic(_hwnd))
				return 0;

			RECT windowRect;
			GetClientRect(_hwnd, &windowRect);
			rect = { (int)windowRect.left, (int)windowRect.top, (int)windowRect.right, (int)windowRect.bottom };
			pCurrentWindow->windowedRect = rect;
		}

		if (pCurrentWindow->callbacks.onResize)
			pCurrentWindow->callbacks.onResize(pCurrentWindow, getRectWidth(rect), getRectHeight(rect));
		break;
	}
	case WM_SETCURSOR:
	{
		if (LOWORD(lParam) == HTCLIENT)
		{
			if (pCurrentWindow->callbacks.setCursor)
				pCurrentWindow->callbacks.setCursor();
			else
			{
				static HCURSOR defaultCurosr = LoadCursor(NULL, IDC_ARROW);
				SetCursor(defaultCurosr);
			}
		}
		break;
	}
	case WM_DESTROY:
	case WM_CLOSE:
		PostQuitMessage(0); break;
	default:
	{
		if (pCurrentWindow->callbacks.onHandleMessage)
		{
			MSG msg = {};
			msg.hwnd = _hwnd;
			msg.lParam = lParam;
			msg.message = _id;
			msg.wParam = wParam;
			pCurrentWindow->callbacks.onHandleMessage(pCurrentWindow, &msg);
		}
		return DefWindowProcW(_hwnd, _id, wParam, lParam); break;
	}
	}
	return 0;
}

struct MonitorInfo
{
	unsigned index;
	WCHAR adapterName[32];
};

static BOOL CALLBACK monitorCallback(HMONITOR pMonitor, HDC pDeviceContext, LPRECT pRect, LPARAM pParam)
{
	MONITORINFOEXW info;
	info.cbSize = sizeof(info);
	GetMonitorInfoW(pMonitor, &info);
	MonitorInfo* data = (MonitorInfo*)pParam;
	unsigned index = data->index;

	if (wcscmp(info.szDevice, data->adapterName) == 0)
	{
		gMonitors[index].monitorRect = { (int)info.rcMonitor.left, (int)info.rcMonitor.top, (int)info.rcMonitor.right,
										 (int)info.rcMonitor.bottom };
		gMonitors[index].workRect = { (int)info.rcWork.left, (int)info.rcWork.top, (int)info.rcWork.right, (int)info.rcWork.bottom };
	}
	return TRUE;
}

static void collectMonitorInfo()
{
	DISPLAY_DEVICEW adapter;
	adapter.cb = sizeof(adapter);

	int found = 0;
	int size = 0;
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
				HDC dc;

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
		HMONITOR  currentMonitor = NULL;
		currentMonitor = MonitorFromWindow(NULL, MONITOR_DEFAULTTOPRIMARY);
		if (currentMonitor)
		{
			monitorCount = 1;
			gMonitors = (MonitorDesc*)tf_calloc(monitorCount, sizeof(MonitorDesc));

			MONITORINFOEXW info;
			info.cbSize = sizeof(MONITORINFOEXW);
			bool infoRead = GetMonitorInfoW(currentMonitor, &info);
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
		DEVMODEW devMode = {};
		devMode.dmSize = sizeof(DEVMODEW);
		devMode.dmFields = DM_PELSHEIGHT | DM_PELSWIDTH;

		EnumDisplaySettingsW(pMonitor->adapterName, ENUM_CURRENT_SETTINGS, &devMode);
		pMonitor->defaultResolution.mHeight = devMode.dmPelsHeight;
		pMonitor->defaultResolution.mWidth = devMode.dmPelsWidth;

		eastl::vector<Resolution> displays;
		DWORD current = 0;
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
	*rect = { 0, 0, min(1920, (int)(GetSystemMetrics(SM_CXSCREEN)*0.75)), min(1080, (int)(GetSystemMetrics(SM_CYSCREEN)*0.75)) };
}

void requestShutdown() { PostQuitMessage(0); }

class WindowClass
{
public:
	void Init()
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
					LPSTR messageBuffer = NULL;
					size_t size = FormatMessageA(
						FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, errorMessageID,
						MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);
					eastl::string message(messageBuffer, size);
					LOGF(eERROR, message.c_str());
					return;
				}
				else
				{
					gWindowClassInitialized = success;
				}
			}
		}

		collectMonitorInfo();
	}
	void Exit()
	{
		for (uint32_t i = 0; i < gMonitorCount; ++i)
			tf_free(gMonitors[i].resolutions);

		tf_free(gMonitors);
	}
};

void openWindow(const char* app_name, WindowsDesc* winDesc)
{
	winDesc->fullscreenRect = { 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };

	// If user provided invalid or zero rect, get the rect from renderer
	if (getRectWidth(winDesc->windowedRect) <= 0 || getRectHeight(winDesc->windowedRect) <= 0)
	{
		getRecommendedResolution(&winDesc->windowedRect);
	}

	// Adjust windowed rect for windowed mode rendering.
	RECT clientRect = { (LONG)winDesc->windowedRect.left, (LONG)winDesc->windowedRect.top, (LONG)winDesc->windowedRect.right,
						(LONG)winDesc->windowedRect.bottom };
	DWORD windowStyle = WS_OVERLAPPEDWINDOW;
	if (winDesc->noresizeFrame) windowStyle ^= WS_THICKFRAME | WS_MAXIMIZEBOX;
	if (winDesc->borderlessWindow) windowStyle ^= WS_CAPTION;
	AdjustWindowRect(&clientRect, windowStyle, FALSE);
	winDesc->windowedRect = { (int)clientRect.left, (int)clientRect.top, (int)clientRect.right, (int)clientRect.bottom };

	// Always open in adjusted windowed mode. Adjust to full screen after opening.
	RectDesc& rect = winDesc->windowedRect;

	WCHAR app[FS_MAX_PATH] = {};
	size_t charConverted = 0;
	mbstowcs_s(&charConverted, app, app_name, FS_MAX_PATH);

	//
	int windowY = rect.top;
	//because on dual monitor setup this results to always 0 which might not be the case in reality
	int windowX = rect.left;
	if (!winDesc->overrideDefaultPosition && windowX < 0)
		windowX = CW_USEDEFAULT;

	if (!winDesc->overrideDefaultPosition && windowY < 0)
		windowY = CW_USEDEFAULT;

	HWND hwnd = CreateWindowW(
		FORGE_WINDOW_CLASS,
		app,
		windowStyle | ((winDesc->hide) ? 0 : WS_VISIBLE) | WS_BORDER,
		windowX, windowY,
		rect.right - rect.left, rect.bottom - rect.top,
		NULL, NULL, (HINSTANCE)GetModuleHandle(NULL), 0);

	if (hwnd)
	{
		GetClientRect(hwnd, &clientRect);
		rect = { (int)clientRect.left, (int)clientRect.top, (int)clientRect.right, (int)clientRect.bottom };

		winDesc->handle.window = hwnd;
		pCurrentWindow = winDesc;

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
			else if (winDesc->fullScreen)
			{
				adjustWindow(winDesc);
			}
			else if (winDesc->borderlessWindow)
			{
				winDesc->borderlessWindow = false;
				toggleBorderless(winDesc, getRectWidth(rect), getRectHeight(rect));
			}
		}

		LOGF(LogLevel::eINFO, "Created window app %s", app_name);
	}
	else
	{
		LOGF(LogLevel::eERROR, "Failed to create window app %s", app_name);
	}
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

void setWindowRect(WindowsDesc* winDesc, const RectDesc& rect)
{
	HWND hwnd = (HWND)winDesc->handle.window;
	RectDesc& currentRect = winDesc->fullScreen ? winDesc->fullscreenRect : winDesc->windowedRect;
	currentRect = rect;

	RECT clientRect = { (LONG)winDesc->windowedRect.left, (LONG)winDesc->windowedRect.top, (LONG)winDesc->windowedRect.right,
						(LONG)winDesc->windowedRect.bottom };

	DWORD windowStyle = WS_OVERLAPPEDWINDOW;
	if (winDesc->noresizeFrame) windowStyle ^= WS_THICKFRAME | WS_MAXIMIZEBOX;
	if (winDesc->borderlessWindow) windowStyle ^= WS_CAPTION;

	// Apply the new style.
	SetWindowLong((HWND)winDesc->handle.window, GWL_STYLE, windowStyle);

	// Adjust window rect to maintain the client area and adjust by the caption border size.
	// This is not needed in borderless mode.
	if(!winDesc->borderlessWindow)
	{
		AdjustWindowRect(&clientRect, windowStyle, FALSE);
	}

	currentRect = { clientRect.left, clientRect.top, clientRect.right, clientRect.bottom };
	// Set the window position.
	MoveWindow(hwnd, currentRect.left, currentRect.top, getRectWidth(currentRect), getRectHeight(currentRect), TRUE);

	showWindow(winDesc);
}

void setWindowSize(WindowsDesc* winDesc, unsigned width, unsigned height)
{
	// Center the window position with the new size. Otherwise it is stuck to the top-left at 0,0.
	// Get the current monitor on which the window is displayed.
	HMONITOR  currentMonitor = MonitorFromWindow((HWND)pApp->pWindow->handle.window, MONITOR_DEFAULTTOPRIMARY);
	MONITORINFOEX info;
	info.cbSize = sizeof(MONITORINFOEX);
	bool infoRead = GetMonitorInfo(currentMonitor, &info);

	int offsetX = info.rcMonitor.left;
	int offsetY = info.rcMonitor.top;

	int screenSizeX = gMonitors[pApp->mSettings.mMonitorIndex].defaultResolution.mWidth;
	int screenSizeY = gMonitors[pApp->mSettings.mMonitorIndex].defaultResolution.mHeight;

	// Percent ratio of requested size to display size.
	float screenRatioX = 1.f - ((float)width / (float)screenSizeX);
	float screenRatioY = 1.f - ((float)height / (float)screenSizeY);

	// Get the centered start position in pixels.
	float screenStartX = offsetX + (screenSizeX * screenRatioX * 0.5f);
	float screenStartY = offsetY + (screenSizeY * screenRatioY * 0.5f);

	pApp->mSettings.mWindowX = (int)screenStartX;
	pApp->mSettings.mWindowY = (int)screenStartY;
	// Set the start and end positions of the window in pixels.
	setWindowRect(winDesc, { (int)screenStartX,  (int)screenStartY,  (int)(screenStartX + width), (int)(screenStartY + height) });
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
		HMONITOR  currentMonitor = MonitorFromWindow((HWND)pApp->pWindow->handle.window, MONITOR_DEFAULTTOPRIMARY);
		MONITORINFOEX info;
		info.cbSize = sizeof(MONITORINFOEX);
		bool infoRead = GetMonitorInfo(currentMonitor, &info);

		pApp->mSettings.mWindowX = info.rcMonitor.left;
		pApp->mSettings.mWindowY = info.rcMonitor.top;

		SetWindowPos(
			hwnd, HWND_TOPMOST, info.rcMonitor.left, info.rcMonitor.top, info.rcMonitor.right - info.rcMonitor.left,
			info.rcMonitor.bottom - info.rcMonitor.top, SWP_FRAMECHANGED | SWP_NOACTIVATE);

		ShowWindow(hwnd, SW_MAXIMIZE);
	}
	else
	{
		// Restore the window's attributes and size. Remember to set the correct style.
		DWORD windowStyle = WS_OVERLAPPEDWINDOW;
		if (winDesc->noresizeFrame) windowStyle ^= WS_THICKFRAME | WS_MAXIMIZEBOX;
		if (winDesc->borderlessWindow) windowStyle ^= WS_CAPTION;
		SetWindowLong(hwnd, GWL_STYLE, windowStyle);

		SetWindowPos(
			hwnd, HWND_NOTOPMOST, winDesc->windowedRect.left, winDesc->windowedRect.top,
			winDesc->windowedRect.right - winDesc->windowedRect.left, winDesc->windowedRect.bottom - winDesc->windowedRect.top,
			SWP_FRAMECHANGED | SWP_NOACTIVATE);

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
		setWindowSize(winDesc, width, height);
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

void setMousePositionRelative(const WindowsDesc* winDesc, int32_t x, int32_t y)
{
	POINT point = { (LONG)x, (LONG)y };
	ClientToScreen((HWND)winDesc->handle.window, &point);
	//SetCursorPos(point.x, point.y);
}

MonitorDesc* getMonitor(uint32_t index)
{
	ASSERT(gMonitorCount > index);
	return &gMonitors[index];
}

float2 getDpiScale()
{
	HDC hdc = ::GetDC(NULL);
	float2 ret = {};
	const float dpi = 96.0f;
	if (hdc)
	{
		ret.x = (UINT)(::GetDeviceCaps(hdc, LOGPIXELSX)) / dpi;
		ret.y = static_cast<UINT>(::GetDeviceCaps(hdc, LOGPIXELSY)) / dpi;
		::ReleaseDC(NULL, hdc);
	}
	else
	{
#if(WINVER >= 0x0605)
		float systemDpi = ::GetDpiForSystem() / 96.0f;
		ret = { systemDpi, systemDpi };
#else
		ret = { 1.0f, 1.0f };
#endif
	}

	return ret;
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

static inline float CounterToSecondsElapsed(int64_t start, int64_t end)
{
	return (float)(end - start) / (float)1e6;
}
/************************************************************************/
// App Entrypoint
/************************************************************************/
static void onResize(WindowsDesc* wnd, int32_t newSizeX, int32_t newSizeY)
{
	if (!pApp)
		return;

	pApp->mSettings.mWidth = newSizeX;
	pApp->mSettings.mHeight = newSizeY;

	pApp->mSettings.mFullScreen = wnd->fullScreen;
	pApp->Unload();
	pApp->Load();
}


int WindowsMain(int argc, char** argv, IApp* app)
{
	extern bool MemAllocInit(const char*);
	extern void MemAllocExit();

	if (!MemAllocInit(app->GetName()))
		return EXIT_FAILURE;

	FileSystemInitDesc fsDesc = {};
	fsDesc.pAppName = app->GetName();

	if (!initFileSystem(&fsDesc))
		return EXIT_FAILURE;

	fsSetPathForResourceDir(pSystemFileIO, RM_DEBUG, RD_LOG, "");

#if USE_MTUNER
	rmemInit(0);
#endif
	
	Log::Init(app->GetName());

#ifdef FORGE_STACKTRACE_DUMP
	if (!WindowsStackTrace::Init())
		return EXIT_FAILURE;
#endif

	WindowClass wnd;

	pApp = app;

	wnd.Init();

	//Used for automated testing, if enabled app will exit after 120 frames
#ifdef AUTOMATED_TESTING
	uint32_t testingFrameCount = 0;
	const uint32_t testingDesiredFrameCount = 120;
#endif

	IApp::Settings* pSettings = &pApp->mSettings;
	WindowsDesc window = {};
	Timer deltaTimer;

	if (!pSettings->mInitialized)
	{
		pApp->pWindow = &window;
		if (pSettings->mWidth == -1 || pSettings->mHeight == -1)
		{
			RectDesc rect = {};
			getRecommendedResolution(&rect);
			pSettings->mWidth = getRectWidth(rect);
			pSettings->mHeight = getRectHeight(rect);
		}


		window.windowedRect = { 0, 0, (int)pSettings->mWidth, (int)pSettings->mHeight };
		window.fullScreen = pSettings->mFullScreen;
		window.maximized = false;
		window.noresizeFrame = !pSettings->mDragToResize;
		window.borderlessWindow = pSettings->mBorderlessWindow;
	}
	else
	{
		//get the requested monitor info and calculate windowedRect
		if (pSettings->mMonitorIndex <= 0 || pSettings->mMonitorIndex >= (int)gMonitorCount)
		{
			pSettings->mMonitorIndex = 0;
		}
		MonitorDesc* monitor = getMonitor(pSettings->mMonitorIndex);
		int windowWidth = pSettings->mWidth;
		if (windowWidth <= 0 || windowWidth >= (int)monitor->defaultResolution.mWidth)
		{
			if (pSettings->mAllowedOverSizeWindows)
			{
				windowWidth = max(windowWidth, (int)monitor->defaultResolution.mWidth);
			}
			else
			{
				windowWidth = monitor->defaultResolution.mWidth;
			}
			pSettings->mWidth = windowWidth;
		}
		int windowHeight = pSettings->mHeight;
		if (windowHeight <= 0 || windowHeight >= (int)monitor->defaultResolution.mHeight)
		{
			if (pSettings->mAllowedOverSizeWindows)
			{
				windowHeight = max(windowHeight, (int)monitor->defaultResolution.mHeight);
			}
			else
			{
				windowHeight = monitor->defaultResolution.mHeight;
			}
			pSettings->mHeight = windowHeight;
		}

		int screenSizeX = monitor->defaultResolution.mWidth;
		int screenSizeY = monitor->defaultResolution.mHeight;

		// Percent ratio of requested size to display size.
		float screenRatioX = 1.f - ((float)pSettings->mWidth / (float)screenSizeX);
		float screenRatioY = 1.f - ((float)pSettings->mHeight / (float)screenSizeY);

		//check if requested windowX and windowY fall in bounds else default to center or if the window is fullscreen
		int monitorLeft = monitor->monitorRect.left;
		int monitorWidth = monitor->monitorRect.right - monitor->monitorRect.left;

		int monitorTop = monitor->monitorRect.top;
		int monitorHeight = monitor->monitorRect.bottom - monitor->monitorRect.top;

		pSettings->mWindowX = pSettings->mWindowX + monitor->monitorRect.left;
		if (pSettings->mWindowX < monitorLeft || pSettings->mWindowX >= monitorLeft + monitorWidth || pApp->pWindow->fullScreen)
			pSettings->mWindowX = monitorLeft + (int)(screenSizeX * screenRatioX * 0.5f);

		pSettings->mWindowY = pSettings->mWindowY + monitor->monitorRect.top;
		if (pSettings->mWindowY < monitorTop || pSettings->mWindowY >= monitorTop + monitorHeight || pApp->pWindow->fullScreen)
			pSettings->mWindowY = monitorTop + (int)(screenSizeY * screenRatioY * 0.5f);

		pApp->pWindow->windowedRect = { pSettings->mWindowX, pSettings->mWindowY, pSettings->mWindowX + (int)pSettings->mWidth, pSettings->mWindowY + (int)pSettings->mHeight };
		//original client rect before adjustment
		pApp->pWindow->clientRect = pApp->pWindow->windowedRect;
	}

	if (!pSettings->mExternalWindow)
		openWindow(pApp->GetName(), pApp->pWindow);

	pSettings->mWidth = pApp->pWindow->fullScreen ? getRectWidth(pApp->pWindow->fullscreenRect) : getRectWidth(pApp->pWindow->windowedRect);
	pSettings->mHeight = pApp->pWindow->fullScreen ? getRectHeight(pApp->pWindow->fullscreenRect) : getRectHeight(pApp->pWindow->windowedRect);

	pApp->pCommandLine = GetCommandLineA();
	{
		Timer t;
		if (!pApp->Init())
			return EXIT_FAILURE;

		if (!pApp->Load())
			return EXIT_FAILURE;
		LOGF(LogLevel::eINFO, "Application Init+Load %f", t.GetMSec(false)/1000.0f);
	}

	// register callback after app has loaded since the callback needs to unload
	window.callbacks.onResize = onResize;

	bool quit = false;
	int64_t lastCounter = getUSec();
	while (!quit)
	{
		int64_t counter = getUSec();
		float deltaTime = CounterToSecondsElapsed(lastCounter, counter);
		lastCounter = counter;

		// if framerate appears to drop below about 6, assume we're at a breakpoint and simulate 20fps.
		if (deltaTime > 0.15f)
			deltaTime = 0.05f;

		quit = handleMessages() || pSettings->mQuit;

		// If window is minimized let other processes take over
		if (window.minimized)
		{
			Thread::Sleep(1);
			continue;
		}

		pApp->Update(deltaTime);
		pApp->Draw();

#ifdef AUTOMATED_TESTING
		//used in automated tests only.
		if (pSettings->mDefaultAutomatedTesting)
		{
			testingFrameCount++;
			if (testingFrameCount >= testingDesiredFrameCount)
				quit = true;
		}
#endif
	}

	pApp->Unload();
	pApp->Exit();

	wnd.Exit();

#ifdef FORGE_STACKTRACE_DUMP
	WindowsStackTrace::Exit();
#endif

	Log::Exit();

	exitFileSystem();

#if USE_MTUNER
	rmemUnload();
	rmemShutDown();
#endif

	MemAllocExit();

	return 0;
}
#endif
