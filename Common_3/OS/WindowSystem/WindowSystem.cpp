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
#include "../../Application/Interfaces/IScreenshot.h"
#include "../../Application/Interfaces/IUI.h"
#include "../../Game/Interfaces/IScripting.h"
#include "../../OS/Interfaces/IOperatingSystem.h"
#include "../../Utilities/Interfaces/ITime.h"

#define MAX_WINDOW_RES_COUNT                         64
#define MAX_WINDOW_RES_STR_LENGTH                    64

#define RECOMMENDED_WINDOW_SIZE_FACTOR               8
#define RECOMMENDED_WINDOW_SIZE_MIN_DESIRED_FRACTION 0.75
#define RECOMMENDED_WINDOW_SIZE_DISPLAY_FRACTION     0.75

static WindowDesc*  pWindowRef = NULL;
static UIComponent* pWindowControlsComponent = NULL;

static char    gPlatformNameBuffer[64];
static bstring gPlatformName = bemptyfromarr(gPlatformNameBuffer);

#if defined(_WINDOWS) || defined(__APPLE__) && !defined(TARGET_IOS) || (defined(__linux__) && !defined(__ANDROID__))
#define WINDOW_UI_ENABLED
#endif

#if defined(WINDOW_UI_ENABLED)

// UI Window globals

static uint32_t gWindowSize = 0;
static uint32_t gWindowPrevSize = 0;
static uint32_t gPrevActiveMonitor;

// UI Monitor globals

static int32_t gMonitorResolution[MAX_MONITOR_COUNT] = {};
static int32_t gMonitorLastResolution[MAX_MONITOR_COUNT] = {};

#endif

#if defined(AUTOMATED_TESTING)
char           gAppName[64];
static char    gScriptNameBuffer[64];
static bstring gScriptName = bemptyfromarr(gScriptNameBuffer);
#endif

#if !defined(TARGET_IOS) && !defined(TARGET_IOS_SIMULATOR)
void getRecommendedResolution(RectDesc* rect)
{
    float          dpiScale[2];
    const uint32_t monitorIdx = getActiveMonitorIdx();
    getMonitorDpiScale(monitorIdx, dpiScale);

    MonitorDesc* pMonitor = getMonitor(monitorIdx);

    // Early exit
    if (arrlenu(pMonitor->resolutions) == 1)
    {
        rect->left = 0;
        rect->top = 0;
        rect->right = (int32_t)pMonitor->resolutions[0].mWidth;
        rect->bottom = (int32_t)pMonitor->resolutions[0].mHeight;
        return;
    }

    rect->left = 0;
    rect->top = 0;

    ASSERT(pMonitor->resolutions && pMonitor->currentResolution < arrlen(pMonitor->resolutions));

    const uint32_t monitorWidth = (uint32_t)pMonitor->resolutions[pMonitor->currentResolution].mWidth;
    const uint32_t monitorHeight = (uint32_t)pMonitor->resolutions[pMonitor->currentResolution].mHeight;

    const uint32_t desiredWidth = (uint32_t)(monitorWidth * RECOMMENDED_WINDOW_SIZE_DISPLAY_FRACTION);
    const uint32_t desiredHeight = (uint32_t)(monitorHeight * RECOMMENDED_WINDOW_SIZE_DISPLAY_FRACTION);

    const uint32_t minWidth = (uint32_t)(desiredWidth * RECOMMENDED_WINDOW_SIZE_MIN_DESIRED_FRACTION);
    const uint32_t minHeight = (uint32_t)(desiredHeight * RECOMMENDED_WINDOW_SIZE_MIN_DESIRED_FRACTION);

    uint32_t resultIndex = UINT32_MAX;
    uint32_t resultWidth = 0;
    uint32_t resultHeight = 0;

    // The search is based on the constraints below, which are relaxed one by one
    // until we find some valid resolution.
    // 1. Resolution should be divisible by the factor.
    //      Ensures that odd resolutions are not selected.
    // 2. Resolution should be bigger than min desired
    //      Ensures that resolution is not too far off from the desired
    // 3. Resolution should be less or equal to desired resolution
    //      Ensures that resolution can fit onto screen.
    // 4. Resolution should be less than display resolution
    //
    // Also we always take bigger resolution of the two that satisfy the constraints

    for (uint32_t factor = RECOMMENDED_WINDOW_SIZE_FACTOR; factor > 0 && resultIndex == UINT32_MAX; factor = factor >> 1)
    {
        for (uint32_t i = 0; i < arrlenu(pMonitor->resolutions); ++i)
        {
            const Resolution resolution = pMonitor->resolutions[i];

            if (resolution.mWidth % factor == 0 && resolution.mHeight % factor == 0 && resolution.mWidth >= minWidth &&
                resolution.mHeight >= minHeight && resolution.mWidth <= desiredWidth && resolution.mHeight <= desiredHeight &&
                resolution.mWidth >= resultWidth && resolution.mHeight >= resultHeight)
            {
                resultIndex = i;
                resultWidth = resolution.mWidth;
                resultHeight = resolution.mHeight;
            }
        }
    }

    // If search failed - remove 2nd constraint (Resolution should be bigger than min desired)
    for (uint32_t factor = RECOMMENDED_WINDOW_SIZE_FACTOR; factor > 0 && resultIndex == UINT32_MAX; factor = factor >> 1)
    {
        for (uint32_t i = 0; i < arrlenu(pMonitor->resolutions); ++i)
        {
            const Resolution resolution = pMonitor->resolutions[i];

            if (resolution.mWidth % factor == 0 && resolution.mHeight % factor == 0 && resolution.mWidth <= desiredWidth &&
                resolution.mHeight <= desiredHeight && resolution.mWidth >= resultWidth && resolution.mHeight >= resultHeight)
            {
                resultIndex = i;
                resultWidth = resolution.mWidth;
                resultHeight = resolution.mHeight;
            }
        }
    }

    // If search failed - keep only 4th constraint (Resolution should be less than display resolution)
    if (resultIndex == UINT32_MAX)
    {
        for (uint32_t i = 0; i < arrlenu(pMonitor->resolutions); ++i)
        {
            const Resolution resolution = pMonitor->resolutions[i];

            if (resolution.mWidth < monitorWidth && resolution.mHeight < monitorHeight && resolution.mWidth >= resultWidth &&
                resolution.mHeight >= resultHeight)
            {
                resultIndex = i;
                resultWidth = resolution.mWidth;
                resultHeight = resolution.mHeight;
            }
        }
    }

    // If search failed again - it's better to have window size
    // that it is same as display in width but smaller in height compared to display resolution
    if (resultIndex == UINT32_MAX)
    {
        for (uint32_t i = 0; i < arrlenu(pMonitor->resolutions); ++i)
        {
            const Resolution resolution = pMonitor->resolutions[i];

            if (resolution.mWidth <= monitorWidth && resolution.mHeight < monitorHeight && resolution.mWidth >= resultWidth &&
                resolution.mHeight >= resultHeight)
            {
                resultIndex = i;
                resultWidth = resolution.mWidth;
                resultHeight = resolution.mHeight;
            }
        }
    }

    // If all searches failed - return current display resolution
    resultIndex = resultIndex == UINT32_MAX ? pMonitor->currentResolution : resultIndex;

    *rect = { 0, 0, (int)pMonitor->resolutions[resultIndex].mWidth, (int)pMonitor->resolutions[resultIndex].mHeight };
}
#endif

#if defined(WINDOW_UI_ENABLED)

static bool wndValidateWindowPos(int32_t x, int32_t y)
{
    WindowDesc* winDesc = pWindowRef;
    int         clientWidthStart = (getRectWidth(&winDesc->windowedRect) - getRectWidth(&winDesc->clientRect)) >> 1;
    int         clientHeightStart = getRectHeight(&winDesc->windowedRect) - getRectHeight(&winDesc->clientRect) - clientWidthStart;

    if ((abs(x - winDesc->windowedRect.left - clientWidthStart) > 1) || (abs(y - winDesc->windowedRect.top - clientHeightStart) > 1))
        return false;

    return true;
}

static bool wndValidateWindowSize(int32_t width, int32_t height)
{
    RectDesc windowRect = pWindowRef->windowedRect;
    if ((abs(getRectWidth(&windowRect) - width) > 1) || (abs(getRectHeight(&windowRect) - height) > 1))
        return false;
    return true;
}

static void wndUpdateResolutionsList(void* pWindowResWidget)
{
    ASSERT(pWindowResWidget);
    UIWidget* pWidget = (UIWidget*)pWindowResWidget;
    ASSERT(pWidget->mType == WIDGET_TYPE_DROPDOWN);
    DropdownWidget* pDropdown = (DropdownWidget*)pWidget->pWidget;

    if (!pWindowRef)
    {
        pDropdown->mCount = 0;
        return;
    }

    char**       pNames = (char**)pDropdown->pNames;
    uint32_t     monitorIdx = getActiveMonitorIdx();
    MonitorDesc* pMonitor = getMonitor(monitorIdx);
    uint32_t     monitorResolutionIdx = gMonitorResolution[monitorIdx];
    ASSERT(pMonitor->resolutions && monitorResolutionIdx < arrlen(pMonitor->resolutions));
    Resolution monitorResolution = pMonitor->resolutions[monitorResolutionIdx];

    if (pWindowRef->fullScreen)
    {
        sprintf(pNames[0], "%ux%u##window_res", monitorResolution.mWidth, monitorResolution.mHeight);
        pDropdown->mCount = 1;
        *pDropdown->pData = 0;
        return;
    }

    uint32_t activeResIdx = MAX_WINDOW_RES_COUNT;

    pDropdown->mCount = (uint32_t)arrlen(pMonitor->resolutions) - 1;

    for (uint32_t i = 0; i < pDropdown->mCount; ++i)
    {
        monitorResolution = pMonitor->resolutions[i];
        sprintf(pNames[i], "%ux%u##window_res", monitorResolution.mWidth, monitorResolution.mHeight);

        if (pWindowRef->mWndW == (int32_t)monitorResolution.mWidth && pWindowRef->mWndH == (int32_t)monitorResolution.mHeight)
        {
            activeResIdx = i;
        }
    }

    *pDropdown->pData = activeResIdx;
}

static void wndUpdateResolution(void* pUserData)
{
    ASSERT(pUserData);
    UIWidget* pWidget = (UIWidget*)pUserData;

    uint32_t activeMonitorIdx = getActiveMonitorIdx();

    if (gWindowSize == gWindowPrevSize && activeMonitorIdx == gPrevActiveMonitor)
    {
        return;
    }

    if (activeMonitorIdx != gPrevActiveMonitor)
    {
        wndUpdateResolutionsList(pWidget);
    }
    gPrevActiveMonitor = activeMonitorIdx;

    MonitorDesc* pMonitor = getMonitor(activeMonitorIdx);

    gWindowPrevSize = gWindowSize;
    setWindowClientSize(pWindowRef, pMonitor->resolutions[gWindowSize].mWidth, pMonitor->resolutions[gWindowSize].mHeight);
}

static void wndSelectRecommendedWindowResolution(void* pUserData, const MonitorDesc* pMonitor)
{
    ASSERT(pUserData);
    ASSERT(pMonitor);
    UIWidget* pWidget = (UIWidget*)pUserData;
    ASSERT(pWidget->mType == WIDGET_TYPE_DROPDOWN);
    DropdownWidget* pDropdown = (DropdownWidget*)pWidget->pWidget;
    RectDesc        recommendedRect = {};
    getRecommendedResolution(&recommendedRect);
    const uint32_t recommendedWidth = recommendedRect.right - recommendedRect.left;
    const uint32_t recommendedHeight = recommendedRect.bottom - recommendedRect.top;

    uint32_t resolutionIndex = 0;
    for (; resolutionIndex < arrlen(pMonitor->resolutions) && resolutionIndex < pMonitor->currentResolution; ++resolutionIndex)
    {
        if (pMonitor->resolutions[resolutionIndex].mWidth == recommendedWidth &&
            pMonitor->resolutions[resolutionIndex].mHeight == recommendedHeight)
        {
            break;
        }
    }

    if (resolutionIndex >= arrlen(pMonitor->resolutions))
    {
        resolutionIndex = pMonitor->currentResolution > 0 ? pMonitor->currentResolution - 1 : pMonitor->currentResolution;
    }

    *pDropdown->pData = resolutionIndex;
    wndUpdateResolution(pWidget);
}

void wndSetWindowed(void* pUserData)
{
    setWindowed(pWindowRef);
    wndUpdateResolutionsList(pUserData);
}

void wndSetFullscreen(void* pUserData)
{
    setFullscreen(pWindowRef);
    wndUpdateResolutionsList(pUserData);
}

void wndSetBorderless(void* pUserData)
{
    setBorderless(pWindowRef);
    wndUpdateResolutionsList(pUserData);
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
void wndCenterWindow(void* pUserData)
{
    UNREF_PARAM(pUserData);
    centerWindow(pWindowRef);
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

static void monitorUpdateResolution(void* pUserData)
{
    UNREF_PARAM(pUserData);
    uint32_t monitorCount = getMonitorCount();
    for (uint32_t i = 0; i < monitorCount; ++i)
    {
        if (gMonitorResolution[i] != gMonitorLastResolution[i])
        {
            MonitorDesc* pMonitor = getMonitor(i);
            setResolution(pMonitor, &pMonitor->resolutions[gMonitorResolution[i]]);

            gMonitorLastResolution[i] = gMonitorResolution[i];
            if (i == getActiveMonitorIdx() && !pWindowRef->fullScreen)
            {
                wndUpdateResolutionsList(pUserData);
                wndSelectRecommendedWindowResolution(pUserData, getMonitor(i));
            }
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
    captureCursor(pWindowRef, pWindowRef->mCursorCaptured);
#endif
}

#endif

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
    uiAddComponent("Window and Resolution Controls", &uiDesc, &pWindowControlsComponent);
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
    REGISTER_LUA_WIDGET(uiAddComponentWidget(pWindowControlsComponent, "Platform Name", &Textbox, WIDGET_TYPE_TEXTBOX));

#if defined(WINDOW_UI_ENABLED)
    RadioButtonWidget rbWindowed;
    rbWindowed.pData = &pWindowRef->mWindowMode;
    rbWindowed.mRadioId = WM_WINDOWED;
    UIWidget* pWindowed = uiAddComponentWidget(pWindowControlsComponent, "Windowed", &rbWindowed, WIDGET_TYPE_RADIO_BUTTON);
    // Note: user pointer is set after "Window resolution" creation
    uiSetWidgetOnEditedCallback(pWindowed, nullptr, wndSetWindowed);
    uiSetWidgetDeferred(pWindowed, true);

    RadioButtonWidget rbFullscreen;
    rbFullscreen.pData = &pWindowRef->mWindowMode;
    rbFullscreen.mRadioId = WM_FULLSCREEN;
    UIWidget* pFullscreen = uiAddComponentWidget(pWindowControlsComponent, "Fullscreen", &rbFullscreen, WIDGET_TYPE_RADIO_BUTTON);
    // Note: user pointer is set after "Window resolution" creation
    uiSetWidgetOnEditedCallback(pFullscreen, nullptr, wndSetFullscreen);
    uiSetWidgetDeferred(pFullscreen, true);

    RadioButtonWidget rbBorderless;
    rbBorderless.pData = &pWindowRef->mWindowMode;
    rbBorderless.mRadioId = WM_BORDERLESS;
    UIWidget* pBorderless = uiAddComponentWidget(pWindowControlsComponent, "Borderless", &rbBorderless, WIDGET_TYPE_RADIO_BUTTON);
    // Note: user pointer is set after "Window resolution" creation
    uiSetWidgetOnEditedCallback(pBorderless, nullptr, wndSetBorderless);
    uiSetWidgetDeferred(pBorderless, true);

    ButtonWidget bMaximize;
    UIWidget*    pMaximize = uiAddComponentWidget(pWindowControlsComponent, "Maximize", &bMaximize, WIDGET_TYPE_BUTTON);
    uiSetWidgetOnEditedCallback(pMaximize, nullptr, wndMaximizeWindow);
    uiSetWidgetDeferred(pMaximize, true);
    REGISTER_LUA_WIDGET(pMaximize);

    ButtonWidget bMinimize;
    UIWidget*    pMinimize = uiAddComponentWidget(pWindowControlsComponent, "Minimize", &bMinimize, WIDGET_TYPE_BUTTON);
    uiSetWidgetOnEditedCallback(pMinimize, nullptr, wndMinimizeWindow);
    uiSetWidgetDeferred(pMinimize, true);
    REGISTER_LUA_WIDGET(pMinimize);

    ButtonWidget bCenter = {};
    UIWidget*    pCenter = uiAddComponentWidget(pWindowControlsComponent, "Center", &bCenter, WIDGET_TYPE_BUTTON);
    uiSetWidgetOnEditedCallback(pCenter, nullptr, wndCenterWindow);
    uiSetWidgetDeferred(pCenter, true);
    REGISTER_LUA_WIDGET(pCenter);

    RectDesc recRes;
    getRecommendedResolution(&recRes);

    uint32_t recWidth = recRes.right - recRes.left;
    uint32_t recHeight = recRes.bottom - recRes.top;

    SliderIntWidget setRectSliderX;
    setRectSliderX.pData = &pWindowRef->mWndX;
    setRectSliderX.mMin = 0;
    setRectSliderX.mMax = recWidth;
    REGISTER_LUA_WIDGET(uiAddComponentWidget(pWindowControlsComponent, "Window X Offset", &setRectSliderX, WIDGET_TYPE_SLIDER_INT));

    SliderIntWidget setRectSliderY;
    setRectSliderY.pData = &pWindowRef->mWndY;
    setRectSliderY.mMin = 0;
    setRectSliderY.mMax = recHeight;
    REGISTER_LUA_WIDGET(uiAddComponentWidget(pWindowControlsComponent, "Window Y Offset", &setRectSliderY, WIDGET_TYPE_SLIDER_INT));

    SliderIntWidget setRectSliderW;
    setRectSliderW.pData = &pWindowRef->mWndW;
    setRectSliderW.mMin = 144;
    setRectSliderW.mMax = getRectWidth(&pWindowRef->fullscreenRect);
    REGISTER_LUA_WIDGET(uiAddComponentWidget(pWindowControlsComponent, "Window Width", &setRectSliderW, WIDGET_TYPE_SLIDER_INT));

    SliderIntWidget setRectSliderH;
    setRectSliderH.pData = &pWindowRef->mWndH;
    setRectSliderH.mMin = 144;
    setRectSliderH.mMax = getRectHeight(&pWindowRef->fullscreenRect);
    REGISTER_LUA_WIDGET(uiAddComponentWidget(pWindowControlsComponent, "Window Height", &setRectSliderH, WIDGET_TYPE_SLIDER_INT));

    ButtonWidget bSetRect;
    UIWidget*    pSetRect = uiAddComponentWidget(pWindowControlsComponent, "Set window rectangle", &bSetRect, WIDGET_TYPE_BUTTON);
    // Note: user pointer is set after "Window resolution" creation
    uiSetWidgetOnEditedCallback(pSetRect, nullptr, wndMoveWindow);
    uiSetWidgetDeferred(pSetRect, true);

    ButtonWidget bRecWndSize;
    UIWidget*    pRecWndSize =
        uiAddComponentWidget(pWindowControlsComponent, "Set recommended window rectangle", &bRecWndSize, WIDGET_TYPE_BUTTON);
    // Note: user pointer is set after "Window resolution" creation
    uiSetWidgetOnEditedCallback(pRecWndSize, nullptr, wndSetRecommendedWindowSize);
    uiSetWidgetDeferred(pRecWndSize, true);

    DropdownWidget windowResDropDown = {};

    static char  windowResNames[MAX_WINDOW_RES_COUNT + 1][MAX_WINDOW_RES_STR_LENGTH];
    static char* pWindowResNamePtrs[MAX_WINDOW_RES_COUNT + 1];
    memset(windowResNames, 0, sizeof(windowResNames));
    sprintf(windowResNames[MAX_WINDOW_RES_COUNT], "##invalid_window_res");

    for (int32_t i = 0; i < MAX_WINDOW_RES_COUNT + 1; ++i)
    {
        pWindowResNamePtrs[i] = &windowResNames[i][0];
    }

    gWindowSize = 0;
    gWindowPrevSize = 0;

    windowResDropDown.mCount = 0;
    windowResDropDown.pNames = pWindowResNamePtrs;
    windowResDropDown.pData = &gWindowSize;

    UIWidget* pWindowResDropdown =
        uiAddComponentWidget(pWindowControlsComponent, "Window resolution", &windowResDropDown, WIDGET_TYPE_DROPDOWN);
    uiSetWidgetOnEditedCallback(pWindowResDropdown, pWindowResDropdown, wndUpdateResolution);
    uiSetWidgetDeferred(pWindowResDropdown, false);

    pWindowed->pOnEditedUserData = pWindowResDropdown;
    pFullscreen->pOnEditedUserData = pWindowResDropdown;
    pBorderless->pOnEditedUserData = pWindowResDropdown;
    pSetRect->pOnEditedUserData = pWindowResDropdown;
    pRecWndSize->pOnEditedUserData = pWindowResDropdown;

    REGISTER_LUA_WIDGET(pWindowed);
    REGISTER_LUA_WIDGET(pFullscreen);
    REGISTER_LUA_WIDGET(pBorderless);
    REGISTER_LUA_WIDGET(pSetRect);
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
    uiAddComponentWidget(pWindowControlsComponent, "Window Details", &windowDetailsHeader, WIDGET_TYPE_COLLAPSING_HEADER);
#endif

    uint32_t numMonitors = getMonitorCount();

    char label[64];
    snprintf(label, 64, "Number of displays: ");

    char monitors[10];
    snprintf(monitors, 10, "%u", numMonitors);
    strcat(label, monitors);

    LabelWidget labelWidget;
    REGISTER_LUA_WIDGET(uiAddComponentWidget(pWindowControlsComponent, label, &labelWidget, WIDGET_TYPE_LABEL));

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

            radioWidgets[j].pData = &gMonitorResolution[i];
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
                gMonitorResolution[i] = (int32_t)j;
                gMonitorLastResolution[i] = (int32_t)j;
            }

            uiSetWidgetOnEditedCallback(monitorWidgets[j], pWindowResDropdown, monitorUpdateResolution);
            uiSetWidgetDeferred(monitorWidgets[j], false);
        }

        uiAddComponentWidget(pWindowControlsComponent, monitorLabel, &monitorHeader, WIDGET_TYPE_COLLAPSING_HEADER);
        tf_free(radioWidgets);
        tf_free(monitorWidgetsBases);
        tf_free(monitorWidgets);
        if (i == getActiveMonitorIdx())
        {
            // Update window resolutions list after display resolutions are populated
            wndUpdateResolutionsList(pWindowResDropdown);
        }
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

    REGISTER_LUA_WIDGET(uiAddComponentWidget(pWindowControlsComponent, "Cursor", &inputControlsWidget, WIDGET_TYPE_COLLAPSING_HEADER));
#endif

#if defined(AUTOMATED_TESTING)
    LabelWidget lScreenshotName;
    REGISTER_LUA_WIDGET(uiAddComponentWidget(pWindowControlsComponent, "Screenshot Name", &lScreenshotName, WIDGET_TYPE_LABEL));

    snprintf(gAppName, sizeof(gAppName), "%s", pApp->GetName());

    TextboxWidget bTakeScreenshotName;
    bTakeScreenshotName.pText = &gScriptName;
    UIWidget* pTakeScreenshotName =
        uiAddComponentWidget(pWindowControlsComponent, "Screenshot Name", &bTakeScreenshotName, WIDGET_TYPE_TEXTBOX);
    REGISTER_LUA_WIDGET(pTakeScreenshotName);

    ButtonWidget bTakeScreenshot;
    UIWidget*    pTakeScreenshot = uiAddComponentWidget(pWindowControlsComponent, "Take Screenshot", &bTakeScreenshot, WIDGET_TYPE_BUTTON);
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
