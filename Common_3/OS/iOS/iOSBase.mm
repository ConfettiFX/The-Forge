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

#include "../../ThirdParty/OpenSource/EASTL/vector.h"

#include "../Interfaces/IOperatingSystem.h"
#include "../Interfaces/ILog.h"
#include "../Interfaces/ITime.h"
#include "../Interfaces/IThread.h"
#include "../Interfaces/IFileSystem.h"
#include "../Interfaces/IApp.h"

#include "../Input/InputSystem.h"
#include "../Input/InputMappings.h"

#include "../Interfaces/IMemory.h"

#define CONFETTI_WINDOW_CLASS L"confetti"
#define MAX_KEYS 256

#define elementsOf(a) (sizeof(a) / sizeof((a)[0]))

static WindowsDesc gCurrentWindow;
static eastl::vector<MonitorDesc> gMonitors;
static int gCurrentTouchEvent = 0;

static float2 gRetinaScale = { 1.0f, 1.0f };
static int gDeviceWidth;
static int gDeviceHeight;

static eastl::vector<MonitorDesc> monitors;

// Update the state of the keys based on state previous frame
void updateTouchEvent(int numTaps) { gCurrentTouchEvent = numTaps; }

int getTouchEvent()
{
	int prevTouchEvent = gCurrentTouchEvent;
	gCurrentTouchEvent = 0;
	return prevTouchEvent;
}

void getRecommendedResolution(RectDesc* rect) { *rect = RectDesc{ 0, 0, gDeviceWidth, gDeviceHeight }; }

/************************************************************************/
// Time Related Functions
/************************************************************************/

unsigned getSystemTime()
{
	long			ms; // Milliseconds
	time_t		s;  // Seconds
	struct timespec spec;

	clock_gettime(CLOCK_REALTIME, &spec);

	s  = spec.tv_sec;
	ms = round(spec.tv_nsec / 1.0e6); // Convert nanoseconds to milliseconds

	ms += s * 1000;

	return (unsigned int)ms;
}

int64_t getUSec()
{
	timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	long us = (ts.tv_nsec / 1000);
	us += ts.tv_sec * 1e6;
	return us;
}

int64_t getTimerFrequency()
{
    return 1;
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

@protocol ForgeViewDelegate <NSObject>
@required
-(void)drawRectResized:(CGSize)size;
@end

@interface ForgeMTLViewController : UIViewController
-(id) initWithFrame:(CGRect)FrameRect device:(id<MTLDevice>)device display:(int)displayID hdr:(bool)hdr vsync:(bool)vsync;
@end

@interface ForgeMTLView: UIView
+(Class)layerClass;
-(void)layoutSubviews;
@property (nonatomic,weak) id<ForgeViewDelegate> delegate;

@end

@implementation ForgeMTLView

+(Class)layerClass
{
    return [CAMetalLayer class];
}

-(void)layoutSubviews
{
    [_delegate drawRectResized:self.bounds.size];
    if (pApp->mSettings.mContentScaleFactor >= 1.f)
    {
        [self setContentScaleFactor:pApp->mSettings.mContentScaleFactor];
        for (UIView *subview in self.subviews)
        {
            [subview setContentScaleFactor:pApp->mSettings.mContentScaleFactor];
        }
        ((CAMetalLayer*)(self.layer)).drawableSize = CGSizeMake(self.frame.size.width * pApp->mSettings.mContentScaleFactor, self.frame.size.height * pApp->mSettings.mContentScaleFactor);
    }
    else
    {
        [self setContentScaleFactor:gRetinaScale.x];
        for (UIView *subview in self.subviews)
        {
            [subview setContentScaleFactor:gRetinaScale.x];
        }

        ((CAMetalLayer*)(self.layer)).drawableSize = CGSizeMake(self.frame.size.width * gRetinaScale.x, self.frame.size.height * gRetinaScale.y);
    }
}
@end

@implementation ForgeMTLViewController

-(id)initWithFrame:(CGRect)FrameRect device:(id<MTLDevice>)device display:(int)in_displayID hdr:(bool)hdr vsync:(bool)vsync
{
    self = [super init];
    self.view = [[ForgeMTLView alloc] initWithFrame:FrameRect];
    CAMetalLayer *metalLayer = (CAMetalLayer*)self.view.layer;
    
    metalLayer.device =  device;
    metalLayer.framebufferOnly = YES; //todo: optimized way
    metalLayer.pixelFormat = hdr? MTLPixelFormatRGBA16Float : MTLPixelFormatBGRA8Unorm;
    metalLayer.drawableSize = CGSizeMake(self.view.frame.size.width, self.view.frame.size.height);
    
    return self;
}

- (BOOL)prefersStatusBarHidden
{
    return pApp->mSettings.mShowStatusBar ? NO : YES;
}

@end

void openWindow(const char* app_name, WindowsDesc* winDesc, id<MTLDevice> device)
{
    CGRect ViewRect {0,0, 1280, 720 }; //Initial default values
    UIWindow* Window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
    
    [Window setOpaque:YES];
    [Window makeKeyAndVisible];
    winDesc->handle.window = (void*)CFBridgingRetain(Window);
    
    // Adjust window size to match retina scaling.
    CGFloat scale = UIScreen.mainScreen.scale;
    if (pApp->mSettings.mContentScaleFactor >= 1.0f)
        gRetinaScale = { (float)pApp->mSettings.mContentScaleFactor, (float)pApp->mSettings.mContentScaleFactor };
    else
        gRetinaScale = { (float)scale, (float)scale };
    
    gDeviceWidth = UIScreen.mainScreen.bounds.size.width;
    gDeviceHeight = UIScreen.mainScreen.bounds.size.height;
    
    ForgeMTLViewController *ViewController = [[ForgeMTLViewController alloc] initWithFrame:ViewRect device:device display:0 hdr:NO vsync:NO];
    [Window setRootViewController:ViewController];
}

// Protocol abstracting the platform specific view in order to keep the Renderer class independent from platform
@protocol RenderDestinationProvider
-(void)draw;
@end

// Interface that controls the main updating/rendering loop on Metal appplications.
@interface MetalKitApplication: NSObject<ForgeViewDelegate>

- (nonnull instancetype)initWithMetalDevice:(nonnull id<MTLDevice>)device
                  renderDestinationProvider:(nonnull id<RenderDestinationProvider>)renderDestinationProvider;

- (void)drawRectResized:(CGSize)size;

- (void)update;

- (void)shutdown;

@end

// Our view controller.  Implements the MTKViewDelegate protocol, which allows it to accept
// per-frame update and drawable resize callbacks.  Also implements the RenderDestinationProvider
// protocol, which allows our renderer object to get and set drawable properties such as pixel
// format and sample count
@interface GameController: NSObject<RenderDestinationProvider>

@end

GameController* pMainViewController;
/************************************************************************/
// GameController implementation
/************************************************************************/

@implementation GameController
{
	ForgeMTLView*             _view;
	id<MTLDevice> _device;
	MetalKitApplication* _application;
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
    
    _view = (ForgeMTLView*)UIApplication.sharedApplication.keyWindow.rootViewController.view;

	// Enable multi-touch in our apps.
	[_view setMultipleTouchEnabled:true];
	_view.autoresizingMask = UIViewAutoresizingFlexibleHeight | UIViewAutoresizingFlexibleWidth;
	
	if (pApp->mSettings.mContentScaleFactor >= 1.f)
	{
		[_view setContentScaleFactor:pApp->mSettings.mContentScaleFactor];
		for (UIView *subview in _view.subviews)
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

/*A notification named NSApplicationWillTerminateNotification.*/
- (void)applicationWillTerminate:(UIApplication*)app
{
	[_application shutdown];
}

// Called whenever the view needs to render
- (void)draw
{
	[_application update];
}
@end

/************************************************************************/
// MetalKitApplication implementation
/************************************************************************/

// Timer used in the update function.
Timer deltaTimer;
IApp::Settings* pSettings;
#ifdef AUTOMATED_TESTING
uint32_t testingCurrentFrameCount;
uint32_t testingMaxFrameCount = 120;
#endif

// Metal application implementation.
@implementation MetalKitApplication

- (nonnull instancetype)initWithMetalDevice:(nonnull id<MTLDevice>)device
				  renderDestinationProvider:(nonnull id<RenderDestinationProvider, ForgeViewDelegate>)renderDestinationProvider
{
	self = [super init];
	if (self)
	{
		FileSystem::SetCurrentDir(FileSystem::GetProgramDir());

		pSettings = &pApp->mSettings;

        gCurrentWindow = {};
        openWindow(pApp->GetName(), &gCurrentWindow, device);
        UIApplication.sharedApplication.delegate.window = (__bridge UIWindow*)gCurrentWindow.handle.window;
        
		if (pSettings->mWidth == -1 || pSettings->mHeight == -1)
		{
			RectDesc rect = {};
			getRecommendedResolution(&rect);
			pSettings->mWidth = getRectWidth(rect);
			pSettings->mHeight = getRectHeight(rect);
			pSettings->mFullScreen = true;
		}

		gCurrentWindow.fullscreenRect = { 0, 0, (int)pSettings->mWidth, (int)pSettings->mHeight };
		gCurrentWindow.fullScreen = pSettings->mFullScreen;
		gCurrentWindow.maximized = false;

        ForgeMTLView *forgeView = (ForgeMTLView*)((__bridge UIWindow*)(gCurrentWindow.handle.window)).rootViewController.view;
        forgeView.delegate = self;
        
		pSettings->mWidth = getRectWidth(gCurrentWindow.fullscreenRect);
		pSettings->mHeight = getRectHeight(gCurrentWindow.fullscreenRect);
		pApp->pWindow = &gCurrentWindow;

        InputSystem::Init(gDeviceWidth, gDeviceHeight);
        InputSystem::InitSubView((__bridge void*)forgeView);
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
		pApp->Unload();
		pApp->Load();
	}
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
			[self shutdown];
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
