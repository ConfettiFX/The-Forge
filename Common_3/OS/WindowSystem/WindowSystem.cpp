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

#include "../../Application/Interfaces/IApp.h"
#include "../../Application/Interfaces/IInput.h"
#include "../../Application/Interfaces/IScreenshot.h"
#include "../../Application/Interfaces/IUI.h"
#include "../../Game/Interfaces/IScripting.h"
#include "../../OS/Interfaces/IOperatingSystem.h"
#include "../../Utilities/Interfaces/ITime.h"

static WindowDesc*  pWindowRef = NULL;
static UIComponent* pWindowControlsComponent = NULL;

static char    gPlatformNameBuffer[64];
static bstring gPlatformName = bemptyfromarr(gPlatformNameBuffer);

#if defined(AUTOMATED_TESTING)
char           gAppName[64];
static char    gScriptNameBuffer[64];
static bstring gScriptName = bemptyfromarr(gScriptNameBuffer);
#endif

static bool wndValidateWindowPos(int32_t x, int32_t y)
{
    WindowDesc* winDesc = pWindowRef;
    int         clientWidthStart = (getRectWidth(&winDesc->windowedRect) - getRectWidth(&winDesc->clientRect)) >> 1;
    int         clientHeightStart = getRectHeight(&winDesc->windowedRect) - getRectHeight(&winDesc->clientRect) - clientWidthStart;

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
    RectDesc windowRect = pWindowRef->windowedRect;
    if ((abs(getRectWidth(&windowRect) - width) > 1) || (abs(getRectHeight(&windowRect) - height) > 1))
        return false;
    return true;
}

void wndSetWindowed(void* pUserData)
{
    UNREF_PARAM(pUserData);
    setWindowed(pWindowRef, getRectWidth(&pWindowRef->clientRect), getRectHeight(&pWindowRef->clientRect));
}

void wndSetFullscreen(void* pUserData)
{
    UNREF_PARAM(pUserData);
    setFullscreen(pWindowRef);
}

void wndSetBorderless(void* pUserData)
{
    UNREF_PARAM(pUserData);
    setBorderless(pWindowRef, getRectWidth(&pWindowRef->clientRect), getRectHeight(&pWindowRef->clientRect));
}

void wndMaximizeWindow(void* pUserData)
{
    UNREF_PARAM(pUserData);
    WindowDesc* pWindow = pWindowRef;

    maximizeWindow(pWindow);
}

void wndMinimizeWindow(void* pUserData)
{
    UNREF_PARAM(pUserData);
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

void wndUpdateResolution(void* pUserData)
{
    UNREF_PARAM(pUserData);
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

void wndMoveWindow(void* pUserData)
{
    WindowDesc* pWindow = pWindowRef;

    wndSetWindowed(pUserData);
    int clientWidthStart = (getRectWidth(&pWindow->windowedRect) - getRectWidth(&pWindow->clientRect)) >> 1,
        clientHeightStart = getRectHeight(&pWindow->windowedRect) - getRectHeight(&pWindow->clientRect) - clientWidthStart;
    RectDesc rectDesc{ pWindowRef->mWndX, pWindowRef->mWndY, pWindowRef->mWndX + pWindowRef->mWndW, pWindowRef->mWndY + pWindowRef->mWndH };
    setWindowRect(pWindow, &rectDesc);
    LOGF(LogLevel::eINFO, "MoveWindow() Position check: %s",
         wndValidateWindowPos(pWindowRef->mWndX + clientWidthStart, pWindowRef->mWndY + clientHeightStart) ? "SUCCESS" : "FAIL");
    LOGF(LogLevel::eINFO, "MoveWindow() Size check: %s", wndValidateWindowSize(pWindowRef->mWndW, pWindowRef->mWndH) ? "SUCCESS" : "FAIL");
}

void wndSetRecommendedWindowSize(void* pUserData)
{
    WindowDesc* pWindow = pWindowRef;

    wndSetWindowed(pUserData);

    RectDesc rect;
    getRecommendedWindowRect(pWindowRef, &rect);

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

void wndUpdateCaptureCursor(void* pUserData)
{
    UNREF_PARAM(pUserData);
#ifdef ENABLE_FORGE_INPUT
    setEnableCaptureInput(pWindowRef->mCursorCaptured);
#endif
}

#if defined(AUTOMATED_TESTING)
void wndTakeScreenshot(void* pUserData)
{
    UNREF_PARAM(pUserData);
    char screenShotName[256];
    snprintf(screenShotName, sizeof(screenShotName), "%s_%s", gAppName, gScriptNameBuffer);

    setCaptureScreenshot(screenShotName);
}
#endif

void platformInitWindowSystem(WindowDesc* pData)
{
    ASSERT(pWindowRef == NULL);

    RectDesc currentRes = pData->fullScreen ? pData->fullscreenRect : pData->windowedRect;
    pData->mWndX = currentRes.left;
    pData->mWndY = currentRes.top;
    pData->mWndW = currentRes.right - currentRes.left;
    pData->mWndH = currentRes.bottom - currentRes.top;

    pWindowRef = pData;

#if WINDOW_DETAILS
    pWindowRef->pWindowedRectLabel = bempty();
    pWindowRef->pFullscreenRectLabel = bempty();
    pWindowRef->pClientRectLabel = bempty();
    pWindowRef->pWndLabel = bempty();
    pWindowRef->pFullscreenLabel = bempty();
    pWindowRef->pCursorCapturedLabel = bempty();
    pWindowRef->pIconifiedLabel = bempty();
    pWindowRef->pMaximizedLabel = bempty();
    pWindowRef->pMinimizedLabel = bempty();
    pWindowRef->pNoResizeFrameLabel = bempty();
    pWindowRef->pBorderlessWindowLabel = bempty();
    pWindowRef->pOverrideDefaultPositionLabel = bempty();
    pWindowRef->pCenteredLabel = bempty();
    pWindowRef->pForceLowDPILabel = bempty();
    pWindowRef->pWindowModeLabel = bempty();
#endif
}

void platformExitWindowSystem()
{
#if WINDOW_DETAILS
    bdestroy(&pWindowRef->pWindowedRectLabel);
    bdestroy(&pWindowRef->pFullscreenRectLabel);
    bdestroy(&pWindowRef->pClientRectLabel);
    bdestroy(&pWindowRef->pWndLabel);
    bdestroy(&pWindowRef->pFullscreenLabel);
    bdestroy(&pWindowRef->pCursorCapturedLabel);
    bdestroy(&pWindowRef->pIconifiedLabel);
    bdestroy(&pWindowRef->pMaximizedLabel);
    bdestroy(&pWindowRef->pMinimizedLabel);
    bdestroy(&pWindowRef->pNoResizeFrameLabel);
    bdestroy(&pWindowRef->pBorderlessWindowLabel);
    bdestroy(&pWindowRef->pOverrideDefaultPositionLabel);
    bdestroy(&pWindowRef->pCenteredLabel);
    bdestroy(&pWindowRef->pForceLowDPILabel);
    bdestroy(&pWindowRef->pWindowModeLabel);
#endif
    pWindowRef = NULL;
}

void platformUpdateWindowSystem()
{
    pWindowRef->mCursorInsideWindow = isCursorInsideTrackingArea();

    if (pWindowRef->mMinimizeRequested)
    {
        minimizeWindow(pWindowRef);
        pWindowRef->mMinimizeRequested = false;
    }

#if WINDOW_DETAILS
    bdestroy(&pWindowRef->pWindowedRectLabel);
    bformat(&pWindowRef->pWindowedRectLabel, "WindowedRect L: %d, T: %d, R: %d, B: %d", pWindowRef->windowedRect.left,
            pWindowRef->windowedRect.top, pWindowRef->windowedRect.right, pWindowRef->windowedRect.bottom);
    bdestroy(&pWindowRef->pFullscreenRectLabel);
    bformat(&pWindowRef->pFullscreenRectLabel, "FullscreenRect L: %d, T: %d, R: %d, B: %d", pWindowRef->fullscreenRect.left,
            pWindowRef->fullscreenRect.top, pWindowRef->fullscreenRect.right, pWindowRef->fullscreenRect.bottom);
    bdestroy(&pWindowRef->pClientRectLabel);
    bformat(&pWindowRef->pClientRectLabel, "ClientRect L: %d, T: %d, R: %d, B: %d", pWindowRef->clientRect.left, pWindowRef->clientRect.top,
            pWindowRef->clientRect.right, pWindowRef->clientRect.bottom);
    bdestroy(&pWindowRef->pWndLabel);
    bformat(&pWindowRef->pWndLabel, "Wnd X: %d, Y: %d, W: %d, H: %d", pWindowRef->mWndX, pWindowRef->mWndY, pWindowRef->mWndW,
            pWindowRef->mWndH);
    bdestroy(&pWindowRef->pFullscreenLabel);
    bformat(&pWindowRef->pFullscreenLabel, "Fullscreen: %s", pWindowRef->fullScreen ? "True" : "False");
    bdestroy(&pWindowRef->pCursorCapturedLabel);
    bformat(&pWindowRef->pCursorCapturedLabel, "CursorCaptured: %s", pWindowRef->cursorCaptured ? "True" : "False");
    bdestroy(&pWindowRef->pIconifiedLabel);
    bformat(&pWindowRef->pIconifiedLabel, "Iconified: %s", pWindowRef->iconified ? "True" : "False");
    bdestroy(&pWindowRef->pMaximizedLabel);
    bformat(&pWindowRef->pMaximizedLabel, "Maximized: %s", pWindowRef->maximized ? "True" : "False");
    bdestroy(&pWindowRef->pMinimizedLabel);
    bformat(&pWindowRef->pMinimizedLabel, "Minimized: %s", pWindowRef->minimized ? "True" : "False");
    bdestroy(&pWindowRef->pNoResizeFrameLabel);
    bformat(&pWindowRef->pNoResizeFrameLabel, "NoResizeFrame: %s", pWindowRef->noresizeFrame ? "True" : "False");
    bdestroy(&pWindowRef->pBorderlessWindowLabel);
    bformat(&pWindowRef->pBorderlessWindowLabel, "BorderlessWindow: %s", pWindowRef->borderlessWindow ? "True" : "False");
    bdestroy(&pWindowRef->pOverrideDefaultPositionLabel);
    bformat(&pWindowRef->pOverrideDefaultPositionLabel, "OverrideDefaultPosition: %s",
            pWindowRef->overrideDefaultPosition ? "True" : "False");
    bdestroy(&pWindowRef->pCenteredLabel);
    bformat(&pWindowRef->pCenteredLabel, "Centered: %s", pWindowRef->centered ? "True" : "False");
    bdestroy(&pWindowRef->pForceLowDPILabel);
    bformat(&pWindowRef->pForceLowDPILabel, "ForceLowDPI: %s", pWindowRef->forceLowDPI ? "True" : "False");
    bdestroy(&pWindowRef->pWindowModeLabel);
    bformat(&pWindowRef->pWindowModeLabel, "WindowMode: %s",
            pWindowRef->mWindowMode == WM_BORDERLESS ? "Borderless"
                                                     : (pWindowRef->mWindowMode == WM_FULLSCREEN ? "Fullscreen" : "Windowed"));
#endif
}

void platformSetupWindowSystemUI(IApp* pApp)
{
#ifdef ENABLE_FORGE_UI
    float dpiScale;
    {
        float dpiScaleArray[2];
        getMonitorDpiScale(pApp->mSettings.mMonitorIndex, dpiScaleArray);
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
    bassignliteral(&gPlatformName, "Windows");
#elif defined(__APPLE__) && !defined(TARGET_IOS)
    bassignliteral(&gPlatformName, "MacOS");
#elif defined(__linux__) && !defined(__ANDROID__)
    bassignliteral(&gPlatformName, "Linux");
#else
    bassignliteral(&gPlatformName, "Unsupported");
#endif

    TextboxWidget Textbox;
    Textbox.pText = &gPlatformName;
    REGISTER_LUA_WIDGET(uiCreateComponentWidget(pWindowControlsComponent, "Platform Name", &Textbox, WIDGET_TYPE_TEXTBOX));

#if defined(_WINDOWS) || defined(__APPLE__) && !defined(TARGET_IOS) || (defined(__linux__) && !defined(__ANDROID__))
    RadioButtonWidget rbWindowed;
    rbWindowed.pData = &pWindowRef->mWindowMode;
    rbWindowed.mRadioId = WM_WINDOWED;
    UIWidget* pWindowed = uiCreateComponentWidget(pWindowControlsComponent, "Windowed", &rbWindowed, WIDGET_TYPE_RADIO_BUTTON);
    uiSetWidgetOnEditedCallback(pWindowed, nullptr, wndSetWindowed);
    uiSetWidgetDeferred(pWindowed, true);
    REGISTER_LUA_WIDGET(pWindowed);

    RadioButtonWidget rbFullscreen;
    rbFullscreen.pData = &pWindowRef->mWindowMode;
    rbFullscreen.mRadioId = WM_FULLSCREEN;
    UIWidget* pFullscreen = uiCreateComponentWidget(pWindowControlsComponent, "Fullscreen", &rbFullscreen, WIDGET_TYPE_RADIO_BUTTON);
    uiSetWidgetOnEditedCallback(pFullscreen, nullptr, wndSetFullscreen);
    uiSetWidgetDeferred(pFullscreen, true);
    REGISTER_LUA_WIDGET(pFullscreen);

    RadioButtonWidget rbBorderless;
    rbBorderless.pData = &pWindowRef->mWindowMode;
    rbBorderless.mRadioId = WM_BORDERLESS;
    UIWidget* pBorderless = uiCreateComponentWidget(pWindowControlsComponent, "Borderless", &rbBorderless, WIDGET_TYPE_RADIO_BUTTON);
    uiSetWidgetOnEditedCallback(pBorderless, nullptr, wndSetBorderless);
    uiSetWidgetDeferred(pBorderless, true);
    REGISTER_LUA_WIDGET(pBorderless);

    ButtonWidget bMaximize;
    UIWidget*    pMaximize = uiCreateComponentWidget(pWindowControlsComponent, "Maximize", &bMaximize, WIDGET_TYPE_BUTTON);
    uiSetWidgetOnEditedCallback(pMaximize, nullptr, wndMaximizeWindow);
    uiSetWidgetDeferred(pMaximize, true);
    REGISTER_LUA_WIDGET(pMaximize);

    ButtonWidget bMinimize;
    UIWidget*    pMinimize = uiCreateComponentWidget(pWindowControlsComponent, "Minimize", &bMinimize, WIDGET_TYPE_BUTTON);
    uiSetWidgetOnEditedCallback(pMinimize, nullptr, wndMinimizeWindow);
    uiSetWidgetDeferred(pMinimize, true);
    REGISTER_LUA_WIDGET(pMinimize);

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
    setRectSliderW.mMax = getRectWidth(&pWindowRef->fullscreenRect);
    REGISTER_LUA_WIDGET(uiCreateComponentWidget(pWindowControlsComponent, "Window Width", &setRectSliderW, WIDGET_TYPE_SLIDER_INT));

    SliderIntWidget setRectSliderH;
    setRectSliderH.pData = &pWindowRef->mWndH;
    setRectSliderH.mMin = 144;
    setRectSliderH.mMax = getRectHeight(&pWindowRef->fullscreenRect);
    REGISTER_LUA_WIDGET(uiCreateComponentWidget(pWindowControlsComponent, "Window Height", &setRectSliderH, WIDGET_TYPE_SLIDER_INT));

    ButtonWidget bSetRect;
    UIWidget*    pSetRect = uiCreateComponentWidget(pWindowControlsComponent, "Set window rectangle", &bSetRect, WIDGET_TYPE_BUTTON);
    uiSetWidgetOnEditedCallback(pSetRect, nullptr, wndMoveWindow);
    uiSetWidgetDeferred(pSetRect, true);
    REGISTER_LUA_WIDGET(pSetRect);

    ButtonWidget bRecWndSize;
    UIWidget*    pRecWndSize =
        uiCreateComponentWidget(pWindowControlsComponent, "Set recommended window rectangle", &bRecWndSize, WIDGET_TYPE_BUTTON);
    uiSetWidgetOnEditedCallback(pRecWndSize, nullptr, wndSetRecommendedWindowSize);
    uiSetWidgetDeferred(pRecWndSize, true);
    REGISTER_LUA_WIDGET(pRecWndSize);

#if WINDOW_DETAILS
    uint      windowDetailsWidgetCount = 0;
    UIWidget  windowDetailsWidgets[32];
    UIWidget* windowDetailsWidgetGroup[32] = { 0 };

    TextboxWidget WindowedRectWidget = {};
    WindowedRectWidget.pText = &pWindowRef->pWindowedRectLabel;
    windowDetailsWidgets[windowDetailsWidgetCount].mType = WIDGET_TYPE_TEXTBOX;
    strcpy(windowDetailsWidgets[windowDetailsWidgetCount].mLabel, "WindowedRect");
    windowDetailsWidgets[windowDetailsWidgetCount].pWidget = &WindowedRectWidget;
    windowDetailsWidgetGroup[windowDetailsWidgetCount] = &windowDetailsWidgets[windowDetailsWidgetCount];
    ++windowDetailsWidgetCount;

    TextboxWidget FullscreenRectWidget = {};
    FullscreenRectWidget.pText = &pWindowRef->pFullscreenRectLabel;
    windowDetailsWidgets[windowDetailsWidgetCount].mType = WIDGET_TYPE_TEXTBOX;
    strcpy(windowDetailsWidgets[windowDetailsWidgetCount].mLabel, "FullscreenRect");
    windowDetailsWidgets[windowDetailsWidgetCount].pWidget = &FullscreenRectWidget;
    windowDetailsWidgetGroup[windowDetailsWidgetCount] = &windowDetailsWidgets[windowDetailsWidgetCount];
    ++windowDetailsWidgetCount;

    TextboxWidget ClientRectWidget = {};
    ClientRectWidget.pText = &pWindowRef->pClientRectLabel;
    windowDetailsWidgets[windowDetailsWidgetCount].mType = WIDGET_TYPE_TEXTBOX;
    strcpy(windowDetailsWidgets[windowDetailsWidgetCount].mLabel, "ClientRect");
    windowDetailsWidgets[windowDetailsWidgetCount].pWidget = &ClientRectWidget;
    windowDetailsWidgetGroup[windowDetailsWidgetCount] = &windowDetailsWidgets[windowDetailsWidgetCount];
    ++windowDetailsWidgetCount;

    TextboxWidget WndWidget = {};
    WndWidget.pText = &pWindowRef->pWndLabel;
    windowDetailsWidgets[windowDetailsWidgetCount].mType = WIDGET_TYPE_TEXTBOX;
    strcpy(windowDetailsWidgets[windowDetailsWidgetCount].mLabel, "Wnd");
    windowDetailsWidgets[windowDetailsWidgetCount].pWidget = &WndWidget;
    windowDetailsWidgetGroup[windowDetailsWidgetCount] = &windowDetailsWidgets[windowDetailsWidgetCount];
    ++windowDetailsWidgetCount;

    TextboxWidget FullscreenWidget = {};
    FullscreenWidget.pText = &pWindowRef->pFullscreenLabel;
    windowDetailsWidgets[windowDetailsWidgetCount].mType = WIDGET_TYPE_TEXTBOX;
    strcpy(windowDetailsWidgets[windowDetailsWidgetCount].mLabel, "Fullscreen");
    windowDetailsWidgets[windowDetailsWidgetCount].pWidget = &FullscreenWidget;
    windowDetailsWidgetGroup[windowDetailsWidgetCount] = &windowDetailsWidgets[windowDetailsWidgetCount];
    ++windowDetailsWidgetCount;

    TextboxWidget CursorCapturedWidget = {};
    CursorCapturedWidget.pText = &pWindowRef->pCursorCapturedLabel;
    windowDetailsWidgets[windowDetailsWidgetCount].mType = WIDGET_TYPE_TEXTBOX;
    strcpy(windowDetailsWidgets[windowDetailsWidgetCount].mLabel, "CursorCaptured");
    windowDetailsWidgets[windowDetailsWidgetCount].pWidget = &CursorCapturedWidget;
    windowDetailsWidgetGroup[windowDetailsWidgetCount] = &windowDetailsWidgets[windowDetailsWidgetCount];
    ++windowDetailsWidgetCount;

    TextboxWidget IconifiedWidget = {};
    IconifiedWidget.pText = &pWindowRef->pIconifiedLabel;
    windowDetailsWidgets[windowDetailsWidgetCount].mType = WIDGET_TYPE_TEXTBOX;
    strcpy(windowDetailsWidgets[windowDetailsWidgetCount].mLabel, "Iconified");
    windowDetailsWidgets[windowDetailsWidgetCount].pWidget = &IconifiedWidget;
    windowDetailsWidgetGroup[windowDetailsWidgetCount] = &windowDetailsWidgets[windowDetailsWidgetCount];
    ++windowDetailsWidgetCount;

    TextboxWidget MaximizedWidget = {};
    MaximizedWidget.pText = &pWindowRef->pMaximizedLabel;
    windowDetailsWidgets[windowDetailsWidgetCount].mType = WIDGET_TYPE_TEXTBOX;
    strcpy(windowDetailsWidgets[windowDetailsWidgetCount].mLabel, "Maximized");
    windowDetailsWidgets[windowDetailsWidgetCount].pWidget = &MaximizedWidget;
    windowDetailsWidgetGroup[windowDetailsWidgetCount] = &windowDetailsWidgets[windowDetailsWidgetCount];
    ++windowDetailsWidgetCount;

    TextboxWidget MinimizedWidget = {};
    MinimizedWidget.pText = &pWindowRef->pMinimizedLabel;
    windowDetailsWidgets[windowDetailsWidgetCount].mType = WIDGET_TYPE_TEXTBOX;
    strcpy(windowDetailsWidgets[windowDetailsWidgetCount].mLabel, "Minimized");
    windowDetailsWidgets[windowDetailsWidgetCount].pWidget = &MinimizedWidget;
    windowDetailsWidgetGroup[windowDetailsWidgetCount] = &windowDetailsWidgets[windowDetailsWidgetCount];
    ++windowDetailsWidgetCount;

    TextboxWidget NoResizeFrameWidget = {};
    NoResizeFrameWidget.pText = &pWindowRef->pNoResizeFrameLabel;
    windowDetailsWidgets[windowDetailsWidgetCount].mType = WIDGET_TYPE_TEXTBOX;
    strcpy(windowDetailsWidgets[windowDetailsWidgetCount].mLabel, "NoResizeFrame");
    windowDetailsWidgets[windowDetailsWidgetCount].pWidget = &NoResizeFrameWidget;
    windowDetailsWidgetGroup[windowDetailsWidgetCount] = &windowDetailsWidgets[windowDetailsWidgetCount];
    ++windowDetailsWidgetCount;

    TextboxWidget BorderlessWindowWidget = {};
    BorderlessWindowWidget.pText = &pWindowRef->pBorderlessWindowLabel;
    windowDetailsWidgets[windowDetailsWidgetCount].mType = WIDGET_TYPE_TEXTBOX;
    strcpy(windowDetailsWidgets[windowDetailsWidgetCount].mLabel, "BorderlessWindow");
    windowDetailsWidgets[windowDetailsWidgetCount].pWidget = &BorderlessWindowWidget;
    windowDetailsWidgetGroup[windowDetailsWidgetCount] = &windowDetailsWidgets[windowDetailsWidgetCount];
    ++windowDetailsWidgetCount;

    TextboxWidget OverrideDefaultPositionWidget = {};
    OverrideDefaultPositionWidget.pText = &pWindowRef->pOverrideDefaultPositionLabel;
    windowDetailsWidgets[windowDetailsWidgetCount].mType = WIDGET_TYPE_TEXTBOX;
    strcpy(windowDetailsWidgets[windowDetailsWidgetCount].mLabel, "OverrideDefaultPosition");
    windowDetailsWidgets[windowDetailsWidgetCount].pWidget = &OverrideDefaultPositionWidget;
    windowDetailsWidgetGroup[windowDetailsWidgetCount] = &windowDetailsWidgets[windowDetailsWidgetCount];
    ++windowDetailsWidgetCount;

    TextboxWidget CenteredWidget = {};
    CenteredWidget.pText = &pWindowRef->pCenteredLabel;
    windowDetailsWidgets[windowDetailsWidgetCount].mType = WIDGET_TYPE_TEXTBOX;
    strcpy(windowDetailsWidgets[windowDetailsWidgetCount].mLabel, "Centered");
    windowDetailsWidgets[windowDetailsWidgetCount].pWidget = &CenteredWidget;
    windowDetailsWidgetGroup[windowDetailsWidgetCount] = &windowDetailsWidgets[windowDetailsWidgetCount];
    ++windowDetailsWidgetCount;

    TextboxWidget ForceLowDPIWidget = {};
    ForceLowDPIWidget.pText = &pWindowRef->pForceLowDPILabel;
    windowDetailsWidgets[windowDetailsWidgetCount].mType = WIDGET_TYPE_TEXTBOX;
    strcpy(windowDetailsWidgets[windowDetailsWidgetCount].mLabel, "ForceLowDPI");
    windowDetailsWidgets[windowDetailsWidgetCount].pWidget = &ForceLowDPIWidget;
    windowDetailsWidgetGroup[windowDetailsWidgetCount] = &windowDetailsWidgets[windowDetailsWidgetCount];
    ++windowDetailsWidgetCount;

    TextboxWidget WindowModeWidget = {};
    WindowModeWidget.pText = &pWindowRef->pWindowModeLabel;
    windowDetailsWidgets[windowDetailsWidgetCount].mType = WIDGET_TYPE_TEXTBOX;
    strcpy(windowDetailsWidgets[windowDetailsWidgetCount].mLabel, "WindowMode");
    windowDetailsWidgets[windowDetailsWidgetCount].pWidget = &WindowModeWidget;
    windowDetailsWidgetGroup[windowDetailsWidgetCount] = &windowDetailsWidgets[windowDetailsWidgetCount];
    ++windowDetailsWidgetCount;

    CollapsingHeaderWidget windowDetailsHeader;
    windowDetailsHeader.mWidgetsCount = windowDetailsWidgetCount;
    windowDetailsHeader.pGroupedWidgets = windowDetailsWidgetGroup;
    uiCreateComponentWidget(pWindowControlsComponent, "Window Details", &windowDetailsHeader, WIDGET_TYPE_COLLAPSING_HEADER);
#endif

    uint32_t numMonitors = getMonitorCount();

    char label[64];
    snprintf(label, 64, "Number of displays: ");

    char monitors[10];
    snprintf(monitors, 10, "%u", numMonitors);
    strcat(label, monitors);

    LabelWidget labelWidget;
    REGISTER_LUA_WIDGET(uiCreateComponentWidget(pWindowControlsComponent, label, &labelWidget, WIDGET_TYPE_LABEL));

    for (uint32_t i = 0; i < numMonitors; ++i)
    {
        MonitorDesc* monitor = getMonitor(i);

        char publicDisplayName[128];
#if defined(_WINDOWS) || defined(XBOX) // Win platform uses wide chars
        if (128 == wcstombs(publicDisplayName, monitor->publicDisplayName, sizeof(publicDisplayName)))
            publicDisplayName[127] = '\0';
#elif !defined(TARGET_IOS)
        strcpy(publicDisplayName, monitor->publicDisplayName);
#endif
        char monitorLabel[128];
        snprintf(monitorLabel, 128, "%s", publicDisplayName);
        strcat(monitorLabel, " (");

        char buffer[10];
        snprintf(buffer, 10, "%u", monitor->physicalSize[0]);
        strcat(monitorLabel, buffer);
        strcat(monitorLabel, "x");

        snprintf(buffer, 10, "%u", monitor->physicalSize[1]);
        strcat(monitorLabel, buffer);
        strcat(monitorLabel, " mm; ");

        snprintf(buffer, 10, "%u", monitor->dpi[0]);
        strcat(monitorLabel, buffer);
        strcat(monitorLabel, " dpi; ");

        snprintf(buffer, 10, "%u", (unsigned)arrlen(monitor->resolutions));
        strcat(monitorLabel, buffer);
        strcat(monitorLabel, " resolutions)");

        UIWidget**         monitorWidgets = (UIWidget**)tf_malloc(arrlen(monitor->resolutions) * sizeof(UIWidget*));
        UIWidget*          monitorWidgetsBases = (UIWidget*)tf_malloc(arrlen(monitor->resolutions) * sizeof(UIWidget));
        RadioButtonWidget* radioWidgets = (RadioButtonWidget*)tf_malloc(arrlen(monitor->resolutions) * sizeof(RadioButtonWidget));

        CollapsingHeaderWidget monitorHeader;
        monitorHeader.pGroupedWidgets = monitorWidgets;
        monitorHeader.mWidgetsCount = (uint32_t)arrlen(monitor->resolutions);

        for (ptrdiff_t j = 0; j < arrlen(monitor->resolutions); ++j)
        {
            Resolution res = monitor->resolutions[j];

            radioWidgets[j].pData = &pWindowRef->pCurRes[i];
            radioWidgets[j].mRadioId = (int32_t)j;

            monitorWidgets[j] = &monitorWidgetsBases[j];
            monitorWidgetsBases[j] = UIWidget{};
            monitorWidgetsBases[j].mType = WIDGET_TYPE_RADIO_BUTTON;
            monitorWidgetsBases[j].pWidget = &radioWidgets[j];

            snprintf(monitorWidgetsBases[j].mLabel, MAX_LABEL_STR_LENGTH, "%u", res.mWidth);
            strcat(monitorWidgetsBases[j].mLabel, "x");

            char height[10];
            snprintf(height, 10, "%u", res.mHeight);
            strcat(monitorWidgetsBases[j].mLabel, height);

            if (monitor->defaultResolution.mWidth == res.mWidth && monitor->defaultResolution.mHeight == res.mHeight)
            {
                strcat(monitorWidgetsBases[j].mLabel, " (native)");
                pWindowRef->pCurRes[i] = (int32_t)j;
                pWindowRef->pLastRes[i] = (int32_t)j;
            }

            uiSetWidgetOnEditedCallback(monitorWidgets[j], nullptr, wndUpdateResolution);
            uiSetWidgetDeferred(monitorWidgets[j], false);
        }

        uiCreateComponentWidget(pWindowControlsComponent, monitorLabel, &monitorHeader, WIDGET_TYPE_COLLAPSING_HEADER);
        tf_free(radioWidgets);
        tf_free(monitorWidgetsBases);
        tf_free(monitorWidgets);
    }

    enum
    {
        CONTROLS_INSIDE_WINDOW_LABEL_WIDGET,
        CONTROLS_INSIDE_WINDOW_NO_WIDGET,
        CONTROLS_INSIDE_WINDOW_YES_WIDGET,
        CONTROLS_CLIP_CURSOR_WIDGET,

        CONTROLS_WIDGET_COUNT
    };

    UIWidget  inputControlsWidgetBases[CONTROLS_WIDGET_COUNT];
    UIWidget* inputControlsWidgets[CONTROLS_WIDGET_COUNT];
    for (int i = 0; i < CONTROLS_WIDGET_COUNT; ++i)
    {
        inputControlsWidgetBases[i] = UIWidget{};
        inputControlsWidgets[i] = &inputControlsWidgetBases[i];
    }
    CollapsingHeaderWidget inputControlsWidget;
    inputControlsWidget.pGroupedWidgets = inputControlsWidgets;
    inputControlsWidget.mWidgetsCount = CONTROLS_WIDGET_COUNT;

    LabelWidget lCursorInWindow;
    strcpy(inputControlsWidgets[CONTROLS_INSIDE_WINDOW_LABEL_WIDGET]->mLabel, "Cursor inside window?");
    inputControlsWidgets[CONTROLS_INSIDE_WINDOW_LABEL_WIDGET]->pWidget = &lCursorInWindow;
    inputControlsWidgets[CONTROLS_INSIDE_WINDOW_LABEL_WIDGET]->mType = WIDGET_TYPE_LABEL;

    RadioButtonWidget rCursorInsideRectFalse;
    rCursorInsideRectFalse.pData = &pWindowRef->mCursorInsideWindow;
    rCursorInsideRectFalse.mRadioId = 0;
    strcpy(inputControlsWidgets[CONTROLS_INSIDE_WINDOW_NO_WIDGET]->mLabel, "No");
    inputControlsWidgets[CONTROLS_INSIDE_WINDOW_NO_WIDGET]->pWidget = &rCursorInsideRectFalse;
    inputControlsWidgets[CONTROLS_INSIDE_WINDOW_NO_WIDGET]->mType = WIDGET_TYPE_RADIO_BUTTON;

    RadioButtonWidget rCursorInsideRectTrue;
    rCursorInsideRectTrue.pData = &pWindowRef->mCursorInsideWindow;
    rCursorInsideRectTrue.mRadioId = 1;
    strcpy(inputControlsWidgets[CONTROLS_INSIDE_WINDOW_YES_WIDGET]->mLabel, "Yes");
    inputControlsWidgets[CONTROLS_INSIDE_WINDOW_YES_WIDGET]->pWidget = &rCursorInsideRectTrue;
    inputControlsWidgets[CONTROLS_INSIDE_WINDOW_YES_WIDGET]->mType = WIDGET_TYPE_RADIO_BUTTON;

    CheckboxWidget bClipCursor;
    bClipCursor.pData = &pWindowRef->mCursorCaptured;

    strcpy(inputControlsWidgets[CONTROLS_CLIP_CURSOR_WIDGET]->mLabel, "Capture cursor");
    inputControlsWidgets[CONTROLS_CLIP_CURSOR_WIDGET]->pWidget = &bClipCursor;
    inputControlsWidgets[CONTROLS_CLIP_CURSOR_WIDGET]->mType = WIDGET_TYPE_CHECKBOX;
    uiSetWidgetOnEditedCallback(inputControlsWidgets[CONTROLS_CLIP_CURSOR_WIDGET], nullptr, wndUpdateCaptureCursor);

    REGISTER_LUA_WIDGET(uiCreateComponentWidget(pWindowControlsComponent, "Cursor", &inputControlsWidget, WIDGET_TYPE_COLLAPSING_HEADER));
#endif

#if defined(AUTOMATED_TESTING)
    LabelWidget lScreenshotName;
    REGISTER_LUA_WIDGET(uiCreateComponentWidget(pWindowControlsComponent, "Screenshot Name", &lScreenshotName, WIDGET_TYPE_LABEL));

    snprintf(gAppName, sizeof(gAppName), "%s", pApp->GetName());

    TextboxWidget bTakeScreenshotName;
    bTakeScreenshotName.pText = &gScriptName;
    UIWidget* pTakeScreenshotName =
        uiCreateComponentWidget(pWindowControlsComponent, "Screenshot Name", &bTakeScreenshotName, WIDGET_TYPE_TEXTBOX);
    REGISTER_LUA_WIDGET(pTakeScreenshotName);

    ButtonWidget bTakeScreenshot;
    UIWidget* pTakeScreenshot = uiCreateComponentWidget(pWindowControlsComponent, "Take Screenshot", &bTakeScreenshot, WIDGET_TYPE_BUTTON);
    uiSetWidgetOnEditedCallback(pTakeScreenshot, nullptr, wndTakeScreenshot);
    uiSetWidgetDeferred(pTakeScreenshot, true);
    REGISTER_LUA_WIDGET(pTakeScreenshot);
#endif

#endif
}

void platformToggleWindowSystemUI(bool active)
{
#ifdef ENABLE_FORGE_UI
    uiSetComponentActive(pWindowControlsComponent, active);
#endif
}
