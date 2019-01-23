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

#ifdef __APPLE__

#import <UIKit/UIKit.h>
#import "AppDelegate.h"
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>

#include <ctime>

#include "../../ThirdParty/OpenSource/TinySTL/vector.h"

#include "../Interfaces/IOperatingSystem.h"
#include "../Interfaces/IPlatformEvents.h"
#include "../Interfaces/ILogManager.h"
#include "../Interfaces/ITimeManager.h"
#include "../Interfaces/IThread.h"
#include "../Interfaces/IFileSystem.h"
#include "../Interfaces/IApp.h"

#include "../../../Middleware_3/Input/InputSystem.h"
#include "../../../Middleware_3/Input/InputMappings.h"

#include "../Interfaces/IMemoryManager.h"

#define CONFETTI_WINDOW_CLASS L"confetti"
#define MAX_KEYS 256

#define elementsOf(a) (sizeof(a) / sizeof((a)[0]))

static WindowsDesc                  gCurrentWindow;
static tinystl::vector<MonitorDesc> gMonitors;
static int                          gCurrentTouchEvent = 0;

static float2 gRetinaScale = { 1.0f, 1.0f };
static int    gDeviceWidth;
static int    gDeviceHeight;

static tinystl::vector<MonitorDesc> monitors;

// Update the state of the keys based on state previous frame
void updateTouchEvent(int numTaps) { gCurrentTouchEvent = numTaps; }

int getTouchEvent()
{
	int prevTouchEvent = gCurrentTouchEvent;
	gCurrentTouchEvent = 0;
	return prevTouchEvent;
}

void getRecommendedResolution(RectDesc* rect) { *rect = RectDesc{ 0, 0, gDeviceWidth, gDeviceHeight }; }

bool getKeyDown(int key) { return InputSystem::IsButtonPressed(key); }

bool getKeyUp(int key) { return InputSystem::IsButtonReleased(key); }

/************************************************************************/
// Time Related Functions
/************************************************************************/

unsigned getSystemTime()
{
	long            ms;    // Milliseconds
	time_t          s;     // Seconds
	struct timespec spec;

	clock_gettime(CLOCK_REALTIME, &spec);

	s = spec.tv_sec;
	ms = round(spec.tv_nsec / 1.0e6);    // Convert nanoseconds to milliseconds

	ms += s * 1000;

	return (unsigned int)ms;
}

long long getUSec()
{
	timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	long us = (ts.tv_nsec / 1000);
	us += ts.tv_sec * 1e6;
	return us;
}

unsigned getTimeSinceStart() { return (unsigned)time(NULL); }

float2 getDpiScale() { return gRetinaScale; }
/************************************************************************/
// App Entrypoint
/************************************************************************/

static IApp* pApp = NULL;

int iOSMain(int argc, char** argv, IApp* app)
{
	pApp = app;
	@autoreleasepool
	{
		return UIApplicationMain(argc, argv, nil, NSStringFromClass([AppDelegate class]));
	}
}

// Protocol abstracting the platform specific view in order to keep the Renderer class independent from platform
@protocol RenderDestinationProvider

@end

// Interface that controls the main updating/rendering loop on Metal appplications.
@interface MetalKitApplication: NSObject

- (nonnull instancetype)initWithMetalDevice:(nonnull id<MTLDevice>)device
				  renderDestinationProvider:(nonnull id<RenderDestinationProvider>)renderDestinationProvider
									   view:(nonnull MTKView*)view;

- (void)drawRectResized:(CGSize)size;

- (void)update;

- (void)shutdown;

@end

// Our view controller.  Implements the MTKViewDelegate protocol, which allows it to accept
// per-frame update and drawable resize callbacks.  Also implements the RenderDestinationProvider
// protocol, which allows our renderer object to get and set drawable properties such as pixel
// format and sample count
@interface GameViewController: UIViewController<MTKViewDelegate, RenderDestinationProvider>

@end

UIViewController* pMainViewController;
/************************************************************************/
// GameViewController implementation
/************************************************************************/

@implementation GameViewController
{
	MTKView*             _view;
	id<MTLDevice>        _device;
	MetalKitApplication* _application;
}

- (void)dealloc
{
	@autoreleasepool
	{
		[_application shutdown];
	}
}

- (void)viewDidLoad
{
	[super viewDidLoad];

	pMainViewController = self;
	// Set the view to use the default device
	_device = MTLCreateSystemDefaultDevice();
	_view = (MTKView*)self.view;
	_view.delegate = self;
	_view.device = _device;

	// Get the device's width and height.
	gDeviceWidth = _view.drawableSize.width;
	gDeviceHeight = _view.drawableSize.height;
	gRetinaScale = { (float)(_view.drawableSize.width / _view.frame.size.width),
					 (float)(_view.drawableSize.height / _view.frame.size.height) };

	// Enable multi-touch in our apps.
	[_view setMultipleTouchEnabled:true];
	_view.autoresizingMask = UIViewAutoresizingFlexibleHeight | UIViewAutoresizingFlexibleWidth;
	_view.autoResizeDrawable = TRUE;

	// Kick-off the MetalKitApplication.
	_application = [[MetalKitApplication alloc] initWithMetalDevice:_device renderDestinationProvider:self view:_view];

	if (!_device)
	{
		NSLog(@"Metal is not supported on this device");
		self.view = [[UIView alloc] initWithFrame:self.view.frame];
	}

	//register terminate callback
	UIApplication* app = [UIApplication sharedApplication];
	[[NSNotificationCenter defaultCenter] addObserver:self
											 selector:@selector(applicationWillTerminate:)
												 name:UIApplicationWillTerminateNotification
											   object:app];
}

/*A notification named NSApplicationWillTerminateNotification.*/
- (void)applicationWillTerminate:(UIApplication*)app
{
	[_application shutdown];
}

- (BOOL)prefersStatusBarHidden
{
	return pApp->mSettings.mShowStatusBar ? NO : YES;
}

// Called whenever view changes orientation or layout is changed
- (void)mtkView:(nonnull MTKView*)view drawableSizeWillChange:(CGSize)size
{
	[_application drawRectResized:view.bounds.size];
}

// Called whenever the view needs to render
- (void)drawInMTKView:(nonnull MTKView*)view
{
	@autoreleasepool
	{
		[_application update];
	}
}
@end

/************************************************************************/
// MetalKitApplication implementation
/************************************************************************/

// Timer used in the update function.
Timer           deltaTimer;
IApp::Settings* pSettings;
#ifdef AUTOMATED_TESTING
uint32_t testingCurrentFrameCount;
uint32_t testingMaxFrameCount = 120;
#endif

// Metal application implementation.
@implementation MetalKitApplication
{
}
- (nonnull instancetype)initWithMetalDevice:(nonnull id<MTLDevice>)device
				  renderDestinationProvider:(nonnull id<RenderDestinationProvider>)renderDestinationProvider
									   view:(nonnull MTKView*)view
{
	self = [super init];
	if (self)
	{
		FileSystem::SetCurrentDir(FileSystem::GetProgramDir());

		pSettings = &pApp->mSettings;

		if (pSettings->mWidth == -1 || pSettings->mHeight == -1)
		{
			RectDesc rect = {};
			getRecommendedResolution(&rect);
			pSettings->mWidth = getRectWidth(rect);
			pSettings->mHeight = getRectHeight(rect);
			pSettings->mFullScreen = true;
		}

		gCurrentWindow = {};
		gCurrentWindow.fullscreenRect = { 0, 0, (int)pSettings->mWidth, (int)pSettings->mHeight };
		gCurrentWindow.fullScreen = pSettings->mFullScreen;
		gCurrentWindow.maximized = false;
		gCurrentWindow.handle = (void*)CFBridgingRetain(view);
		//openWindow(pApp->GetName(), &gCurrentWindow);

		pSettings->mWidth = getRectWidth(gCurrentWindow.fullscreenRect);
		pSettings->mHeight = getRectHeight(gCurrentWindow.fullscreenRect);
		pApp->pWindow = &gCurrentWindow;

		InputSystem::Init(gDeviceWidth, gDeviceHeight);
		InputSystem::InitSubView((__bridge void*)view);
		// App init may override those
		// Mouse captured to true on iOS
		// Set HideMouse to false so that UI can always be picked.
		InputSystem::SetMouseCapture(true);
		InputSystem::SetHideMouseCursorWhileCaptured(false);

		@autoreleasepool
		{
			if (!pApp->Init())
				exit(1);

			if (!pApp->Load())
				exit(1);
		}
	}

	return self;
}

- (void)drawRectResized:(CGSize)size
{
	pApp->mSettings.mWidth = size.width * gRetinaScale.x;
	pApp->mSettings.mHeight = size.height * gRetinaScale.y;
	pApp->mSettings.mFullScreen = true;
	pApp->Unload();
	pApp->Load();
}

- (void)update
{
	float deltaTime = deltaTimer.GetMSec(true) / 1000.0f;
	// if framerate appears to drop below about 6, assume we're at a breakpoint and simulate 20fps.
	if (deltaTime > 0.15f)
		deltaTime = 0.05f;

	InputSystem::Update();
	pApp->Update(deltaTime);
	pApp->Draw();

#ifdef AUTOMATED_TESTING
	testingCurrentFrameCount++;
	if (testingCurrentFrameCount >= testingMaxFrameCount)
	{
		exit(0);
	}
#endif
}

- (void)shutdown
{
	InputSystem::Shutdown();
	pApp->Unload();
	pApp->Exit();
}
@end
/************************************************************************/
/************************************************************************/
#endif
