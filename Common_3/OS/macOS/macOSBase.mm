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

#import <Cocoa/Cocoa.h>
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
#define MAX_CURSOR_DELTA 200

#define elementsOf(a) (sizeof(a) / sizeof((a)[0]))

namespace {
bool isCaptured = false;
}

static WindowsDesc gCurrentWindow;
static float2      gRetinaScale = { 1.0f, 1.0f };

// TODO: Add multiple window/monitor handling functionality to macOS.
//static tinystl::vector <MonitorDesc> gMonitors;
//static tinystl::unordered_map<void*, WindowsDesc*> gWindowMap;

void adjustWindow(WindowsDesc* winDesc);

namespace PlatformEvents {
extern bool wantsMouseCapture;
extern bool skipMouseCapture;

}

static bool captureMouse(bool shouldCapture, bool shouldHide)
{
	if (shouldCapture != isCaptured)
	{
		if (shouldCapture)
		{
			if (shouldHide)
			{
				CGDisplayHideCursor(kCGDirectMainDisplay);
				CGAssociateMouseAndMouseCursorPosition(false);
			}
			isCaptured = true;
		}
		else
		{
			CGDisplayShowCursor(kCGDirectMainDisplay);
			CGAssociateMouseAndMouseCursorPosition(true);
			isCaptured = false;
		}
	}

	InputSystem::SetMouseCapture(isCaptured && shouldHide);
	return true;
}

#if !defined(METAL)
// TODO: Add multiple monitor handling functionality.
static void collectMonitorInfo()
{
	// TODO: Implement.
	ASSERT(0);
}
void setResolution(const MonitorDesc* pMonitor, const Resolution* pMode)
{
	// TODO: Implement.
	ASSERT(0);
}
#endif

void getRecommendedResolution(RectDesc* rect) { *rect = RectDesc{ 0, 0, 1920, 1080 }; }

void requestShutdown() { [[NSApplication sharedApplication] terminate:[NSApplication sharedApplication]]; }

// TODO: Add multiple window handling functionality.

void openWindow(const char* app_name, WindowsDesc* winDesc) {}

void closeWindow(const WindowsDesc* winDesc) {}

void setWindowRect(WindowsDesc* winDesc, const RectDesc& rect)
{
	RectDesc& currentRect = winDesc->fullScreen ? winDesc->fullscreenRect : winDesc->windowedRect;
	currentRect = rect;

	NSRect winRect;
	winRect.origin.x = currentRect.left;
	winRect.origin.y = currentRect.bottom;
	winRect.size.width = currentRect.right - currentRect.left;
	winRect.size.height = currentRect.top - currentRect.bottom;

	MTKView* view = (MTKView*)CFBridgingRelease(winDesc->handle);
	[view.window setFrame:winRect display:true];
}

void setWindowSize(WindowsDesc* winDesc, unsigned width, unsigned height) { setWindowRect(winDesc, { 0, 0, (int)width, (int)height }); }

void toggleFullscreen(WindowsDesc* winDesc)
{
	MTKView* view = (MTKView*)CFBridgingRelease(winDesc->handle);
	if (!view)
		return;

	bool isFullscreen = ((view.window.styleMask & NSWindowStyleMaskFullScreen) == NSWindowStyleMaskFullScreen);
	winDesc->fullScreen = !isFullscreen;

	[view.window toggleFullScreen:view.window];
	CFRetain(winDesc->handle);
}

void showWindow(WindowsDesc* winDesc)
{
	// TODO: Implement.
	ASSERT(0);
}

void hideWindow(WindowsDesc* winDesc)
{
	// TODO: Implement.
	ASSERT(0);
}

void maximizeWindow(WindowsDesc* winDesc)
{
	winDesc->visible = true;
	MTKView* view = (MTKView*)CFBridgingRelease(winDesc->handle);
	[view.window deminiaturize:nil];
}

void minimizeWindow(WindowsDesc* winDesc)
{
	winDesc->visible = false;
	MTKView* view = (MTKView*)CFBridgingRelease(winDesc->handle);
	[view.window miniaturize:nil];
}

void setMousePositionRelative(const WindowsDesc* winDesc, int32_t x, int32_t y)
{
	CGPoint location;
	location.x = winDesc->windowedRect.left + x;
	location.y = winDesc->windowedRect.bottom - y;
	CGWarpMouseCursorPosition(location);
}

float2 getMousePosition()
{
	NSPoint mouseLoc = [NSEvent mouseLocation];
	return float2((float)mouseLoc.x, (float)mouseLoc.y);
}

bool getKeyDown(int key) { return InputSystem::IsButtonPressed(key); }

bool getKeyUp(int key) { return InputSystem::IsButtonReleased(key); }

bool getJoystickButtonDown(int button)
{
	// TODO: Implement gamepad / joystick support on macOS
	return false;
}

bool getJoystickButtonUp(int button)
{
	// TODO: Implement gamepad / joystick support on macOS
	return false;
}

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

int macOSMain(int argc, const char** argv, IApp* app)
{
	pApp = app;
	return NSApplicationMain(argc, argv);
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
- (void)updateInput;
- (void)update;
- (void)shutdown;

@end

// Our view controller.  Implements the MTKViewDelegate protocol, which allows it to accept
// per-frame update and drawable resize callbacks.  Also implements the RenderDestinationProvider
// protocol, which allows our renderer object to get and set drawable properties such as pixel
// format and sample count
@interface GameViewController: NSViewController<MTKViewDelegate, RenderDestinationProvider>

@end

/************************************************************************/
// GameViewController implementation
/************************************************************************/

@implementation GameViewController
{
	MTKView*             _view;
	id<MTLDevice>        _device;
	MetalKitApplication* _application;
}

- (void)viewDidLoad
{
	[super viewDidLoad];

	// Set the view to use the default device
	_device = MTLCreateSystemDefaultDevice();
	_view = (MTKView*)self.view;
	_view.delegate = self;
	_view.device = _device;
	_view.paused = NO;
	_view.enableSetNeedsDisplay = NO;
	_view.preferredFramesPerSecond = 60.0;
	[_view.window makeFirstResponder:self];
	_view.autoresizesSubviews = YES;
	isCaptured = false;

	// Adjust window size to match retina scaling.
	gRetinaScale = { (float)(_view.drawableSize.width / _view.frame.size.width),
					 (float)(_view.drawableSize.height / _view.frame.size.height) };
	NSSize windowSize = CGSizeMake(_view.frame.size.width / gRetinaScale.x, _view.frame.size.height / gRetinaScale.y);
	[_view.window setContentSize:windowSize];
	[_view.window setCollectionBehavior:NSWindowCollectionBehaviorFullScreenPrimary];

	// Kick-off the MetalKitApplication.
	_application = [[MetalKitApplication alloc] initWithMetalDevice:_device renderDestinationProvider:self view:_view];

	if (!_device)
	{
		NSLog(@"Metal is not supported on this device");
		self.view = [[NSView alloc] initWithFrame:self.view.frame];
	}

	//register terminate callback
	NSApplication* app = [NSApplication sharedApplication];
	[[NSNotificationCenter defaultCenter] addObserver:self
											 selector:@selector(applicationWillTerminate:)
												 name:NSApplicationWillTerminateNotification
											   object:app];
}

/*A notification named NSApplicationWillTerminateNotification.*/
- (void)applicationWillTerminate:(NSNotification*)notification
{
	[_application shutdown];
}

- (BOOL)acceptsFirstResponder
{
	return TRUE;
}

- (BOOL)canBecomeKeyView
{
	return TRUE;
}

// Called whenever view changes orientation or layout is changed
- (void)mtkView:(nonnull MTKView*)view drawableSizeWillChange:(CGSize)size
{
	view.window.contentView = _view;
	[_application drawRectResized:view.bounds.size];
}

// Called whenever the view needs to render
- (void)drawInMTKView:(nonnull MTKView*)view
{
	@autoreleasepool
	{
		[_application update];
		[_application updateInput];
		InputSystem::Update();
		//this is needed for NON Vsync mode.
		//This enables to force update the display
		if (_view.enableSetNeedsDisplay == YES)
			[_view setNeedsDisplay:YES];
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
		}
		else
		{
			//if width and height were set manually in App constructor
			//then override and set window size to user width/height.
			//That means we now render at size * gRetinaScale.
			//TODO: make sure pSettings->mWidth determines window size and not drawable size as on retina displays we need to make sure that's what user wants.
			NSSize windowSize = CGSizeMake(pSettings->mWidth, pSettings->mHeight);
			[view.window setContentSize:windowSize];
			[view setFrameSize:windowSize];
		}

		gCurrentWindow = {};
		gCurrentWindow.windowedRect = { 0, 0, (int)pSettings->mWidth, (int)pSettings->mHeight };
		gCurrentWindow.fullScreen = pSettings->mFullScreen;
		gCurrentWindow.maximized = false;
		gCurrentWindow.handle = (void*)CFBridgingRetain(view);
		openWindow(pApp->GetName(), &gCurrentWindow);

		pSettings->mWidth =
			gCurrentWindow.fullScreen ? getRectWidth(gCurrentWindow.fullscreenRect) : getRectWidth(gCurrentWindow.windowedRect);
		pSettings->mHeight =
			gCurrentWindow.fullScreen ? getRectHeight(gCurrentWindow.fullscreenRect) : getRectHeight(gCurrentWindow.windowedRect);
		pApp->pWindow = &gCurrentWindow;

		InputSystem::Init(pSettings->mWidth, pSettings->mHeight);
		InputSystem::InitSubView((__bridge void*)(view));

		@autoreleasepool
		{
			//if init fails then exit the app
			if (!pApp->Init())
			{
				for (NSWindow* window in [NSApplication sharedApplication].windows)
				{
					[window close];
				}

				exit(1);
			}

			//if load fails then exit the app
			if (!pApp->Load())
			{
				for (NSWindow* window in [NSApplication sharedApplication].windows)
				{
					[window close];
				}

				exit(1);
			}
		}
	}

	return self;
}

- (void)drawRectResized:(CGSize)size
{
	float newWidth = size.width * gRetinaScale.x;
	float newHeight = size.height * gRetinaScale.y;

	if (newWidth != pApp->mSettings.mWidth || newHeight != pApp->mSettings.mHeight)
	{
		pApp->mSettings.mWidth = newWidth;
		pApp->mSettings.mHeight = newHeight;
		pApp->Unload();
		pApp->Load();
	}
	// Used to notify input backend of new display size
	InputSystem::UpdateSize(newWidth, newHeight);
}

- (void)updateInput
{
	if (InputSystem::IsButtonTriggered(UserInputKeys::KEY_CANCEL))
	{
		if (!isCaptured && !gCurrentWindow.fullScreen)
		{
			[NSApp terminate:self];
		}
		else
		{
			captureMouse(false, InputSystem::GetHideMouseCursorWhileCaptured());
		}
	}

	if (InputSystem::IsButtonTriggered(UserInputKeys::KEY_CONFIRM))
	{
		if (!InputSystem::IsMouseCaptured() && !PlatformEvents::skipMouseCapture)
		{
			captureMouse(true, InputSystem::GetHideMouseCursorWhileCaptured());
		}
	}

	//if alt (left or right) is pressed and Enter is triggered then toggle fullscreen
	if ((InputSystem::IsButtonPressed(UserInputKeys::KEY_LEFT_ALT) || InputSystem::IsButtonPressed(UserInputKeys::KEY_RIGHT_ALT)) &&
		InputSystem::IsButtonReleased(UserInputKeys::KEY_MENU))
	{
		//get first window available.
		//TODO:Fix this once we have multiple window handles
		toggleFullscreen(&gCurrentWindow);
	}
}

- (void)update
{
	float deltaTime = deltaTimer.GetMSec(true) / 1000.0f;
	// if framerate appears to drop below about 6, assume we're at a breakpoint and simulate 20fps.
	if (deltaTime > 0.15f)
		deltaTime = 0.05f;

	pApp->Update(deltaTime);
	pApp->Draw();

#ifdef AUTOMATED_TESTING
	testingCurrentFrameCount++;
	if (testingCurrentFrameCount >= testingMaxFrameCount)
	{
		for (NSWindow* window in [NSApplication sharedApplication].windows)
		{
			[window close];
		}

		[NSApp terminate:nil];
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
