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

#ifdef __APPLE__

#import <Cocoa/Cocoa.h>
#import <IOKit/graphics/IOGraphicsLib.h>
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#include <ctime>
#include <mach/clock.h>
#include <mach/mach.h>

#include "../../Utilities/ThirdParty/OpenSource/rmem/inc/rmem.h"

#include "../../Application/Interfaces/IApp.h"
#include "../../Application/Interfaces/IFont.h"
#include "../../Application/Interfaces/IProfiler.h"
#include "../../Application/Interfaces/IUI.h"
#include "../../Game/Interfaces/IScripting.h"
#include "../../Graphics/Interfaces/IGraphics.h"
#include "../../Utilities/Interfaces/IFileSystem.h"
#include "../../Utilities/Interfaces/ILog.h"
#include "../../Utilities/Interfaces/IThread.h"
#include "../../Utilities/Interfaces/ITime.h"
#include "../Interfaces/IOperatingSystem.h"

#include "../../OS/CPUConfig.h"
#if defined(ENABLE_FORGE_REMOTE_UI)
#include "../../Tools/Network/Network.h"
#endif
#if defined(ENABLE_FORGE_RELOAD_SHADER)
#include "../../Tools/ReloadServer/ReloadClient.h"
#endif
#include "../../Utilities/Math/MathTypes.h"

#include "../../Utilities/Interfaces/IMemory.h"

#define FORGE_WINDOW_CLASS L"The Forge"
#define MAX_KEYS           256
#define MAX_CURSOR_DELTA   200

#define elementsOf(a)      (sizeof(a) / sizeof((a)[0]))

static IApp*                  pApp = NULL;
WindowDesc                    gCurrentWindow;
extern CustomMessageProcessor sCustomProc;

static ReloadDesc gReloadDescriptor = { RELOAD_TYPE_ALL };
static bool       gShowPlatformUI = true;
static bool       gBaseSubsystemAppDrawn = false;

/// CPU
static CpuInfo gCpu;
static OSInfo  gOsInfo = {};

extern MonitorDesc* gMonitors;
extern uint32_t     gMonitorCount;
extern float2*      gDPIScales;

/// VSync Toggle
static UIComponent* pToggleVSyncWindow = NULL;
#if defined(ENABLE_FORGE_RELOAD_SHADER)
static UIComponent* pReloadShaderComponent = NULL;
#endif
@interface ForgeApplication: NSApplication
@end

ThermalStatus getThermalStatus(void) { return THERMAL_STATUS_NOT_SUPPORTED; }

@implementation ForgeApplication
- (void)sendEvent:(NSEvent*)event
{
    if ([event type] == NSEventTypeKeyUp)
    {
        [[[self mainWindow] firstResponder] tryToPerform:@selector(keyUp:) with:event];
        // returning as it will get discarded.
        return;
    }
    [super sendEvent:event];
}

@end

@interface ForgeNSWindow: NSWindow
{
}

- (CGFloat)titleBarHeight;

@end

// Protocol abstracting the platform specific view in order to keep the Renderer class independent from platform
@protocol RenderDestinationProvider
- (void)draw;
- (void)didResize:(CGSize)size;
- (void)didMiniaturize;
- (void)didDeminiaturize;
- (void)didFocusChange:(bool)active;
@end

@interface ForgeMTLView: NSView <NSWindowDelegate>
{
@private
    CVDisplayLinkRef displayLink;
    CAMetalLayer*    metalLayer;
}
@property(weak) id<RenderDestinationProvider> delegate;

- (id)initWithFrame:(NSRect)FrameRect device:(id<MTLDevice>)device display:(int)displayID hdr:(bool)hdr vsync:(bool)vsync;
- (CVReturn)getFrameForTime:(const CVTimeStamp*)outputTime;

@end

namespace
{
bool isCaptured = false;
}

//------------------------------------------------------------------------
// MARK: OPERATING SYSTEM INTERFACE FUNCTIONS
//------------------------------------------------------------------------

void requestShutdown()
{
    dispatch_async(dispatch_get_main_queue(), ^{
        ForgeMTLView* view = (__bridge ForgeMTLView*)(gCurrentWindow.handle.window);
        if (view == nil)
        {
            return;
        }

        NSNotificationCenter* notificationCenter = [[NSWorkspace sharedWorkspace] notificationCenter];
        [notificationCenter removeObserver:view name:NSWorkspaceDidActivateApplicationNotification object:nil];

        [notificationCenter removeObserver:view name:NSWorkspaceDidDeactivateApplicationNotification object:nil];

        [[NSApplication sharedApplication] terminate:[NSApplication sharedApplication]];
    });
}

void requestReload(const ReloadDesc* pReloadDesc) { gReloadDescriptor = *pReloadDesc; }

void errorMessagePopup(const char* title, const char* msg, WindowHandle* windowHandle, errorMessagePopupCallbackFn callback)
{
#if defined(AUTOMATED_TESTING)
    LOGF(eERROR, title);
    LOGF(eERROR, msg);
#else
    NSAlert* alert = [[NSAlert alloc] init];

    [alert setMessageText:[NSString stringWithCString:title encoding:[NSString defaultCStringEncoding]]];
    [alert setInformativeText:[NSString stringWithCString:msg encoding:[NSString defaultCStringEncoding]]];
    [alert setAlertStyle:NSAlertStyleCritical];
    [alert addButtonWithTitle:@"OK"];
    [alert runModal];
#endif
    if (callback)
    {
        callback();
    }
}

CustomMessageProcessor sCustomProc = nullptr;
void                   setCustomMessageProcessor(CustomMessageProcessor proc) { sCustomProc = proc; }

//------------------------------------------------------------------------
// MARK: TIME RELATED FUNCTIONS (consider moving...)
//------------------------------------------------------------------------
unsigned getSystemTime()
{
    long            ms; // Milliseconds
    time_t          s;  // Seconds
    struct timespec spec;

    clock_gettime(_CLOCK_MONOTONIC, &spec);

    s = spec.tv_sec;
    ms = round(spec.tv_nsec / CLOCKS_PER_SEC); // Convert nanoseconds to milliseconds
    ms += s * 1000;

    return (unsigned int)ms;
}

int64_t getUSec(bool precise)
{
    timespec ts;
    clock_gettime(_CLOCK_MONOTONIC, &ts);

    long us = (ts.tv_nsec / 1000);
    us += ts.tv_sec * CLOCKS_PER_SEC;
    return us;
}

int64_t getTimerFrequency() { return CLOCKS_PER_SEC; }

unsigned getTimeSinceStart() { return (unsigned)time(NULL); }

CpuInfo* getCpuInfo() { return &gCpu; }

OSInfo* getOsInfo() { return &gOsInfo; }
//------------------------------------------------------------------------
// MARK: PLATFORM LAYER CORE SUBSYSTEMS
//------------------------------------------------------------------------

bool initBaseSubsystems()
{
    // Not exposed in the interface files / app layer
    extern bool platformInitFontSystem();
    extern bool platformInitUserInterface();
    extern void platformInitLuaScriptingSystem();
    extern void platformInitWindowSystem(WindowDesc*);

    platformInitWindowSystem(&gCurrentWindow);
    pApp->pWindow = &gCurrentWindow;

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

#if defined(ENABLE_FORGE_SCRIPTING) && defined(AUTOMATED_TESTING)
    // Tests below are executed first, before any tests registered in IApp::Init
    const char*    sFirstTestScripts[] = { "Test_Default.lua" };
    const uint32_t numScripts = sizeof(sFirstTestScripts) / sizeof(sFirstTestScripts[0]);
    LuaScriptDesc  scriptDescs[numScripts] = {};
    for (uint32_t i = 0; i < numScripts; ++i)
    {
        scriptDescs[i].pScriptFileName = sFirstTestScripts[i];
    }
    luaDefineScripts(scriptDescs, numScripts);
#endif
#endif

#if defined(ENABLE_FORGE_REMOTE_UI)
    initNetwork();
#endif

    return true;
}

void updateBaseSubsystems(float deltaTime, bool appDrawn)
{
    // Not exposed in the interface files / app layer
    extern void platformUpdateLuaScriptingSystem(bool appDrawn);
    extern void platformUpdateUserInterface(float deltaTime);
    extern void platformUpdateWindowSystem();

    platformUpdateWindowSystem();

#ifdef ENABLE_FORGE_SCRIPTING
    platformUpdateLuaScriptingSystem(appDrawn);
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
    extern void platformExitWindowSystem();

    platformExitWindowSystem();

#ifdef ENABLE_FORGE_UI
    platformExitUserInterface();
#endif

#ifdef ENABLE_FORGE_FONTS
    platformExitFontSystem();
#endif

#ifdef ENABLE_FORGE_SCRIPTING
    platformExitLuaScriptingSystem();
#endif

#if defined(ENABLE_FORGE_REMOTE_UI)
    exitNetwork();
#endif
}

//------------------------------------------------------------------------
// MARK: PLATFORM LAYER USER INTERFACE
//------------------------------------------------------------------------

void setupPlatformUI(int32_t width, int32_t height)
{
#ifdef ENABLE_FORGE_UI

    // WINDOW AND RESOLUTION CONTROL
    extern void platformSetupWindowSystemUI(IApp*);
    platformSetupWindowSystemUI(pApp);

    // VSYNC CONTROL
    UIComponentDesc UIComponentDesc = {};
    UIComponentDesc.mStartPosition = vec2(width * 0.4f, height * 0.90f);
    uiCreateComponent("VSync Control", &UIComponentDesc, &pToggleVSyncWindow);

    CheckboxWidget checkbox;
    checkbox.pData = &pApp->mSettings.mVSyncEnabled;
    UIWidget* pCheckbox = uiCreateComponentWidget(pToggleVSyncWindow, "Toggle VSync\t\t\t\t\t", &checkbox, WIDGET_TYPE_CHECKBOX);
    REGISTER_LUA_WIDGET(pCheckbox);

#if defined(ENABLE_FORGE_RELOAD_SHADER)
    // RELOAD CONTROL
    UIComponentDesc = {};
    UIComponentDesc.mStartPosition = vec2(width * 0.6f, height * 0.90f);
    uiCreateComponent("Reload Control", &UIComponentDesc, &pReloadShaderComponent);
    platformReloadClientAddReloadShadersButton(pReloadShaderComponent);
#endif

    // MICROPROFILER UI
    toggleProfilerMenuUI(true);

#endif

#if defined(ENABLE_FORGE_SCRIPTING) && defined(AUTOMATED_TESTING)
// Tests below are executed last, after tests registered in IApp::Init have executed
#if 0 // For now we don't have any tests that require running after UT tests for this platform
	const char* sLastTestScripts[] = { "" };
	const uint32_t numScripts = sizeof(sLastTestScripts) / sizeof(sLastTestScripts[0]);
	LuaScriptDesc scriptDescs[numScripts] = {};
	for (uint32_t i = 0; i < numScripts; ++i)
	{
		scriptDescs[i].pScriptFileName = sLastTestScripts[i];
	}
	luaDefineScripts(scriptDescs, numScripts);
#endif
#endif
}

void togglePlatformUI()
{
    gShowPlatformUI = pApp->mSettings.mShowPlatformUI;

#ifdef ENABLE_FORGE_UI
    extern void platformToggleWindowSystemUI(bool);
    platformToggleWindowSystemUI(gShowPlatformUI);

    uiSetComponentActive(pToggleVSyncWindow, gShowPlatformUI);
#if defined(ENABLE_FORGE_RELOAD_SHADER)
    uiSetComponentActive(pReloadShaderComponent, gShowPlatformUI);
#endif
#endif
}

//------------------------------------------------------------------------
// MARK: APP ENTRY POINT
//------------------------------------------------------------------------

int          IApp::argc;
const char** IApp::argv;

int macOSMain(int argc, const char** argv, IApp* app)
{
    pApp = app;

    NSDictionary*            info = [[NSBundle mainBundle] infoDictionary];
    NSString*                minVersion = info[@"LSMinimumSystemVersion"];
    NSArray*                 versionStr = [minVersion componentsSeparatedByString:@"."];
    NSOperatingSystemVersion version = {};
    version.majorVersion = versionStr.count > 0 ? [versionStr[0] integerValue] : 10;
    version.minorVersion = versionStr.count > 1 ? [versionStr[1] integerValue] : 11;
    version.patchVersion = versionStr.count > 2 ? [versionStr[2] integerValue] : 0;
    if (![[NSProcessInfo processInfo] isOperatingSystemAtLeastVersion:version])
    {
        NSString* osVersion = [[NSProcessInfo processInfo] operatingSystemVersionString];
        NSLog(@"Application requires at least macOS %@, but is being run on %@, and so is exiting", minVersion, osVersion);
        return 0;
    }

    return NSApplicationMain(argc, argv);
}

// Interface that controls the main updating/rendering loop on Metal appplications.
@interface MetalKitApplication: NSObject

- (nonnull instancetype)initWithMetalDevice:(nonnull id<MTLDevice>)device
                  renderDestinationProvider:(nonnull id<RenderDestinationProvider>)renderDestinationProvider;

- (void)drawRectResized:(CGSize)size;
- (void)onFocusChanged:(BOOL)focused;
- (void)update;
- (void)shutdown;
@end

// Our view controller.  Implements the MTKViewDelegate protocol, which allows it to accept
// per-frame update and drawable resize callbacks.  Also implements the RenderDestinationProvider
// protocol, which allows our renderer object to get and set drawable properties such as pixel
// format and sample count
@interface GameController: NSObject <RenderDestinationProvider>

@end

//------------------------------------------------------------------------
// MARK: GAMECONTROLLER IMPLEMENTATION
//------------------------------------------------------------------------

@implementation GameController
{
    id<MTLDevice>        _device;
    MetalKitApplication* _application;
}

- (id)init
{
    self = [super init];

    _device = MTLCreateSystemDefaultDevice();
    isCaptured = false;

    // Kick-off the MetalKitApplication.
    _application = [[MetalKitApplication alloc] initWithMetalDevice:_device renderDestinationProvider:self];

    if (!_device)
    {
        NSLog(@"Metal is not supported on this device");
    }

    // register terminate callback
    NSApplication* app = [NSApplication sharedApplication];
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(applicationWillTerminate:)
                                                 name:NSApplicationWillTerminateNotification
                                               object:app];
    return self;
}

/*A notification named NSApplicationWillTerminateNotification.*/
- (void)applicationWillTerminate:(NSNotification*)notification
{
    [_application shutdown];
}

- (void)didFocusChange:(bool)active
{
    [_application onFocusChanged:active];
}

- (void)didDeminiaturize
{
    gCurrentWindow.minimized = false;
    [_application onFocusChanged:TRUE];
}

- (void)didMiniaturize
{
    gCurrentWindow.minimized = true;
    [_application onFocusChanged:FALSE];
}

// Called whenever view changes orientation or layout is changed
- (void)didResize:(CGSize)size
{
    [_application drawRectResized:size];
}

// Called whenever the view needs to render
- (void)draw
{
    static bool lastFocused = true;
    bool        focusChanged = false;
    if (pApp)
    {
        focusChanged = lastFocused != pApp->mSettings.mFocused;
        lastFocused = pApp->mSettings.mFocused;
    }

    // Call update once after minimizing so that IApp can react.
    if (!gCurrentWindow.minimized || focusChanged)
    {
        [_application update];
    }
}

@end

//------------------------------------------------------------------------
// MARK: METALKITAPPLICATION IMPLEMENTATION
//------------------------------------------------------------------------

extern void collectMonitorInfo();
extern void openWindow(const char* app_name, WindowDesc* winDesc, id<MTLDevice> device,
                       id<RenderDestinationProvider> delegateRenderProvider, int32_t monitorIndex);

// Timer used in the update function.
HiresTimer      deltaTimer;
IApp::Settings* pSettings;
#ifdef AUTOMATED_TESTING
uint32_t frameCounter;
uint32_t targetFrameCount = DEFAULT_AUTOMATION_FRAME_COUNT;
char     benchmarkOutput[1024] = { "\0" };
#endif

// Metal application implementation.
@implementation MetalKitApplication
{
}

- (nonnull instancetype)initWithMetalDevice:(nonnull id<MTLDevice>)device
                  renderDestinationProvider:(nonnull id<RenderDestinationProvider>)renderDestinationProvider
{
#define EXIT_IF_FAILED(cond) \
    if (!(cond))             \
        exit(1);

    self = [super init];
    if (self)
    {
        if (!initMemAlloc(pApp->GetName()))
        {
            NSLog(@"Failed to initialize memory manager");
            exit(1);
        }

        initCpuInfo(&gCpu);
        initHiresTimer(&deltaTimer);

#if TF_USE_MTUNER
        rmemInit(0);
#endif

        FileSystemInitDesc fsDesc = {};
        fsDesc.pAppName = pApp->GetName();
        if (!initFileSystem(&fsDesc))
        {
            NSLog(@"Failed to initialize filesystem");
            exit(1);
        }

        fsSetPathForResourceDir(pSystemFileIO, RM_DEBUG, RD_LOG, "");
        initLog(pApp->GetName(), DEFAULT_LOG_LEVEL);

        NSString* version = [[NSProcessInfo processInfo] operatingSystemVersionString];

        snprintf(gOsInfo.osName, 256, "MacOS");
        snprintf(gOsInfo.osVersion, 256, "%s", version.UTF8String);
        snprintf(gOsInfo.osDeviceName, 256, "Unknown");
        LOGF(LogLevel::eINFO, "Operating System: %s. Version: %s. Device Name: %s.", gOsInfo.osName, gOsInfo.osVersion,
             gOsInfo.osDeviceName);

        collectMonitorInfo();

        pSettings = &pApp->mSettings;
        if (pSettings->mMonitorIndex < 0 || pSettings->mMonitorIndex >= (int)gMonitorCount)
        {
            pSettings->mMonitorIndex = 0;
        }

        gCurrentWindow = {};
        gCurrentWindow.fullScreen = pSettings->mFullScreen;
        gCurrentWindow.borderlessWindow = pSettings->mBorderlessWindow;
        gCurrentWindow.noresizeFrame = !pSettings->mDragToResize;
        gCurrentWindow.maximized = false;
        gCurrentWindow.centered = pSettings->mCentered;
        gCurrentWindow.forceLowDPI = pSettings->mForceLowDPI;
        gCurrentWindow.cursorCaptured = false;

        if (pSettings->mWidth <= 0 || pSettings->mHeight <= 0)
        {
            RectDesc rect = {};
            getRecommendedResolution(&rect);
            pSettings->mWidth = getRectWidth(&rect);
            pSettings->mHeight = getRectHeight(&rect);
        }
        MonitorDesc* pMonitor = getMonitor(pSettings->mMonitorIndex);
        ASSERT(pMonitor);

        // use dpi scale of selected monitor
        float dpiScale[2];
        getMonitorDpiScale(pSettings->mMonitorIndex, dpiScale);

        gCurrentWindow.clientRect = { (int32_t)(pSettings->mWindowX + pMonitor->monitorRect.left * dpiScale[0]),
                                      (int32_t)(pSettings->mWindowY + pMonitor->monitorRect.top * dpiScale[0]),
                                      (int32_t)(pSettings->mWindowX + pMonitor->monitorRect.left * dpiScale[0]) + pSettings->mWidth,
                                      (int32_t)(pSettings->mWindowY + pMonitor->monitorRect.top * dpiScale[0]) + pSettings->mHeight };
        gCurrentWindow.windowedRect = gCurrentWindow.clientRect;

        gCurrentWindow.fullscreenRect = {
            (int32_t)(pMonitor->monitorRect.left * dpiScale[0]),
            (int32_t)(pMonitor->monitorRect.top * dpiScale[1]),
            (int32_t)(pMonitor->monitorRect.right * dpiScale[0]),
            (int32_t)(pMonitor->monitorRect.bottom * dpiScale[1]),
        };

        openWindow(pApp->GetName(), &gCurrentWindow, device, renderDestinationProvider, pSettings->mMonitorIndex);
        // center window if settings specify it
        if (pSettings->mCentered)
            centerWindow(&gCurrentWindow);
        pSettings->mFocused = true;

        pSettings->mWidth =
            gCurrentWindow.fullScreen ? getRectWidth(&gCurrentWindow.fullscreenRect) : getRectWidth(&gCurrentWindow.clientRect);
        pSettings->mHeight =
            gCurrentWindow.fullScreen ? getRectHeight(&gCurrentWindow.fullscreenRect) : getRectHeight(&gCurrentWindow.clientRect);

#ifdef AUTOMATED_TESTING
        // Check if benchmarking was given through command line
        for (int i = 0; i < pApp->argc; i += 1)
        {
            if (strcmp(pApp->argv[i], "-b") == 0)
            {
                pSettings->mBenchmarking = true;
                if (i + 1 < pApp->argc && isdigit(*(pApp->argv[i + 1])))
                    targetFrameCount = min(max(atoi(pApp->argv[i + 1]), 32), 512);
            }
            else if (strcmp(pApp->argv[i], "--request-recompile-after") == 0)
            {
                extern uint32_t gReloadServerRequestRecompileAfter;
                if (i + 1 < pApp->argc && isdigit(*pApp->argv[i + 1]))
                    gReloadServerRequestRecompileAfter = atoi(pApp->argv[i + 1]);
            }
            else if (strcmp(pApp->argv[i], "--no-auto-exit") == 0)
            {
                targetFrameCount = UINT32_MAX;
            }
            else if (strcmp(pApp->argv[i], "-o") == 0 && i + 1 < pApp->argc)
            {
                strcpy(benchmarkOutput, pApp->argv[i + 1]);
            }
        }
#endif

        @autoreleasepool
        {
            // if base subsystem fails then exit the app
            if (!initBaseSubsystems())
            {
                for (ForgeNSWindow* window in [NSApplication sharedApplication].windows)
                {
                    [window close];
                }

                exit(1);
            }

            // if app init fails then exit the app
            if (!pApp->Init())
            {
                const char* pRendererReason;
                if (hasRendererInitializationError(&pRendererReason))
                {
                    pApp->ShowUnsupportedMessage(pRendererReason);
                }

                if (pApp->mUnsupported)
                {
                    errorMessagePopup("Application unsupported", pApp->pUnsupportedReason ? pApp->pUnsupportedReason : "",
                                      &pApp->pWindow->handle,
                                      []()
                                      {
                                          for (ForgeNSWindow* window in [NSApplication sharedApplication].windows)
                                          {
                                              [window close];
                                          }
                                          exitLog();
                                          exit(0);
                                      });

                    return self;
                }

                for (ForgeNSWindow* window in [NSApplication sharedApplication].windows)
                {
                    [window close];
                }

                exit(1);
            }

            setupPlatformUI(pSettings->mWidth, pSettings->mHeight);
            pApp->mSettings.mInitialized = true;

            // if load fails then exit the app
            if (!pApp->Load(&gReloadDescriptor))
            {
                for (ForgeNSWindow* window in [NSApplication sharedApplication].windows)
                {
                    [window close];
                }

                exit(1);
            }
        }

#ifdef AUTOMATED_TESTING
        if (pSettings->mBenchmarking)
            setAggregateFrames(targetFrameCount / 2);
#endif
    }

    return self;
}

- (void)drawRectResized:(CGSize)size
{
    float          dpiScale[2];
    const uint32_t monitorIdx = getActiveMonitorIdx();
    getMonitorDpiScale(monitorIdx, dpiScale);
    int32_t newWidth = (int32_t)(size.width * dpiScale[0] + 1e-6);
    int32_t newHeight = (int32_t)(size.height * dpiScale[1] + 1e-6);

    if (newWidth != pApp->mSettings.mWidth || newHeight != pApp->mSettings.mHeight)
    {
        pApp->mSettings.mWidth = newWidth;
        pApp->mSettings.mHeight = newHeight;

        ReloadDesc reloadDesc;
        reloadDesc.mType = RELOAD_TYPE_RESIZE;
        requestReload(&reloadDesc);
    }
}

- (void)update
{
    if (pApp->mUnsupported)
    {
        return;
    }

    float deltaTime = getHiresTimerSeconds(&deltaTimer, true);
    // if framerate appears to drop below about 6, assume we're at a breakpoint and simulate 20fps.
    if (deltaTime > 0.15f)
        deltaTime = 0.05f;

#if defined(AUTOMATED_TESTING)
    // Used to keep screenshot results consistent across CI runs
    deltaTime = AUTOMATION_FIXED_FRAME_TIME;
#endif

    // UPDATE BASE INTERFACES
    updateBaseSubsystems(deltaTime, gBaseSubsystemAppDrawn);
    gBaseSubsystemAppDrawn = false;

    if (gReloadDescriptor.mType != RELOAD_TYPE_ALL)
    {
        pApp->Unload(&gReloadDescriptor);
        pApp->Load(&gReloadDescriptor);

        gReloadDescriptor.mType = RELOAD_TYPE_ALL;
        return;
    }

    // UPDATE APP
    pApp->Update(deltaTime);

    pApp->Draw();
    gBaseSubsystemAppDrawn = true;

    if (gShowPlatformUI != pApp->mSettings.mShowPlatformUI)
    {
        togglePlatformUI();
    }

#if defined(ENABLE_FORGE_RELOAD_SHADER)
    if (platformReloadClientShouldQuit())
    {
        for (ForgeNSWindow* window in [NSApplication sharedApplication].windows)
        {
            [window close];
        }

        [NSApp terminate:nil];
    }
#endif

#ifdef AUTOMATED_TESTING
    extern bool gAutomatedTestingScriptsFinished;
    // wait for the automated testing if it hasn't managed to finish in time
    if (gAutomatedTestingScriptsFinished && frameCounter >= targetFrameCount)
    {
        for (ForgeNSWindow* window in [NSApplication sharedApplication].windows)
        {
            [window close];
        }

        [NSApp terminate:nil];
    }
    frameCounter++;
#endif
}

- (void)shutdown
{
    for (int i = 0; i < gMonitorCount; ++i)
    {
        MonitorDesc& monitor = gMonitors[i];
        arrfree(monitor.resolutions);
    }

    if (gMonitorCount > 0)
    {
        tf_free(gDPIScales);
        tf_free(gMonitors);
    }

#ifdef AUTOMATED_TESTING
    if (pSettings->mBenchmarking)
    {
        dumpBenchmarkData(pSettings, benchmarkOutput, pApp->GetName());
        dumpProfileData(benchmarkOutput, targetFrameCount);
    }
#endif

    pApp->mSettings.mQuit = true;
    gReloadDescriptor.mType = RELOAD_TYPE_ALL;
    pApp->Unload(&gReloadDescriptor);
    pApp->Exit();

    pApp->mSettings.mInitialized = false;

    exitBaseSubsystems();

    exitLog();

    exitFileSystem();

#if TF_USE_MTUNER
    rmemUnload();
    rmemShutDown();
#endif

    exitMemAlloc();
}
- (void)onFocusChanged:(BOOL)focused
{
    if (pApp == nullptr || !pApp->mSettings.mInitialized)
    {
        return;
    }

    pApp->mSettings.mFocused = focused;
}
@end

#endif
