/*
 * Copyright (c) 2018 Confetti Interactive Inc.
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

#include "../../ThirdParty/OpenSource/TinySTL/vector.h"
#include "../../ThirdParty/OpenSource/TinySTL/unordered_map.h"

#include "../Interfaces/IOperatingSystem.h"
#include "../Interfaces/IPlatformEvents.h"
#include "../Interfaces/ILogManager.h"
#include "../Interfaces/ITimeManager.h"
#include "../Interfaces/IThread.h"
#include "../Interfaces/IMemoryManager.h"

#define CONFETTI_WINDOW_CLASS L"confetti"
#define MAX_KEYS 256
#define MAX_CURSOR_DELTA 200

#define GETX(l) (int(l & 0xFFFF))
#define GETY(l) (int(l) >> 16)

#define elementsOf(a) (sizeof(a) / sizeof((a)[0]))

namespace
{
	bool isCaptured = false;
}

struct KeyState
{
	bool current;   // What is the current key state?
	bool previous;  // What is the previous key state?
	bool down;      // Was the key down this frame?
	bool released;  // Was the key released this frame?
};

static bool			gWindowClassInitialized = false;
static WNDCLASSW	gWindowClass;
static bool			gAppRunning = false;

static KeyState		gKeys[MAX_KEYS] = { { false, false, false, false } };

static tinystl::vector <MonitorDesc> gMonitors;
static tinystl::unordered_map<void*, WindowsDesc*> gHWNDMap;

void adjustWindow(WindowsDesc* winDesc);

namespace PlatformEvents
{
	extern bool wantsMouseCapture;
	extern bool skipMouseCapture;

	extern void onWindowResize(const WindowResizeEventData* pData);
	extern void onKeyboardChar(const KeyboardCharEventData* pData);
	extern void onKeyboardButton(const KeyboardButtonEventData* pData);
	extern void onMouseMove(const MouseMoveEventData* pData);
	extern void onRawMouseMove(const RawMouseMoveEventData* pData);
	extern void onMouseButton(const MouseButtonEventData* pData);
	extern void onMouseWheel(const MouseWheelEventData* pData);
}

// Update the state of the keys based on state previous frame
void updateKeys(void)
{
	// Calculate each of the key states here
	for (KeyState& element : gKeys)
	{
		element.down = element.current == true;
		element.released = ((element.previous == true) && (element.current == false));
		// Record this state
		element.previous = element.current;
	}
}

// Update the given key
static void updateKeyArray(int uMsg, unsigned int wParam)
{
	KeyboardButtonEventData eventData;
	eventData.key = wParam;
	switch (uMsg)
	{
	case WM_SYSKEYDOWN:
	case WM_KEYDOWN:
		if ((0 <= wParam) && (wParam <= MAX_KEYS))
			gKeys[wParam].current = true;

		eventData.pressed = true;
		break;

	case WM_SYSKEYUP:
	case WM_KEYUP:
		if ((0 <= wParam) && (wParam <= MAX_KEYS))
			gKeys[wParam].current = false;

		eventData.pressed = false;
		break;

	default:
		break;
	}

	PlatformEvents::onKeyboardButton(&eventData);
}

static bool captureMouse(HWND hwnd, bool shouldCapture)
{
	if (shouldCapture != isCaptured)
	{
		if (shouldCapture)
		{
			ShowCursor(FALSE);
			isCaptured = true;
		}
		else
		{
			ShowCursor(TRUE);
			isCaptured = false;
		}
	}

	return true;
}

// Window event handler - Use as less as possible
LRESULT CALLBACK WinProc(HWND _hwnd, UINT _id, WPARAM wParam, LPARAM lParam)
{
	WindowsDesc* gCurrentWindow = NULL;
	tinystl::unordered_hash_node<void*, WindowsDesc*>* pNode = gHWNDMap.find(_hwnd).node;
	if (pNode)
		gCurrentWindow = pNode->second;
	else
		return DefWindowProcW(_hwnd, _id, wParam, lParam);

	switch (_id)
	{
	case WM_ACTIVATE:
		if (LOWORD(wParam) == WA_INACTIVE)
		{
			captureMouse(_hwnd, false);
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

	case WM_SIZE:
		if (gCurrentWindow)
		{
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

			WindowResizeEventData eventData = { rect, gCurrentWindow };
			PlatformEvents::onWindowResize(&eventData);
		}
		break;

	case WM_CLOSE:
	case WM_QUIT:
		gAppRunning = false;
		break;

	case WM_CHAR:
	{
		KeyboardCharEventData eventData;
		eventData.unicode = (unsigned)wParam;
		PlatformEvents::onKeyboardChar(&eventData);
		break;
	}
	case WM_MOUSEMOVE:
	{
		static int lastX = 0, lastY = 0;
		int x, y;
		x = GETX(lParam);
		y = GETY(lParam);

		MouseMoveEventData eventData;
		eventData.x = x;
		eventData.y = y;
		eventData.deltaX = x - lastX;
		eventData.deltaY = y - lastY;
		eventData.captured = isCaptured;
		PlatformEvents::onMouseMove(&eventData);

		lastX = x;
		lastY = y;
		break;
	}
	case WM_INPUT:
	{
		UINT dwSize;
		static BYTE lpb[128] = {};

		GetRawInputData((HRAWINPUT)lParam, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER));

		RAWINPUT* raw = (RAWINPUT*)lpb;

		if (raw->header.dwType == RIM_TYPEMOUSE)
		{
			static int lastX = 0, lastY = 0;

			int xPosRelative = raw->data.mouse.lLastX;
			int yPosRelative = raw->data.mouse.lLastY;

			RawMouseMoveEventData eventData;
			eventData.x = xPosRelative;
			eventData.y = yPosRelative;
			eventData.captured = isCaptured;
			PlatformEvents::onRawMouseMove(&eventData);

			lastX = xPosRelative;
			lastY = yPosRelative;
		}

		return 0;
	}
	case WM_LBUTTONDOWN:
	{
		MouseButtonEventData eventData;
		eventData.button = MOUSE_LEFT;
		eventData.pressed = true;
		eventData.x = GETX(lParam);
		eventData.y = GETY(lParam);
		if (PlatformEvents::wantsMouseCapture && !PlatformEvents::skipMouseCapture && !isCaptured)
		{
			captureMouse(_hwnd, true);
		}
		PlatformEvents::onMouseButton(&eventData);
		break;
	}
	case WM_LBUTTONUP:
	{
		MouseButtonEventData eventData;
		eventData.button = MOUSE_LEFT;
		eventData.pressed = false;
		eventData.x = GETX(lParam);
		eventData.y = GETY(lParam);
		PlatformEvents::onMouseButton(&eventData);
		break;
	}
	case WM_RBUTTONDOWN:
	{
		MouseButtonEventData eventData;
		eventData.button = MOUSE_RIGHT;
		eventData.pressed = true;
		eventData.x = GETX(lParam);
		eventData.y = GETY(lParam);
		PlatformEvents::onMouseButton(&eventData);
		break;
	}
	case WM_RBUTTONUP:
	{
		MouseButtonEventData eventData;
		eventData.button = MOUSE_RIGHT;
		eventData.pressed = false;
		eventData.x = GETX(lParam);
		eventData.y = GETY(lParam);
		PlatformEvents::onMouseButton(&eventData);
		break;
	}
	case WM_MBUTTONDOWN:
	{
		MouseButtonEventData eventData;
		eventData.button = MOUSE_MIDDLE;
		eventData.pressed = true;
		eventData.x = GETX(lParam);
		eventData.y = GETY(lParam);
		PlatformEvents::onMouseButton(&eventData);
		break;
	}
	case WM_MBUTTONUP:
	{
		MouseButtonEventData eventData;
		eventData.button = MOUSE_MIDDLE;
		eventData.pressed = false;
		eventData.x = GETX(lParam);
		eventData.y = GETY(lParam);
		PlatformEvents::onMouseButton(&eventData);
		break;
	}
	case WM_MOUSEWHEEL:
	{
		static int scroll;
		int s;

		scroll += GET_WHEEL_DELTA_WPARAM(wParam);
		s = scroll / WHEEL_DELTA;
		scroll %= WHEEL_DELTA;

		POINT point;
		point.x = GETX(lParam);
		point.y = GETY(lParam);
		ScreenToClient(_hwnd, &point);

		if (s != 0)
		{
			MouseWheelEventData eventData;
			eventData.scroll = s;
			eventData.x = point.x;
			eventData.y = point.y;
			PlatformEvents::onMouseWheel(&eventData);
		}
		break;
	}
	case WM_SYSKEYDOWN:
		if ((lParam & (1 << 29)) && (wParam == KEY_ENTER))
		{
			toggleFullscreen(gCurrentWindow);
		}
		updateKeyArray(_id, (unsigned)wParam);
		break;

	case WM_SYSKEYUP:
		updateKeyArray(_id, (unsigned)wParam);
		break;

	case WM_KEYUP:
		if (wParam == KEY_ESCAPE)
		{
			if (!isCaptured)
			{
				gAppRunning = false;
			}
			else
			{
				captureMouse(_hwnd, false);
			}
		}
		updateKeyArray(_id, (unsigned)wParam);
		break;

	case WM_KEYDOWN:
		updateKeyArray(_id, (unsigned)wParam);
		break;
	default:
		return DefWindowProcW(_hwnd, _id, wParam, lParam);
		break;
	}

	return 0;
}

static BOOL CALLBACK monitorCallback(HMONITOR pMonitor, HDC pDeviceContext, LPRECT pRect, LPARAM pParam)
{
	MONITORINFO info;
	info.cbSize = sizeof(info);
	GetMonitorInfo(pMonitor, &info);
	unsigned index = (unsigned)pParam;

	gMonitors[index].monitorRect = { (int)info.rcMonitor.left, (int)info.rcMonitor.top, (int)info.rcMonitor.right, (int)info.rcMonitor.bottom };
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

		for (int displayIndex = 0; ; displayIndex++)
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

		tinystl::vector<Resolution> displays;
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

bool isRunning()
{
	return gAppRunning;
}

void getRecommendedResolution(RectDesc* rect)
{
	*rect = { 0, 0, min(1920, GetSystemMetrics(SM_CXSCREEN)), min(1080, GetSystemMetrics(SM_CYSCREEN)) };
}

void requestShutDown()
{
	gAppRunning = false;
}

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

			gAppRunning = RegisterClassW(&gWindowClass) != 0;

			if (!gAppRunning)
			{
				//Get the error message, if any.
				DWORD errorMessageID = ::GetLastError();

				if (errorMessageID != ERROR_CLASS_ALREADY_EXISTS)
				{
					LPSTR messageBuffer = nullptr;
					size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
						NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);
					String message(messageBuffer, size);
					ErrorMsg(message.c_str());
					return;
				}
				else
				{
					gAppRunning = true;
					gWindowClassInitialized = gAppRunning;
				}
			}

			RAWINPUTDEVICE Rid[2];

			Rid[0].usUsagePage = 0x01;
			Rid[0].usUsage = 0x02;
			Rid[0].dwFlags = 0;	// adds HID mouse and also ignores legacy mouse messages
			Rid[0].hwndTarget = 0;

			Rid[1].usUsagePage = 0x01;
			Rid[1].usUsage = 0x06;
			Rid[1].dwFlags = 0;	// adds HID keyboard and also ignores legacy keyboard messages
			Rid[1].hwndTarget = 0;

			if (RegisterRawInputDevices(Rid, 2, sizeof(Rid[0])) == FALSE)
			{
				LOGERRORF("Failed to register raw input devices");
				//registration failed. Call GetLastError for the cause of the error
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

	RECT clientRect = { (LONG)winDesc->windowedRect.left, (LONG)winDesc->windowedRect.top, (LONG)winDesc->windowedRect.right, (LONG)winDesc->windowedRect.bottom };
	AdjustWindowRect(&clientRect, WS_OVERLAPPEDWINDOW, FALSE);
	winDesc->windowedRect = { (int)clientRect.left, (int)clientRect.top, (int)clientRect.right, (int)clientRect.bottom };

	RectDesc& rect = winDesc->fullScreen ? winDesc->fullscreenRect : winDesc->windowedRect;

	WCHAR app[MAX_PATH];
	size_t charConverted = 0;
	mbstowcs_s(&charConverted, app, app_name, MAX_PATH);

	HWND hwnd = CreateWindowW(CONFETTI_WINDOW_CLASS,
		app,
		WS_OVERLAPPEDWINDOW | ((winDesc->visible) ? WS_VISIBLE : 0),
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		rect.right - rect.left,
		rect.bottom - rect.top,
		NULL,
		NULL,
		(HINSTANCE)GetModuleHandle(NULL),
		0
	);

	if (hwnd)
	{
		GetClientRect(hwnd, &clientRect);
		winDesc->windowedRect = { (int)clientRect.left, (int)clientRect.top, (int)clientRect.right, (int)clientRect.bottom };

		winDesc->handle = hwnd;
		gHWNDMap.insert({ hwnd, winDesc });

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

		LOGINFOF("Created window app %s", app_name);
	}
	else
	{
		LOGERRORF("Failed to create window app %s", app_name);
	}
}

void closeWindow(const WindowsDesc* winDesc)
{
	DestroyWindow((HWND)winDesc->handle);
}

void handleMessages()
{
	MSG msg;
	msg.message = NULL;
	while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	updateKeys();
}

void setWindowRect(WindowsDesc* winDesc, const RectDesc& rect)
{
	HWND hwnd = (HWND)winDesc->handle;
	RectDesc& currentRect = winDesc->fullScreen ? winDesc->fullscreenRect : winDesc->windowedRect;
	currentRect = rect;
	MoveWindow(hwnd, rect.left, rect.top, getRectWidth(rect), getRectHeight(rect), TRUE);
}

void setWindowSize(WindowsDesc* winDesc, unsigned width, unsigned height)
{
	setWindowRect(winDesc, { 0, 0, (int)width, (int)height });
}

void adjustWindow(WindowsDesc* winDesc)
{
	HWND hwnd = (HWND)winDesc->handle;

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
			hwnd,
			HWND_TOPMOST,
			devMode.dmPosition.x,
			devMode.dmPosition.y,
			devMode.dmPosition.x + devMode.dmPelsWidth,
			devMode.dmPosition.y + devMode.dmPelsHeight,
			SWP_FRAMECHANGED | SWP_NOACTIVATE);

		ShowWindow(hwnd, SW_MAXIMIZE);
	}
	else
	{
		// Restore the window's attributes and size.
		SetWindowLong(hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);

		SetWindowPos(
			hwnd,
			HWND_NOTOPMOST,
			winDesc->windowedRect.left,
			winDesc->windowedRect.top,
			winDesc->windowedRect.right - winDesc->windowedRect.left,
			winDesc->windowedRect.bottom - winDesc->windowedRect.top,
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
	ShowWindow((HWND)winDesc->handle, SW_SHOW);
}

void hideWindow(WindowsDesc* winDesc)
{
	winDesc->visible = false;
	ShowWindow((HWND)winDesc->handle, SW_HIDE);
}

void maximizeWindow(WindowsDesc* winDesc)
{
	winDesc->maximized = true;
	ShowWindow((HWND)winDesc->handle, SW_MAXIMIZE);
}

void minimizeWindow(WindowsDesc* winDesc)
{
	winDesc->maximized = false;
	ShowWindow((HWND)winDesc->handle, SW_MINIMIZE);
}

void setMousePositionRelative(const WindowsDesc* winDesc, int32_t x, int32_t y)
{
	POINT point = { (LONG)x, (LONG)y };
	ClientToScreen((HWND)winDesc->handle, &point);
	//SetCursorPos(point.x, point.y);
}

MonitorDesc* getMonitor(uint32_t index)
{
	ASSERT((uint32_t)gMonitors.size() > index);
	return &gMonitors[index];
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

float2 getMousePosition()
{
	POINT point;
	GetCursorPos(&point);
	return float2((float)point.x, (float)point.y);
}

bool getKeyDown(int key)
{
	return gKeys[key].down;
}

bool getKeyUp(int key)
{
	return gKeys[key].released;
}

bool getJoystickButtonDown(int button)
{
	// TODO: Implement gamepad / joystick support on windows
	return false;
}

bool getJoystickButtonUp(int button)
{
	// TODO: Implement gamepad / joystick support on windows
	return false;
}

/************************************************************************/
// Time Related Functions
/************************************************************************/
unsigned getSystemTime()
{
	return (unsigned)timeGetTime();
}

unsigned getTimeSinceStart()
{
	return (unsigned)time(NULL);
}

void sleep(unsigned mSec)
{
	::Sleep((DWORD)mSec);
}

static int64_t highResTimerFrequency = 0;

void initTime()
{
	LARGE_INTEGER frequency;
	if (QueryPerformanceFrequency(&frequency))
	{
		highResTimerFrequency = frequency.QuadPart;
	}
	else
	{
		highResTimerFrequency = 1000LL;
	}
}

// Make sure timer frequency is initialized before anyone tries to use it
struct StaticTime { StaticTime() { initTime(); } }staticTimeInst;

int64_t getUSec()
{
	LARGE_INTEGER counter;
	QueryPerformanceCounter(&counter);
	return counter.QuadPart;
}

int64_t getMSec()
{
	LARGE_INTEGER curr;
	QueryPerformanceCounter(&curr);
	return curr.QuadPart * (int64_t)1e3;
}

int64_t getTimerFrequency()
{
	if (highResTimerFrequency == 0)
		initTime();

	return highResTimerFrequency;
}
/************************************************************************/
// App Entrypoint
/************************************************************************/
#include "../Interfaces/IApp.h"
#include "../Interfaces/IFileSystem.h"

static IApp* pApp = NULL;

static void onResize(const WindowResizeEventData* pData)
{
	pApp->mSettings.mWidth = getRectWidth(pData->rect);
	pApp->mSettings.mHeight = getRectHeight(pData->rect);
	pApp->mSettings.mFullScreen = pData->pWindow->fullScreen;
	pApp->Unload();
	pApp->Load();
}

int WindowsMain(int argc, char** argv, IApp* app)
{
	pApp = app;

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

	window.windowedRect = { 0, 0, (int)pSettings->mWidth, (int)pSettings->mHeight };
	window.fullScreen = pSettings->mFullScreen;
	window.maximized = false;
	openWindow(pApp->GetName(), &window);

	pSettings->mWidth = window.fullScreen ? getRectWidth(window.fullscreenRect) : getRectWidth(window.windowedRect);
	pSettings->mHeight = window.fullScreen ? getRectHeight(window.fullscreenRect) : getRectHeight(window.windowedRect);
	pApp->pWindow = &window;

	if (!pApp->Init())
		return EXIT_FAILURE;

	registerWindowResizeEvent(onResize);

	while (isRunning())
	{
		float deltaTime = deltaTimer.GetMSec(true) / 1000.0f;
		// if framerate appears to drop below about 6, assume we're at a breakpoint and simulate 20fps.
		if (deltaTime > 0.15f)
			deltaTime = 0.05f;

		handleMessages();
		pApp->Update(deltaTime);
		pApp->Draw();
	}

	pApp->Exit();

	return 0;
}
/************************************************************************/
/************************************************************************/
#endif
