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

#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import <UIKit/UIKit.h>
#include <ctime>
#include <mach/clock.h>
#include <mach/mach.h>

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

#import "iOSAppDelegate.h"

#include "../../Utilities/Interfaces/IMemory.h"

#define FORGE_WINDOW_CLASS L"The Forge"
#define MAX_KEYS           256

#define elementsOf(a)      (sizeof(a) / sizeof((a)[0]))

static WindowDesc gCurrentWindow;
static int        gCurrentTouchEvent = 0;

extern float2 gRetinaScale;
extern int    gDeviceWidth;
extern int    gDeviceHeight;

static ReloadDesc gReloadDescriptor = { RELOAD_TYPE_ALL };
static bool       gShowPlatformUI = true;
static bool       gShutdownRequested = false;
static bool       gIsLoaded = false;
static bool       gBaseSubsystemAppDrawn = false;

static ThermalStatus gThermalStatus = THERMAL_STATUS_NONE;
#if defined(ENABLE_FORGE_RELOAD_SHADER)
static UIComponent* pReloadShaderComponent = NULL;
#endif

/// CPU
static CpuInfo gCpu;
static OSInfo  gOsInfo = {};

static IApp* pApp = NULL;
extern IApp* pWindowAppRef;

ThermalStatus getThermalStatus(void) { return gThermalStatus; }

@protocol ForgeViewDelegate <NSObject>
@required
- (void)drawRectResized:(CGSize)size;
@end

@interface ForgeMTLViewController: UIViewController
- (id)initWithFrame:(CGRect)FrameRect device:(id<MTLDevice>)device display:(int)displayID hdr:(bool)hdr vsync:(bool)vsync;
@end

@interface ForgeMTLView: UIView
+ (Class)layerClass;
- (void)layoutSubviews;
@property(nonatomic, nullable) id<ForgeViewDelegate> delegate;
@end

// Protocol abstracting the platform specific view in order to keep the Renderer class independent from platform
@protocol RenderDestinationProvider
- (void)draw;
@end

//------------------------------------------------------------------------
// MARK: TOUCH EVENT-RELATED FUNCTIONS
//------------------------------------------------------------------------

// Update the state of the keys based on state previous frame
void updateTouchEvent(int numTaps) { gCurrentTouchEvent = numTaps; }

int getTouchEvent()
{
    int prevTouchEvent = gCurrentTouchEvent;
    gCurrentTouchEvent = 0;
    return prevTouchEvent;
}

//------------------------------------------------------------------------
// MARK: OPERATING SYSTEM INTERFACE FUNCTIONS
//------------------------------------------------------------------------

void requestShutdown() { gShutdownRequested = true; }

void requestReload(const ReloadDesc* pReloadDesc) { gReloadDescriptor = *pReloadDesc; }

void errorMessagePopup(const char* title, const char* msg, WindowHandle* windowHandle, errorMessagePopupCallbackFn callback)
{
#if defined(AUTOMATED_TESTING)
    LOGF(eERROR, title);
    LOGF(eERROR, msg);
    if (callback)
    {
        callback();
    }
#else
    UIAlertController* alert = [UIAlertController alertControllerWithTitle:[NSString stringWithUTF8String:title]
                                                                   message:[NSString stringWithUTF8String:msg]
                                                            preferredStyle:UIAlertControllerStyleAlert];

    UIAlertAction* okAction = [UIAlertAction actionWithTitle:@"OK"
                                                       style:UIAlertActionStyleDefault
                                                     handler:^(UIAlertAction*) {
                                                         if (callback)
                                                         {
                                                             callback();
                                                         }
                                                     }];

    [alert addAction:okAction];
    UIViewController* vc = [[[[UIApplication sharedApplication] delegate] window] rootViewController];
    [vc presentViewController:alert animated:YES completion:nil];
#endif
}

void setCustomMessageProcessor(CustomMessageProcessor proc)
{
    // No-op
}

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
    ms = round(spec.tv_nsec / 1.0e6); // Convert nanoseconds to milliseconds
    ms += s * 1000;

    return (unsigned int)ms;
}

int64_t getUSec(bool precise)
{
    timespec ts;
    clock_gettime(_CLOCK_MONOTONIC, &ts);

    long us = (ts.tv_nsec / 1000);
    us += ts.tv_sec * 1e6;
    return us;
}

int64_t getTimerFrequency() { return 1; }

unsigned getTimeSinceStart() { return (unsigned)time(NULL); }

CpuInfo* getCpuInfo() { return &gCpu; }

OSInfo* getOsInfo() { return &gOsInfo; }
//-----//------------------------------------------------------------------------
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

void setupPlatformUI()
{
#ifdef ENABLE_FORGE_UI

    // WINDOW AND RESOLUTION CONTROL
    extern void platformSetupWindowSystemUI(IApp*);
    platformSetupWindowSystemUI(pApp);

#if defined(ENABLE_FORGE_RELOAD_SHADER)
    // RELOAD CONTROL
    UIComponentDesc desc = {};
    desc.mStartPosition = vec2(pApp->mSettings.mWidth * 0.6f, pApp->mSettings.mHeight * 0.90f);
    uiCreateComponent("Reload Control", &desc, &pReloadShaderComponent);
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
#endif
}

//------------------------------------------------------------------------
// MARK: APP ENTRY POINT
//------------------------------------------------------------------------

int          IApp::argc;
const char** IApp::argv;

int iOSMain(int argc, char** argv, IApp* app)
{
    pApp = app;
    pWindowAppRef = app;

    NSDictionary*            info = [[NSBundle mainBundle] infoDictionary];
    NSString*                minVersion = info[@"MinimumOSVersion"];
    NSArray*                 versionStr = [minVersion componentsSeparatedByString:@"."];
    NSOperatingSystemVersion version = {};
    version.majorVersion = versionStr.count > 0 ? [versionStr[0] integerValue] : 9;
    version.minorVersion = versionStr.count > 1 ? [versionStr[1] integerValue] : 0;
    version.patchVersion = versionStr.count > 2 ? [versionStr[2] integerValue] : 0;
    if (![[NSProcessInfo processInfo] isOperatingSystemAtLeastVersion:version])
    {
        NSString* osVersion = [[NSProcessInfo processInfo] operatingSystemVersionString];
        NSLog(@"Application requires at least iOS %@, but is being run on %@, and so is exiting", minVersion, osVersion);
        return 0;
    }

    @autoreleasepool
    {
        return UIApplicationMain(argc, argv, nil, NSStringFromClass([AppDelegate class]));
    }
}

//------------------------------------------------------------------------
// MARK: METALKITAPPLICATION IMPLEMENTATION
//------------------------------------------------------------------------

// Interface that controls the main updating/rendering loop on Metal appplications.
@interface MetalKitApplication: NSObject <ForgeViewDelegate>

- (void)setThermalStatus:(NSProcessInfoThermalState)state;

- (nonnull instancetype)initWithMetalDevice:(nonnull id<MTLDevice>)device
                  renderDestinationProvider:(nonnull id<RenderDestinationProvider>)renderDestinationProvider;

- (void)drawRectResized:(CGSize)size;

- (void)onFocusChanged:(BOOL)focused;

- (void)onActiveChanged:(BOOL)active;

- (void)update;

- (void)shutdown;

@end

extern void openWindow(const char* app_name, WindowDesc* winDesc, id<MTLDevice> device);

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

- (void)setThermalStatus:(NSProcessInfoThermalState)state
{
    ThermalStatus newStatus = THERMAL_STATUS_NOT_SUPPORTED;
    switch (state)
    {
    case NSProcessInfoThermalStateNominal:
        newStatus = THERMAL_STATUS_LIGHT;
        break;
    case NSProcessInfoThermalStateFair:
        newStatus = THERMAL_STATUS_MODERATE;
        break;
    case NSProcessInfoThermalStateSerious:
        newStatus = THERMAL_STATUS_SEVERE;
        break;
    case NSProcessInfoThermalStateCritical:
        newStatus = THERMAL_STATUS_CRITICAL;
        break;
    default:
        ASSERT(false);
    }

    LOGF(eINFO, "Thermal status event: %s (%d)", getThermalStatusString(newStatus), newStatus);
    gThermalStatus = newStatus;
}

- (void)thermalStateDidChange
{
    [self setThermalStatus:[[NSProcessInfo processInfo] thermalState]];
}

- (nonnull instancetype)initWithMetalDevice:(nonnull id<MTLDevice>)device
                  renderDestinationProvider:(nonnull id<RenderDestinationProvider, ForgeViewDelegate>)renderDestinationProvider
{
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

        // #if TF_USE_MTUNER
        //	rmemInit(0);
        // #endif

        FileSystemInitDesc fsDesc = {};
        fsDesc.pAppName = pApp->GetName();
        if (!initFileSystem(&fsDesc))
        {
            NSLog(@"Failed to initialize filesystem");
            exit(1);
        }

        fsSetPathForResourceDir(pSystemFileIO, RM_DEBUG, RD_LOG, "");
        initLog(pApp->GetName(), DEFAULT_LOG_LEVEL);

        NSString* strSysName = [[UIDevice currentDevice] systemName];
        NSString* strSysVersion = [[UIDevice currentDevice] systemVersion];
        NSString* strModel = [[UIDevice currentDevice] model];

        snprintf(gOsInfo.osName, 256, "%s", strSysName.UTF8String);
        snprintf(gOsInfo.osVersion, 256, "%s", strSysVersion.UTF8String);
        snprintf(gOsInfo.osDeviceName, 256, "%s", strModel.UTF8String);
        LOGF(LogLevel::eINFO, "Operating System: %s. Version: %s. Device Name: %s.", gOsInfo.osName, gOsInfo.osVersion,
             gOsInfo.osDeviceName);

        pSettings = &pApp->mSettings;

        gCurrentWindow = {};
        openWindow(pApp->GetName(), &gCurrentWindow, device);
        // the delagate is the one owning the Window so we need to bridge transfer instead of just bridge
        [UIApplication.sharedApplication.delegate setWindow:(__bridge_transfer UIWindow*)gCurrentWindow.handle.window];

        if (pSettings->mWidth == -1 || pSettings->mHeight == -1)
        {
            RectDesc rect = {};
            getRecommendedResolution(&rect);
            pSettings->mWidth = getRectWidth(&rect);
            pSettings->mHeight = getRectHeight(&rect);
            pSettings->mFullScreen = true;
        }

        gCurrentWindow.fullscreenRect = { 0, 0, (int)pSettings->mWidth, (int)pSettings->mHeight };
        gCurrentWindow.fullScreen = pSettings->mFullScreen;
        gCurrentWindow.maximized = false;
        gCurrentWindow.cursorCaptured = false;

        ForgeMTLView* forgeView = (ForgeMTLView*)((__bridge UIWindow*)(gCurrentWindow.handle.window)).rootViewController.view;
        forgeView.delegate = self;
        gCurrentWindow.handle.window = (void*)CFBridgingRetain(forgeView);

        pSettings->mWidth = getRectWidth(&gCurrentWindow.fullscreenRect);
        pSettings->mHeight = getRectHeight(&gCurrentWindow.fullscreenRect);

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
            if (!initBaseSubsystems())
                exit(1);

            // Apple Developer documentation: To receive NSProcessInfoThermalStateDidChangeNotification, you must access the thermalState
            // prior to registering for the notification.
            // https://developer.apple.com/documentation/foundation/processinfo/1410656-thermalstatedidchangenotificatio
            [self setThermalStatus:[[NSProcessInfo processInfo] thermalState]];

            [[NSNotificationCenter defaultCenter] addObserver:self
                                                     selector:@selector(thermalStateDidChange)
                                                         name:NSProcessInfoThermalStateDidChangeNotification
                                                       object:nil];

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
                                          exitLog();
                                          exit(0);
                                      });
                    return self;
                }

                exit(1);
            }

            setupPlatformUI();
            pApp->mSettings.mInitialized = true;

            if (gShutdownRequested)
            {
                [self shutdown];
                exit(0);
            }

            if (!pApp->Load(&gReloadDescriptor))
                exit(1);

            gIsLoaded = true;
        }
    }

#ifdef AUTOMATED_TESTING
    if (pSettings->mBenchmarking)
        setAggregateFrames(targetFrameCount / 2);
#endif

    return self;
}

- (void)drawRectResized:(CGSize)size
{
    bool needToUpdateApp = false;
    if (pApp->mSettings.mWidth != size.width * gRetinaScale.x)
    {
        pApp->mSettings.mWidth = size.width * gRetinaScale.x;
        needToUpdateApp = true;
    }
    if (pApp->mSettings.mHeight != size.height * gRetinaScale.y)
    {
        pApp->mSettings.mHeight = size.height * gRetinaScale.y;
        needToUpdateApp = true;
    }

    pApp->mSettings.mFullScreen = true;

    if (needToUpdateApp)
    {
        ReloadDesc reloadDesc;
        reloadDesc.mType = RELOAD_TYPE_RESIZE;
        requestReload(&reloadDesc);
    }
}

- (void)onFocusChanged:(BOOL)focused
{
    if (pApp == nullptr || !pApp->mSettings.mInitialized)
    {
        return;
    }

    pApp->mSettings.mFocused = focused;
}

- (void)onActiveChanged:(BOOL)active
{
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

    if (gShutdownRequested)
    {
        [self shutdown];
        exit(0);
    }

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
        [self shutdown];
        exit(0);
    }
#endif

#ifdef AUTOMATED_TESTING
    extern bool gAutomatedTestingScriptsFinished;
    // wait for the automated testing if it hasn't managed to finish in time
    if (gAutomatedTestingScriptsFinished && frameCounter >= targetFrameCount)
    {
        [self shutdown];
        exit(0);
    }
    frameCounter++;
#endif
}

- (void)shutdown
{
    if (pApp->mUnsupported)
    {
        return;
    }
#ifdef AUTOMATED_TESTING
    if (pSettings->mBenchmarking)
    {
        dumpBenchmarkData(pSettings, benchmarkOutput, pApp->GetName());
        dumpProfileData(benchmarkOutput, targetFrameCount);
    }
#endif
    pApp->mSettings.mQuit = true;
    if (gIsLoaded)
    {
        gReloadDescriptor.mType = RELOAD_TYPE_ALL;
        pApp->Unload(&gReloadDescriptor);
    }
    pApp->Exit();
    pApp->mSettings.mInitialized = false;

    exitBaseSubsystems();

    exitLog();
    exitFileSystem();

    // #if TF_USE_MTUNER
    //	rmemUnload();
    //	rmemShutDown();
    // #endif

    exitMemAlloc();
}
@end

//------------------------------------------------------------------------
// MARK: GAMECONTROLLER IMPLEMENTATION
//------------------------------------------------------------------------

// Our view controller.  Implements the MTKViewDelegate protocol, which allows it to accept
// per-frame update and drawable resize callbacks.  Also implements the RenderDestinationProvider
// protocol, which allows our renderer object to get and set drawable properties such as pixel
// format and sample count
@interface GameController: NSObject <RenderDestinationProvider>

@end

GameController* pMainViewController;

@implementation GameController
{
    ForgeMTLView*        _view;
    id<MTLDevice>        _device;
    MetalKitApplication* _application;
    bool                 _active;
}

- (void)dealloc
{
    @autoreleasepool
    {
        [_application shutdown];
    }
}

- (id)init
{
    self = [super init];

    pMainViewController = self;
    // Set the view to use the default device
    _device = MTLCreateSystemDefaultDevice();

    // Notify app delegate and display link with target frame pacing
    AppDelegate* appDel = (AppDelegate*)[UIApplication sharedApplication].delegate;
    appDel.displayLinkRefreshRate = pApp->mSettings.mMaxDisplayRefreshRate;

    // Kick-off the MetalKitApplication.
    _application = [[MetalKitApplication alloc] initWithMetalDevice:_device renderDestinationProvider:self];

    UIWindow* keyWindow = nil;
    for (UIWindow* window in [UIApplication sharedApplication].windows)
    {
        if (window.isKeyWindow)
        {
            keyWindow = window;
            break;
        }
    }

    _view = (ForgeMTLView*)keyWindow.rootViewController.view;

    // Enable multi-touch in our apps.
    [_view setMultipleTouchEnabled:true];
    _view.autoresizingMask = UIViewAutoresizingFlexibleHeight | UIViewAutoresizingFlexibleWidth;

    if (pApp->mSettings.mContentScaleFactor >= 1.f)
    {
        [_view setContentScaleFactor:pApp->mSettings.mContentScaleFactor];
    }

    if (!_device)
    {
        NSLog(@"Metal is not supported on this device");
    }

    // register terminate callback
    UIApplication* app = [UIApplication sharedApplication];
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(applicationWillTerminate:)
                                                 name:UIApplicationWillTerminateNotification
                                               object:app];

    return self;
}

- (void)onFocusChanged:(BOOL)focused
{
    [_application onFocusChanged:focused];
}

- (void)onActiveChanged:(BOOL)active
{
    _active = active;
    [_application onFocusChanged:active];
}

/*A notification named NSApplicationWillTerminateNotification.*/
- (void)applicationWillTerminate:(UIApplication*)app
{
    [_application shutdown];

    // Correctly clean up ARC and View references
    if (gCurrentWindow.handle.window)
    {
        CFRelease(gCurrentWindow.handle.window);
        gCurrentWindow.handle.window = nil;
    }
    // Decrement ARC
    _view = nil;
    _device = nil;
    _application = nil;
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

    // In case the application already resigned the active state,
    // call update once after entering the background so that IApp can react.
    if (_active || focusChanged)
    {
        [_application update];
    }
}
@end

#endif
