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

#ifdef _WIN32

#include <ctime>
#pragma comment(lib, "WinMM.lib")
#include <windowsx.h>
#include <ntverp.h>

#if !defined(_DURANGO)
#include <shlwapi.h>
#pragma comment(lib, "shlwapi.lib")
#endif

#include "../Input/InputSystem.h"
#include "../Input/InputMappings.h"

#include "../../ThirdParty/OpenSource/EASTL/vector.h"
#include "../../ThirdParty/OpenSource/EASTL/unordered_map.h"

#include "../Interfaces/IOperatingSystem.h"
#include "../Interfaces/ILog.h"
#include "../Interfaces/ITime.h"
#include "../Interfaces/IThread.h"
#include "../Interfaces/IApp.h"
#include "../Interfaces/IFileSystem.h"
#include "../Interfaces/IMemory.h"

static IApp* pApp = NULL;

#define CONFETTI_WINDOW_CLASS L"confetti"
#define MAX_KEYS 256
#define MAX_CURSOR_DELTA 200

#define GETX(l) ((int)LOWORD(l))
#define GETY(l) ((int)HIWORD(l))

#define elementsOf(a) (sizeof(a) / sizeof((a)[0]))

namespace {
bool isCaptured = false;
}

static bool		 gWindowClassInitialized = false;
static WNDCLASSW	gWindowClass;

static eastl::vector<MonitorDesc>                gMonitors;
static eastl::unordered_map<void*, WindowsDesc*> gHWNDMap;

void adjustWindow(WindowsDesc* winDesc);

namespace PlatformEvents {
extern bool wantsMouseCapture;
extern bool skipMouseCapture;

extern void onWindowResize(const WindowResizeEventData* pData);
}    // namespace PlatformEvents

static bool captureMouse(bool shouldCapture, bool shouldHide)
{
	if (shouldCapture != isCaptured)
	{
		WindowsDesc* currentWind = gHWNDMap.begin()->second;
		if (shouldCapture)
		{
			//TODO:Fix this once we have multiple window handles
			POINT lastCursorPoint;
			GetCursorPos(&lastCursorPoint);
			currentWind->lastCursorPosX = lastCursorPoint.x;
			currentWind->lastCursorPosY = lastCursorPoint.y;

			SetCapture((HWND)currentWind->handle.window);

			RECT clientRect;
			GetClientRect((HWND)currentWind->handle.window, &clientRect);
			//convert screen rect to client coordinates.
			POINT ptClientUL = { clientRect.left, clientRect.top };
			// Add one to the right and bottom sides, because the
			// coordinates retrieved by GetClientRect do not
			// include the far left and lowermost pixels.
			POINT ptClientLR = { clientRect.right + 1, clientRect.bottom + 1 };
			ClientToScreen((HWND)currentWind->handle.window, &ptClientUL);
			ClientToScreen((HWND)currentWind->handle.window, &ptClientLR);

			// Copy the client coordinates of the client area
			// to the rcClient structure. Confine the mouse cursor
			// to the client area by passing the rcClient structure
			// to the ClipCursor function.
			SetRect(&clientRect, ptClientUL.x, ptClientUL.y, ptClientLR.x, ptClientLR.y);
			ClipCursor(&clientRect);

			if (shouldHide)
				ShowCursor(FALSE);

			isCaptured = true;
		}
		else
		{
			ShowCursor(TRUE);
			ReleaseCapture();
			isCaptured = false;

			if (shouldHide)
				SetCursorPos(currentWind->lastCursorPosX, currentWind->lastCursorPosY);
		}
	}

	InputSystem::SetMouseCapture(isCaptured);

	return true;
}

// Window event handler - Use as less as possible
LRESULT CALLBACK WinProc(HWND _hwnd, UINT _id, WPARAM wParam, LPARAM lParam)
{
	WindowsDesc* gCurrentWindow = NULL;
	decltype(gHWNDMap)::iterator pNode = gHWNDMap.find(_hwnd);
	if (pNode != gHWNDMap.end())
		gCurrentWindow = pNode->second;
	else
		return DefWindowProcW(_hwnd, _id, wParam, lParam);

	switch (_id)
	{
	case WM_ACTIVATE:
		if (LOWORD(wParam) == WA_INACTIVE)
		{
			captureMouse(false, InputSystem::GetHideMouseCursorWhileCaptured());
		}
		break;

	case WM_DISPLAYCHANGE:
	{
		if (gCurrentWindow)
		{
			if (gCurrentWindow->fullScreen)
			{
				adjustWindow(gCurrentWindow);
			}
			else
			{
				adjustWindow(gCurrentWindow);
			}
		}
		break;
	}

	case WM_GETMINMAXINFO:
	{
		if (gCurrentWindow)
		{
			LPMINMAXINFO lpMMI = (LPMINMAXINFO)lParam;
			lpMMI->ptMinTrackSize.x = 128;
			lpMMI->ptMinTrackSize.y = 128;
			break;
		}
	}

	case WM_SIZE:
		if (gCurrentWindow)
		{
			if (wParam == SIZE_MINIMIZED)
			{
				gCurrentWindow->minimized = true;
			}
			else
			{
				gCurrentWindow->minimized = false;
			}
			RectDesc rect = { 0 };
			if (gCurrentWindow->fullScreen)
			{
				gCurrentWindow->fullscreenRect = { 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
				rect = gCurrentWindow->fullscreenRect;
			}
			else
			{
				if (IsIconic(_hwnd))
					return 0;

				RECT windowRect;
				GetClientRect(_hwnd, &windowRect);
				rect = { (int)windowRect.left, (int)windowRect.top, (int)windowRect.right, (int)windowRect.bottom };
				gCurrentWindow->windowedRect = rect;
			}

			if (gCurrentWindow->callbacks.onResize)
				gCurrentWindow->callbacks.onResize(gCurrentWindow, getRectWidth(rect), getRectHeight(rect));
		}
		break;
	case WM_DESTROY:
		case WM_CLOSE: PostQuitMessage(0); break;
		default: return DefWindowProcW(_hwnd, _id, wParam, lParam); break;
	}
	return 0;
}

static BOOL CALLBACK monitorCallback(HMONITOR pMonitor, HDC pDeviceContext, LPRECT pRect, LPARAM pParam)
{
	MONITORINFO info;
	info.cbSize = sizeof(info);
	GetMonitorInfo(pMonitor, &info);
	unsigned index = (unsigned)pParam;

	gMonitors[index].monitorRect = { (int)info.rcMonitor.left, (int)info.rcMonitor.top, (int)info.rcMonitor.right,
									 (int)info.rcMonitor.bottom };
	gMonitors[index].workRect = { (int)info.rcWork.left, (int)info.rcWork.top, (int)info.rcWork.right, (int)info.rcWork.bottom };

	return TRUE;
}

static void collectMonitorInfo()
{
	DISPLAY_DEVICEW adapter;
	adapter.cb = sizeof(adapter);

	int found = 0;
	int size = 0;

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
			wcsncpy_s(desc.publicAdapterName, adapter.DeviceName, elementsOf(adapter.DeviceName));
			wcsncpy_s(desc.displayName, display.DeviceName, elementsOf(display.DeviceName));
			wcsncpy_s(desc.publicDisplayName, display.DeviceName, elementsOf(display.DeviceName));

			gMonitors.push_back(desc);
			EnumDisplayMonitors(NULL, NULL, monitorCallback, gMonitors.size() - 1);

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

	for (uint32_t monitor = 0; monitor < (uint32_t)gMonitors.size(); ++monitor)
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
		pMonitor->resolutions = (Resolution*)conf_calloc(pMonitor->resolutionCount, sizeof(Resolution));
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
	*rect = { 0, 0, min(1920, GetSystemMetrics(SM_CXSCREEN)), min(1080, GetSystemMetrics(SM_CYSCREEN)) };
}

void requestShutdown() { PostQuitMessage(0); }

class StaticWindowManager
{
	public:
	StaticWindowManager()
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
			gWindowClass.lpszClassName = CONFETTI_WINDOW_CLASS;

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
					ErrorMsg(message.c_str());
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
	~StaticWindowManager()
	{
		for (uint32_t i = 0; i < (uint32_t)gMonitors.size(); ++i)
			conf_free(gMonitors[i].resolutions);
	}
} windowClass;

void openWindow(const char* app_name, WindowsDesc* winDesc)
{
	winDesc->fullscreenRect = { 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };

	// If user provided invalid or zero rect, get the rect from renderer
	if (getRectWidth(winDesc->windowedRect) <= 0 || getRectHeight(winDesc->windowedRect) <= 0)
	{
		getRecommendedResolution(&winDesc->windowedRect);
	}

	RECT clientRect = { (LONG)winDesc->windowedRect.left, (LONG)winDesc->windowedRect.top, (LONG)winDesc->windowedRect.right,
						(LONG)winDesc->windowedRect.bottom };
	AdjustWindowRect(&clientRect, WS_OVERLAPPEDWINDOW, FALSE);
	winDesc->windowedRect = { (int)clientRect.left, (int)clientRect.top, (int)clientRect.right, (int)clientRect.bottom };

	RectDesc& rect = winDesc->fullScreen ? winDesc->fullscreenRect : winDesc->windowedRect;

	WCHAR app[MAX_PATH];
	size_t charConverted = 0;
	mbstowcs_s(&charConverted, app, app_name, MAX_PATH);

	HWND hwnd = CreateWindowW(
		CONFETTI_WINDOW_CLASS, app, WS_OVERLAPPEDWINDOW | ((winDesc->visible) ? WS_VISIBLE : 0), CW_USEDEFAULT, CW_USEDEFAULT,
		rect.right - rect.left, rect.bottom - rect.top, NULL, NULL, (HINSTANCE)GetModuleHandle(NULL), 0);

	if (hwnd)
	{
		GetClientRect(hwnd, &clientRect);
		winDesc->windowedRect = { (int)clientRect.left, (int)clientRect.top, (int)clientRect.right, (int)clientRect.bottom };

		winDesc->handle.window = hwnd;
		gHWNDMap[hwnd] = winDesc;

		if (winDesc->visible)
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

		// Forward any input messages to Gainput
		InputSystem::HandleMessage(msg);
	}

#ifndef NO_GAINPUT
	if (InputSystem::GetBoolInput(UserInputKeys::KEY_CANCEL_TRIGGERED))
	{
		if (!isCaptured)
		{
			quit = true;
		}
		else
		{
			captureMouse(false, InputSystem::GetHideMouseCursorWhileCaptured());
			ClipCursor(NULL);
		}
	}

	if (InputSystem::GetBoolInput(UserInputKeys::KEY_CONFIRM_TRIGGERED))
	{
		if (!InputSystem::IsMouseCaptured() && !PlatformEvents::skipMouseCapture)
		{
			if (gHWNDMap.size() == 0)
				return quit;

			captureMouse(true, InputSystem::GetHideMouseCursorWhileCaptured());
		}
	}

	if (InputSystem::GetBoolInput(UserInputKeys::KEY_MENU_TRIGGERED) &&
		(InputSystem::GetBoolInput(UserInputKeys::KEY_LEFT_ALT_PRESSED) || InputSystem::GetBoolInput(UserInputKeys::KEY_RIGHT_ALT_PRESSED)))
	{
		if (gHWNDMap.size() == 0)
			return quit;

		//TODO:Fix this once we have multiple window handles
		WindowsDesc* currentWind = gHWNDMap.begin()->second;

		if (currentWind)
			toggleFullscreen(currentWind);
	}
#endif

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
	MoveWindow(hwnd, rect.left, rect.top, getRectWidth(rect), getRectHeight(rect), TRUE);
}

void setWindowSize(WindowsDesc* winDesc, unsigned width, unsigned height) { setWindowRect(winDesc, { 0, 0, (int)width, (int)height }); }

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

		// Get the settings of the primary display. We want the app to go into
		// fullscreen mode on the display that supports Independent Flip.
		DEVMODE devMode = {};
		devMode.dmSize = sizeof(DEVMODE);
		EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &devMode);

		SetWindowPos(
			hwnd, HWND_TOPMOST, devMode.dmPosition.x, devMode.dmPosition.y, devMode.dmPosition.x + devMode.dmPelsWidth,
			devMode.dmPosition.y + devMode.dmPelsHeight, SWP_FRAMECHANGED | SWP_NOACTIVATE);

		ShowWindow(hwnd, SW_MAXIMIZE);
	}
	else
	{
		// Restore the window's attributes and size.
		SetWindowLong(hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);

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

void showWindow(WindowsDesc* winDesc)
{
	winDesc->visible = true;
	ShowWindow((HWND)winDesc->handle.window, SW_SHOW);
}

void hideWindow(WindowsDesc* winDesc)
{
	winDesc->visible = false;
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
	ASSERT((uint32_t)gMonitors.size() > index);
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

	InputSystem::UpdateSize(pApp->mSettings.mWidth, pApp->mSettings.mHeight);
}

int WindowsMain(int argc, char** argv, IApp* app)
{
	pApp = app;

	//Used for automated testing, if enabled app will exit after 120 frames
#ifdef AUTOMATED_TESTING
	uint32_t testingFrameCount = 0;
	const uint32_t testingDesiredFrameCount = 120;
#endif

	FileSystem::SetCurrentDir(FileSystem::GetProgramDir());

	IApp::Settings* pSettings = &pApp->mSettings;
	WindowsDesc window = {};
	Timer deltaTimer;

	if (pSettings->mWidth == -1 || pSettings->mHeight == -1)
	{
		RectDesc rect = {};
		getRecommendedResolution(&rect);
		pSettings->mWidth = getRectWidth(rect);
		pSettings->mHeight = getRectHeight(rect);
	}

	window.callbacks.onResize = onResize;

	window.windowedRect = { 0, 0, (int)pSettings->mWidth, (int)pSettings->mHeight };
	window.fullScreen = pSettings->mFullScreen;
	window.maximized = false;

	if (!pSettings->mExternalWindow)
		openWindow(pApp->GetName(), &window);

	pSettings->mWidth = window.fullScreen ? getRectWidth(window.fullscreenRect) : getRectWidth(window.windowedRect);
	pSettings->mHeight = window.fullScreen ? getRectHeight(window.fullscreenRect) : getRectHeight(window.windowedRect);

	//Init Input System
	InputSystem::Init(pSettings->mWidth, pSettings->mHeight);

	pApp->pWindow = &window;
	pApp->mCommandLine = GetCommandLineA();

	{
		Timer t;
		if (!pApp->Init())
			return EXIT_FAILURE;

		if (!pApp->Load())
			return EXIT_FAILURE;
		LOGF(LogLevel::eINFO, "Application Init+Load %f", t.GetMSec(false)/1000.0f);
	}

	bool quit = false;

	while (!quit)
	{
		float deltaTime = deltaTimer.GetMSec(true) / 1000.0f;
		// if framerate appears to drop below about 6, assume we're at a breakpoint and simulate 20fps.
		if (deltaTime > 0.15f)
			deltaTime = 0.05f;

		//Update Input after message handling
		InputSystem::Update();

		quit = handleMessages();

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
		testingFrameCount++;
		if (testingFrameCount >= testingDesiredFrameCount)
			quit = true;
#endif
	}

	//Clean input resources
	InputSystem::Shutdown();

	pApp->Unload();
	pApp->Exit();
	return 0;
}
/************************************************************************/
/************************************************************************/
#endif
