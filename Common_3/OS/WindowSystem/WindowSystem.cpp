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

#include "../Core/Config.h"

#include "../Interfaces/IUI.h"
#include "../Interfaces/IApp.h"
#include "../Interfaces/ITime.h"
#include "../Interfaces/IInput.h"
#include "../Interfaces/IScripting.h"
#include "../Interfaces/IOperatingSystem.h"

static WindowDesc* pWindowRef = NULL;
static UIComponent* pWindowControlsComponent = NULL;

Timer gHideTimer;

char gPlatformName[64];

static bool wndValidateWindowPos(int32_t x, int32_t y)
{
	WindowDesc* winDesc = pWindowRef;
	int          clientWidthStart = (getRectWidth(&winDesc->windowedRect) - getRectWidth(&winDesc->clientRect)) >> 1;
	int          clientHeightStart = getRectHeight(&winDesc->windowedRect) - getRectHeight(&winDesc->clientRect) - clientWidthStart;

	if (winDesc->centered)
	{
		uint32_t fsHalfWidth = getRectWidth(&winDesc->fullscreenRect) >> 1;
		uint32_t fsHalfHeight = getRectHeight(&winDesc->fullscreenRect) >> 1;
		uint32_t windowWidth = getRectWidth(&winDesc->clientRect);
		uint32_t windowHeight = getRectHeight(&winDesc->clientRect);
		uint32_t windowHalfWidth = windowWidth >> 1;
		uint32_t windowHalfHeight = windowHeight >> 1;

		int32_t X = fsHalfWidth - windowHalfWidth;
		int32_t Y = fsHalfHeight - windowHalfHeight;

		if ((abs(winDesc->windowedRect.left + clientWidthStart - X) > 1) || (abs(winDesc->windowedRect.top + clientHeightStart - Y) > 1))
			return false;
	}
	else
	{
		if ((abs(x - winDesc->windowedRect.left - clientWidthStart) > 1) || (abs(y - winDesc->windowedRect.top - clientHeightStart) > 1))
			return false;
	}

	return true;
}

static bool wndValidateWindowSize(int32_t width, int32_t height)
{
	RectDesc clientRect = pWindowRef->clientRect;
	if ((abs(getRectWidth(&clientRect) - width) > 1) || (abs(getRectHeight(&clientRect) - height) > 1))
		return false;
	return true;
}

void wndSetWindowed()
{
	WindowDesc* pWindow = pWindowRef;

	pWindow->maximized = false;

	int clientWidthStart = (getRectWidth(&pWindow->windowedRect) - getRectWidth(&pWindow->clientRect)) >> 1,
		clientHeightStart = getRectHeight(&pWindow->windowedRect) - getRectHeight(&pWindow->clientRect) - clientWidthStart;
	int32_t x = pWindow->windowedRect.left + clientWidthStart, y = pWindow->windowedRect.top + clientHeightStart,
		w = getRectWidth(&pWindow->clientRect), h = getRectHeight(&pWindow->clientRect);

	if (pWindow->fullScreen)
	{
		toggleFullscreen(pWindow);
		LOGF(LogLevel::eINFO, "SetWindowed() Position check: %s", wndValidateWindowPos(x, y) ? "SUCCESS" : "FAIL");
		LOGF(LogLevel::eINFO, "SetWindowed() Size check: %s", wndValidateWindowSize(w, h) ? "SUCCESS" : "FAIL");
	}
	if (pWindow->borderlessWindow)
	{
		toggleBorderless(pWindow, getRectWidth(&pWindow->clientRect), getRectHeight(&pWindow->clientRect));

		bool centered = pWindow->centered;
		pWindow->centered = false;
		LOGF(LogLevel::eINFO, "SetWindowed() Position check: %s", wndValidateWindowPos(x, y) ? "SUCCESS" : "FAIL");
		LOGF(LogLevel::eINFO, "SetWindowed() Size check: %s", wndValidateWindowSize(w, h) ? "SUCCESS" : "FAIL");
		pWindow->centered = centered;
	}
	pWindowRef->mWindowMode = WindowMode::WM_WINDOWED;
}

void wndSetFullscreen()
{
	WindowDesc* pWindow = pWindowRef;

	if (!pWindow->fullScreen)
	{
		toggleFullscreen(pWindow);
		pWindowRef->mWindowMode = WindowMode::WM_FULLSCREEN;
	}
}

void wndSetBorderless()
{
	WindowDesc* pWindow = pWindowRef;

	int clientWidthStart = (getRectWidth(&pWindow->windowedRect) - getRectWidth(&pWindow->clientRect)) >> 1,
		clientHeightStart = getRectHeight(&pWindow->windowedRect) - getRectHeight(&pWindow->clientRect) - clientWidthStart;
	int32_t x = pWindow->windowedRect.left + clientWidthStart, y = pWindow->windowedRect.top + clientHeightStart,
		w = getRectWidth(&pWindow->clientRect), h = getRectHeight(&pWindow->clientRect);

	if (pWindow->fullScreen)
	{
		toggleFullscreen(pWindow);
		pWindowRef->mWindowMode = WindowMode::WM_WINDOWED;
		LOGF(LogLevel::eINFO, "SetBorderless() Position check: %s", wndValidateWindowPos(x, y) ? "SUCCESS" : "FAIL");
		LOGF(LogLevel::eINFO, "SetBorderless() Size check: %s", wndValidateWindowSize(w, h) ? "SUCCESS" : "FAIL");
	}
	else
	{
		pWindowRef->mWindowMode = WindowMode::WM_BORDERLESS;
		toggleBorderless(pWindow, getRectWidth(&pWindow->clientRect), getRectHeight(&pWindow->clientRect));
		if (!pWindow->borderlessWindow)
			pWindowRef->mWindowMode = WindowMode::WM_WINDOWED;

		bool centered = pWindow->centered;
		pWindow->centered = false;
		LOGF(LogLevel::eINFO, "SetBorderless() Position check: %s", wndValidateWindowPos(x, y) ? "SUCCESS" : "FAIL");
		LOGF(LogLevel::eINFO, "SetBorderless() Size check: %s", wndValidateWindowSize(w, h) ? "SUCCESS" : "FAIL");
		pWindow->centered = centered;
	}
}

void wndMaximizeWindow()
{
	WindowDesc* pWindow = pWindowRef;

	maximizeWindow(pWindow); 
}

void wndMinimizeWindow()
{
	pWindowRef->mMinimizeRequested = true;
}

void wndHideWindow()
{
	WindowDesc* pWindow = pWindowRef;

	hideWindow(pWindow);
}

void wndShowWindow()
{
	WindowDesc* pWindow = pWindowRef;

	showWindow(pWindow); 
}

void wndUpdateResolution()
{
	uint32_t monitorCount = getMonitorCount();
	for (uint32_t i = 0; i < monitorCount; ++i)
	{
		if (pWindowRef->pCurRes[i] != pWindowRef->pLastRes[i])
		{
			MonitorDesc* monitor = getMonitor(i);

			int32_t resIndex = pWindowRef->pCurRes[i];
			setResolution(monitor, &monitor->resolutions[resIndex]);

			pWindowRef->pLastRes[i] = pWindowRef->pCurRes[i];
		}
	}
}

void wndMoveWindow()
{
	WindowDesc* pWindow = pWindowRef;

	wndSetWindowed();
	int clientWidthStart = (getRectWidth(&pWindow->windowedRect) - getRectWidth(&pWindow->clientRect)) >> 1,
		clientHeightStart = getRectHeight(&pWindow->windowedRect) - getRectHeight(&pWindow->clientRect) - clientWidthStart;
	RectDesc rectDesc{ pWindowRef->mWndX, pWindowRef->mWndY, pWindowRef->mWndX + pWindowRef->mWndW, pWindowRef->mWndY + pWindowRef->mWndH };
	setWindowRect(pWindow, &rectDesc);
	LOGF(
		LogLevel::eINFO, "MoveWindow() Position check: %s",
		wndValidateWindowPos(pWindowRef->mWndX + clientWidthStart, pWindowRef->mWndY + clientHeightStart) ? "SUCCESS" : "FAIL");
	LOGF(LogLevel::eINFO, "MoveWindow() Size check: %s", wndValidateWindowSize(pWindowRef->mWndW, pWindowRef->mWndH) ? "SUCCESS" : "FAIL");

	pWindowRef->mWndX = rectDesc.left;
	pWindowRef->mWndY = rectDesc.top;
	pWindowRef->mWndW = rectDesc.right - rectDesc.left;
	pWindowRef->mWndH = rectDesc.bottom - rectDesc.top;
}

void wndSetRecommendedWindowSize()
{
	WindowDesc* pWindow = pWindowRef;

	wndSetWindowed();

	RectDesc rect;
	getRecommendedResolution(&rect);

	setWindowRect(pWindow, &rect);

	pWindowRef->mWndX = rect.left;
	pWindowRef->mWndY = rect.top;
	pWindowRef->mWndW = rect.right - rect.left;
	pWindowRef->mWndH = rect.bottom - rect.top;
}

void wndHideCursor()
{
	pWindowRef->mCursorHidden = true;
	hideCursor();
}

void wndShowCursor()
{
	pWindowRef->mCursorHidden = false;
	showCursor(); 
}

void wndToggleClipCursor()
{
	pWindowRef->mCursorClipped = !pWindowRef->mCursorClipped;
#ifdef ENABLE_FORGE_INPUT
	setEnableCaptureInput(pWindowRef->mCursorClipped);
#endif
}

#if defined(_WINDOWS) || defined(__APPLE__) && !defined(TARGET_IOS) || (defined(__linux__) && !defined(__ANDROID__))
static void HideWindow()
{
	resetTimer(&gHideTimer);
	wndHideWindow();
}

static void HideCursor()
{
	resetTimer(&gHideTimer);
	wndHideCursor();
}
#endif

void platformInitWindowSystem(WindowDesc* pData)
{
	ASSERT(pWindowRef == NULL);

	RectDesc currentRes = pData->fullScreen ? pData->fullscreenRect : pData->clientRect;
	pData->mWndX = currentRes.left;
	pData->mWndY = currentRes.top;
	pData->mWndW = currentRes.right - currentRes.left;
	pData->mWndH = currentRes.bottom - currentRes.top;

	initTimer(&gHideTimer);
	pWindowRef = pData;
}

void platformExitWindowSystem()
{
	pWindowRef = NULL;
}

void platformUpdateWindowSystem()
{
	if (pWindowRef->hide || pWindowRef->mCursorHidden)
	{
		unsigned msec = getTimerMSec(&gHideTimer, false);
		if (msec >= 2000)
		{
			wndShowWindow();
			wndShowCursor();
			pWindowRef->mCursorHidden = false;
		}
	}

	pWindowRef->mCursorInsideWindow = isCursorInsideTrackingArea();

	if (pWindowRef->mMinimizeRequested)
	{
		minimizeWindow(pWindowRef);
		pWindowRef->mMinimizeRequested = false;
	}
}

void platformSetupWindowSystemUI(IApp* pApp)
{
#ifdef ENABLE_FORGE_UI
	float dpiScale;
	{
		float dpiScaleArray[2];
		getDpiScale(dpiScaleArray);
		dpiScale = dpiScaleArray[0];
	}

	vec2 UIPosition = { pApp->mSettings.mWidth * 0.775f, pApp->mSettings.mHeight * 0.01f };
	vec2 UIPanelSize = vec2(400.f, 750.f) / dpiScale;

	UIComponentDesc uiDesc = {};
	uiDesc.mStartPosition = UIPosition;
	uiDesc.mStartSize = UIPanelSize;
	uiDesc.mFontID = 0;
	uiDesc.mFontSize = 16.0f;
	uiCreateComponent("Window and Resolution Controls", &uiDesc, &pWindowControlsComponent);
	uiSetComponentFlags(pWindowControlsComponent, GUI_COMPONENT_FLAGS_START_COLLAPSED);

#if defined(_WINDOWS)
	strcpy(gPlatformName, "Windows");
#elif defined(__APPLE__) && !defined(TARGET_IOS)
	strcpy(gPlatformName, "MacOS");
#elif defined(__linux__) && !defined(__ANDROID__)
	strcpy(gPlatformName, "Linux");
#else
	strcpy(gPlatformName, "Unsupported");
#endif

	TextboxWidget Textbox;
	Textbox.pData = gPlatformName;
	Textbox.mLength = 64;
	REGISTER_LUA_WIDGET(uiCreateComponentWidget(pWindowControlsComponent, "Platform Name", &Textbox, WIDGET_TYPE_TEXTBOX));

#if defined(_WINDOWS) || defined(__APPLE__) && !defined(TARGET_IOS) || (defined(__linux__) && !defined(__ANDROID__))
	RadioButtonWidget rbWindowed;
	rbWindowed.pData = &pWindowRef->mWindowMode;
	rbWindowed.mRadioId = WM_WINDOWED;
	UIWidget* pWindowed = uiCreateComponentWidget(pWindowControlsComponent, "Windowed", &rbWindowed, WIDGET_TYPE_RADIO_BUTTON);
	uiSetWidgetOnEditedCallback(pWindowed, wndSetWindowed);
	uiSetWidgetDeferred(pWindowed, true);
	REGISTER_LUA_WIDGET(pWindowed);

	RadioButtonWidget rbFullscreen;
	rbFullscreen.pData = &pWindowRef->mWindowMode;
	rbFullscreen.mRadioId = WM_FULLSCREEN;
	UIWidget* pFullscreen = uiCreateComponentWidget(pWindowControlsComponent, "Fullscreen", &rbFullscreen, WIDGET_TYPE_RADIO_BUTTON);
	uiSetWidgetOnEditedCallback(pFullscreen, wndSetFullscreen);
	uiSetWidgetDeferred(pFullscreen, true);
	REGISTER_LUA_WIDGET(pFullscreen);

	RadioButtonWidget rbBorderless;
	rbBorderless.pData = &pWindowRef->mWindowMode;
	rbBorderless.mRadioId = WM_BORDERLESS;
	UIWidget* pBorderless = uiCreateComponentWidget(pWindowControlsComponent, "Borderless", &rbBorderless, WIDGET_TYPE_RADIO_BUTTON);
	uiSetWidgetOnEditedCallback(pBorderless, wndSetBorderless);
	uiSetWidgetDeferred(pBorderless, true);
	REGISTER_LUA_WIDGET(pBorderless);

	ButtonWidget bMaximize;
	UIWidget*     pMaximize = uiCreateComponentWidget(pWindowControlsComponent, "Maximize", &bMaximize, WIDGET_TYPE_BUTTON);
	uiSetWidgetOnEditedCallback(pMaximize, wndMaximizeWindow);
	uiSetWidgetDeferred(pMaximize, true);
	REGISTER_LUA_WIDGET(pMaximize);

	ButtonWidget bMinimize;
	UIWidget*     pMinimize = uiCreateComponentWidget(pWindowControlsComponent, "Minimize", &bMinimize, WIDGET_TYPE_BUTTON);
	uiSetWidgetOnEditedCallback(pMinimize, wndMinimizeWindow);
	uiSetWidgetDeferred(pMinimize, true);
	REGISTER_LUA_WIDGET(pMinimize);

	ButtonWidget bHide;
	UIWidget*     pHide = uiCreateComponentWidget(pWindowControlsComponent, "Hide for 2s", &bHide, WIDGET_TYPE_BUTTON);
	uiSetWidgetOnEditedCallback(pHide, HideWindow);
	REGISTER_LUA_WIDGET(pHide);

	CheckboxWidget rbCentered;
	rbCentered.pData = &(pWindowRef->centered);
	REGISTER_LUA_WIDGET(uiCreateComponentWidget(pWindowControlsComponent, "Toggle Window Centered", &rbCentered, WIDGET_TYPE_CHECKBOX));

	RectDesc recRes;
	getRecommendedResolution(&recRes);

	uint32_t recWidth = recRes.right - recRes.left;
	uint32_t recHeight = recRes.bottom - recRes.top;

	SliderIntWidget setRectSliderX;
	setRectSliderX.pData = &pWindowRef->mWndX;
	setRectSliderX.mMin = 0;
	setRectSliderX.mMax = recWidth;
	REGISTER_LUA_WIDGET(uiCreateComponentWidget(pWindowControlsComponent, "Window X Offset", &setRectSliderX, WIDGET_TYPE_SLIDER_INT));

	SliderIntWidget setRectSliderY;
	setRectSliderY.pData = &pWindowRef->mWndY;
	setRectSliderY.mMin = 0;
	setRectSliderY.mMax = recHeight;
	REGISTER_LUA_WIDGET(uiCreateComponentWidget(pWindowControlsComponent, "Window Y Offset", &setRectSliderY, WIDGET_TYPE_SLIDER_INT));

	SliderIntWidget setRectSliderW;
	setRectSliderW.pData = &pWindowRef->mWndW;
	setRectSliderW.mMin = 144;
	setRectSliderW.mMax = recWidth;
	REGISTER_LUA_WIDGET(uiCreateComponentWidget(pWindowControlsComponent, "Window Width", &setRectSliderW, WIDGET_TYPE_SLIDER_INT));

	SliderIntWidget setRectSliderH;
	setRectSliderH.pData = &pWindowRef->mWndH;
	setRectSliderH.mMin = 144;
	setRectSliderH.mMax = recHeight;
	REGISTER_LUA_WIDGET(uiCreateComponentWidget(pWindowControlsComponent, "Window Height", &setRectSliderH, WIDGET_TYPE_SLIDER_INT));

	ButtonWidget bSetRect;
	UIWidget* pSetRect = uiCreateComponentWidget(pWindowControlsComponent, "Set window rectangle", &bSetRect, WIDGET_TYPE_BUTTON);
	uiSetWidgetOnEditedCallback(pSetRect, wndMoveWindow);
	uiSetWidgetDeferred(pSetRect, true);
	REGISTER_LUA_WIDGET(pSetRect);

	ButtonWidget bRecWndSize;
	UIWidget* pRecWndSize = uiCreateComponentWidget(pWindowControlsComponent, "Set recommended window rectangle", &bRecWndSize, WIDGET_TYPE_BUTTON);
	uiSetWidgetOnEditedCallback(pRecWndSize, wndSetRecommendedWindowSize);
	uiSetWidgetDeferred(pRecWndSize, true);
	REGISTER_LUA_WIDGET(pRecWndSize);

	uint32_t numMonitors = getMonitorCount();

	char label[64];
	sprintf(label, "Number of displays: ");

	char monitors[10];
	sprintf(monitors, "%u", numMonitors);
	strcat(label, monitors);

	LabelWidget labelWidget;
	REGISTER_LUA_WIDGET(uiCreateComponentWidget(pWindowControlsComponent, label, &labelWidget, WIDGET_TYPE_LABEL));

	for (uint32_t i = 0; i < numMonitors; ++i)
	{
		MonitorDesc* monitor = getMonitor(i);

		char publicDisplayName[128];
#if defined(_WINDOWS) || defined(XBOX)    // Win platform uses wide chars
		if (128 == wcstombs(publicDisplayName, monitor->publicDisplayName, sizeof(publicDisplayName)))
			publicDisplayName[127] = '\0';
#elif !defined(TARGET_IOS)
		strcpy(publicDisplayName, monitor->publicDisplayName);
#endif
		char monitorLabel[128];
		sprintf(monitorLabel, "%s", publicDisplayName);
		strcat(monitorLabel, " (");

		char buffer[10];
		sprintf(buffer, "%u", monitor->physicalSize[0]);
		strcat(monitorLabel, buffer);
		strcat(monitorLabel, "x");

		sprintf(buffer, "%u", monitor->physicalSize[1]);
		strcat(monitorLabel, buffer);
		strcat(monitorLabel, " mm; ");

		sprintf(buffer, "%u", monitor->dpi[0]);
		strcat(monitorLabel, buffer);
		strcat(monitorLabel, " dpi; ");

		sprintf(buffer, "%u", monitor->resolutionCount);
		strcat(monitorLabel, buffer);
		strcat(monitorLabel, " resolutions)");

		CollapsingHeaderWidget monitorHeader;

		for (uint32_t j = 0; j < monitor->resolutionCount; ++j)
		{
			Resolution res = monitor->resolutions[j];

			char strRes[64];
			sprintf(strRes, "%u", res.mWidth);
			strcat(strRes, "x");

			char height[10];
			sprintf(height, "%u", res.mHeight);
			strcat(strRes, height);

			if (monitor->defaultResolution.mWidth == res.mWidth && monitor->defaultResolution.mHeight == res.mHeight)
			{
				strcat(strRes, " (native)");
				pWindowRef->pCurRes[i] = j;
				pWindowRef->pLastRes[i] = j;
			}

			RadioButtonWidget rbRes;
			rbRes.pData = &pWindowRef->pCurRes[i];
			rbRes.mRadioId = j;
			UIWidget* pRbRes = uiCreateCollapsingHeaderSubWidget(&monitorHeader, strRes, &rbRes, WIDGET_TYPE_RADIO_BUTTON);
			uiSetWidgetOnEditedCallback(pRbRes, wndUpdateResolution);
			uiSetWidgetDeferred(pRbRes, false);
		}

		uiCreateComponentWidget(pWindowControlsComponent, monitorLabel, &monitorHeader, WIDGET_TYPE_COLLAPSING_HEADER); 
	}

	CollapsingHeaderWidget InputCotrolsWidget;

	ButtonWidget bHideCursor;
	UIWidget* pHideCursor = uiCreateCollapsingHeaderSubWidget(&InputCotrolsWidget, "Hide Cursor for 2s", &bHideCursor, WIDGET_TYPE_BUTTON);
	uiSetWidgetOnEditedCallback(pHideCursor, HideCursor);

	LabelWidget lCursorInWindow;
	uiCreateCollapsingHeaderSubWidget(&InputCotrolsWidget, "Cursor inside window?", &lCursorInWindow, WIDGET_TYPE_LABEL);

	RadioButtonWidget rCursorInsideRectFalse;
	rCursorInsideRectFalse.pData = &pWindowRef->mCursorInsideWindow;
	rCursorInsideRectFalse.mRadioId = 0;
	uiCreateCollapsingHeaderSubWidget(&InputCotrolsWidget, "No", &rCursorInsideRectFalse, WIDGET_TYPE_RADIO_BUTTON);

	RadioButtonWidget rCursorInsideRectTrue;
	rCursorInsideRectTrue.pData = &pWindowRef->mCursorInsideWindow;
	rCursorInsideRectTrue.mRadioId = 1;
	uiCreateCollapsingHeaderSubWidget(&InputCotrolsWidget, "Yes", &rCursorInsideRectTrue, WIDGET_TYPE_RADIO_BUTTON);

	ButtonWidget bClipCursor;
	UIWidget* pClipCursor = uiCreateCollapsingHeaderSubWidget(&InputCotrolsWidget, "Clip Cursor to Window", &bClipCursor, WIDGET_TYPE_BUTTON);
	uiSetWidgetOnEditedCallback(pClipCursor, wndToggleClipCursor);

	REGISTER_LUA_WIDGET(uiCreateComponentWidget(pWindowControlsComponent, "Cursor", &InputCotrolsWidget, WIDGET_TYPE_COLLAPSING_HEADER));
#endif
#endif
}

void platformToggleWindowSystemUI(bool active)
{
#ifdef ENABLE_FORGE_UI
	uiSetComponentActive(pWindowControlsComponent, active);
#endif
}

