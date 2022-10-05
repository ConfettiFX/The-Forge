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

#include "../../Application/Config.h"

#ifdef __APPLE__

#include "../../OS/CPUConfig.h"

#import <UIKit/UIKit.h>
#import "iOSAppDelegate.h"
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>

#include <ctime>

#include <mach/clock.h>
#include <mach/mach.h>

#include "../../Utilities/Math/MathTypes.h"

#include "../Interfaces/IOperatingSystem.h"
#include "../../Utilities/Interfaces/ILog.h"
#include "../../Utilities/Interfaces/ITime.h"
#include "../../Utilities/Interfaces/IThread.h"
#include "../../Utilities/Interfaces/IFileSystem.h"
#include "../../Application/Interfaces/IProfiler.h"
#include "../../Application/Interfaces/IApp.h"

#include "../../Game/Interfaces/IScripting.h"
#include "../../Application/Interfaces/IFont.h"
#include "../../Application/Interfaces/IUI.h"

#include "../../Graphics/Interfaces/IGraphics.h"

#include "../../Utilities/Interfaces/IMemory.h"

#define FORGE_WINDOW_CLASS L"The Forge"
#define MAX_KEYS 256

#define elementsOf(a) (sizeof(a) / sizeof((a)[0]))

static WindowDesc gCurrentWindow;
static int              gCurrentTouchEvent = 0;

extern float2 gRetinaScale;
extern int     gDeviceWidth;
extern int     gDeviceHeight;

static ReloadDesc gReloadDescriptor = { RELOAD_TYPE_ALL };
static bool    gShowPlatformUI = true;

/// CPU
static CpuInfo gCpu;

static IApp* pApp = NULL;
extern IApp* pWindowAppRef;

@protocol ForgeViewDelegate<NSObject>
@required
- (void)drawRectResized:(CGSize)size;
@end

@interface ForgeMTLViewController: UIViewController
- (id)initWithFrame:(CGRect)FrameRect device:(id<MTLDevice>)device display:(int)displayID hdr:(bool)hdr vsync:(bool)vsync;
@end

@interface ForgeMTLView: UIView
+ (Class)layerClass;
- (void)layoutSubviews;
@property(nonatomic, weak) id<ForgeViewDelegate> delegate;

@end

// Protocol abstracting the platform specific view in order to keep the Renderer class independent from platform
@protocol RenderDestinationProvider
- (void)draw;
@end

//------------------------------------------------------------------------
// MARK: TOUCH EVENT-RELATED FUNCTIONS
//------------------------------------------------------------------------

// Update the state of the keys based on state previous frame
void updateTouchEvent(int numTaps)
{
	gCurrentTouchEvent = numTaps;
}

int getTouchEvent()
{
	int prevTouchEvent = gCurrentTouchEvent;
	gCurrentTouchEvent = 0;
	return prevTouchEvent;
}

//------------------------------------------------------------------------
// MARK: OPERATING SYSTEM INTERFACE FUNCTIONS
//------------------------------------------------------------------------

void requestShutdown()
{
	// NOT SUPPORTED ON THIS PLATFORM
}

void requestReload(const ReloadDesc* pReloadDesc)
{
	gReloadDescriptor = *pReloadDesc;
}

void errorMessagePopup(const char* title, const char* msg, void* windowHandle)
{
#ifndef AUTOMATED_TESTING
	UIAlertController* alert = [UIAlertController alertControllerWithTitle:[NSString stringWithUTF8String:title]
																   message:[NSString stringWithUTF8String:msg]
															preferredStyle:UIAlertControllerStyleAlert];

	UIAlertAction* okAction = [UIAlertAction actionWithTitle:@"OK" style:UIAlertActionStyleDefault handler:nil];

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

#ifdef __IPHONE_10_0
#if __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_10_0
#define ENABLE_CLOCK_GETTIME
#endif
#endif

unsigned getSystemTime()
{
	long            ms;    // Milliseconds
	time_t          s;     // Seconds
	struct timespec spec;

#if defined(ENABLE_CLOCK_GETTIME)
	if (@available(iOS 10.0, *))
	{
		clock_gettime(_CLOCK_MONOTONIC, &spec);
	}
	else
#endif
	{
		// https://stackoverflow.com/questions/5167269/clock-gettime-alternative-in-mac-os-x
		clock_serv_t    cclock;
		mach_timespec_t mts;
		host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &cclock);
		clock_get_time(cclock, &mts);
		mach_port_deallocate(mach_task_self(), cclock);
		spec.tv_sec = mts.tv_sec;
		spec.tv_nsec = mts.tv_nsec;
	}

	s = spec.tv_sec;
	ms = round(spec.tv_nsec / 1.0e6);    // Convert nanoseconds to milliseconds
	ms += s * 1000;

	return (unsigned int)ms;
}

int64_t getUSec(bool precise)
{
	timespec ts;

#if defined(ENABLE_CLOCK_GETTIME)
	if (@available(iOS 10.0, *))
	{
		clock_gettime(_CLOCK_MONOTONIC, &ts);
	}
	else
#endif
	{
		// https://stackoverflow.com/questions/5167269/clock-gettime-alternative-in-mac-os-x
		clock_serv_t    cclock;
		mach_timespec_t mts;
		host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &cclock);
		clock_get_time(cclock, &mts);
		mach_port_deallocate(mach_task_self(), cclock);
		ts.tv_sec = mts.tv_sec;
		ts.tv_nsec = mts.tv_nsec;
	}

	long us = (ts.tv_nsec / 1000);
	us += ts.tv_sec * 1e6;
	return us;
}

int64_t getTimerFrequency()
{
	return 1;
}

unsigned getTimeSinceStart()
{
	return (unsigned)time(NULL);
}

CpuInfo* getCpuInfo() {
	return &gCpu;
}
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
#endif

	return true;
}

void updateBaseSubsystems(float deltaTime)
{
	// Not exposed in the interface files / app layer
	extern void platformUpdateLuaScriptingSystem();
	extern void platformUpdateUserInterface(float deltaTime);
	extern void platformUpdateWindowSystem();

	platformUpdateWindowSystem();

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
@interface MetalKitApplication: NSObject<ForgeViewDelegate>

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
uint32_t targetFrameCount = 240;
char benchmarkOutput[1024] = { "\0" };
#endif

// Metal application implementation.
@implementation MetalKitApplication

- (nonnull instancetype)initWithMetalDevice:(nonnull id<MTLDevice>)device
				  renderDestinationProvider:(nonnull id<RenderDestinationProvider, ForgeViewDelegate>)renderDestinationProvider
{
	initCpuInfo(&gCpu);
	initHiresTimer(&deltaTimer);
	
	self = [super init];
	if (self)
	{
		if (!initMemAlloc(pApp->GetName()))
		{
			NSLog(@"Failed to initialize memory manager");
			exit(1);
		}

		//#if TF_USE_MTUNER
		//	rmemInit(0);
		//#endif

		FileSystemInitDesc fsDesc = {};
		fsDesc.pAppName = pApp->GetName();
		if (!initFileSystem(&fsDesc))
		{
			NSLog(@"Failed to initialize filesystem");
			exit(1);
		}

		fsSetPathForResourceDir(pSystemFileIO, RM_DEBUG, RD_LOG, "");
		initLog(pApp->GetName(), DEFAULT_LOG_LEVEL);

		pSettings = &pApp->mSettings;

		gCurrentWindow = {};
		openWindow(pApp->GetName(), &gCurrentWindow, device);
		UIApplication.sharedApplication.delegate.window = (__bridge UIWindow*)gCurrentWindow.handle.window;

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
		//pApp->pWindow = &gCurrentWindow;

#ifdef AUTOMATED_TESTING
	//Check if benchmarking was given through command line
	for (int i = 0; i < pApp->argc; i += 1)
	{
		if (strcmp(pApp->argv[i], "-b") == 0)
		{
			pSettings->mBenchmarking = true;
			if (i + 1 < pApp->argc && isdigit(*(pApp->argv[i + 1])))
				targetFrameCount = min(max(atoi(pApp->argv[i + 1]), 32), 512);
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
			
			if (!pApp->Init())
				exit(1);
			
			setupPlatformUI();
			pApp->mSettings.mInitialized = true;

			if (!pApp->Load(&gReloadDescriptor))
				exit(1);
		}
	}
	
#ifdef AUTOMATED_TESTING
	if (pSettings->mBenchmarking) setAggregateFrames(targetFrameCount / 2);
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
	if(gReloadDescriptor.mType != RELOAD_TYPE_ALL)
	{
		pApp->Unload(&gReloadDescriptor);
		pApp->Load(&gReloadDescriptor);
		
		gReloadDescriptor.mType = RELOAD_TYPE_ALL;
		return;
	}

	float deltaTime = getHiresTimerSeconds(&deltaTimer, true);
	// if framerate appears to drop below about 6, assume we're at a breakpoint and simulate 20fps.
	if (deltaTime > 0.15f)
		deltaTime = 0.05f;
	
	// UPDATE BASE INTERFACES
	updateBaseSubsystems(deltaTime);

	// UPDATE APP
	pApp->Update(deltaTime);
	pApp->Draw();

	if (gShowPlatformUI != pApp->mSettings.mShowPlatformUI)
	{
		togglePlatformUI(); 
	}

#ifdef AUTOMATED_TESTING
	frameCounter++;
	if (frameCounter >= targetFrameCount)
	{
		[self shutdown];
		exit(0);
	}
#endif
}

- (void)shutdown
{
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

	//#if TF_USE_MTUNER
	//	rmemUnload();
	//	rmemShutDown();
	//#endif

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
@interface GameController: NSObject<RenderDestinationProvider>

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
		for (UIView* subview in _view.subviews)
		{
			[subview setContentScaleFactor:pApp->mSettings.mContentScaleFactor];
		}
	}

	if (!_device)
	{
		NSLog(@"Metal is not supported on this device");
	}

	//register terminate callback
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
}

// Called whenever the view needs to render
- (void)draw
{
	static bool lastFocused = true;
	bool focusChanged = false;
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
