/*
 * Copyright (c) 2018-2020 The Forge Interactive Inc.
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
#import <IOKit/graphics/IOGraphicsLib.h>

#include <ctime>

#include <mach/clock.h>
#include <mach/mach.h>

#include "../../ThirdParty/OpenSource/EASTL/vector.h"
#include "../../ThirdParty/OpenSource/rmem/inc/rmem.h"

#include "../Interfaces/IOperatingSystem.h"
#include "../Interfaces/ILog.h"
#include "../Interfaces/ITime.h"
#include "../Interfaces/IThread.h"
#include "../Interfaces/IFileSystem.h"
#include "../Interfaces/IApp.h"
#include "../Interfaces/IMemory.h"

#define CONFETTI_WINDOW_CLASS L"confetti"
#define MAX_KEYS 256
#define MAX_CURSOR_DELTA 200

#define elementsOf(a) (sizeof(a) / sizeof((a)[0]))

float2      gRetinaScale = { 1.0f, 1.0f };
MonitorDesc* gMonitors = nullptr;
uint32_t gMonitorCount = 0;

// Protocol abstracting the platform specific view in order to keep the Renderer class independent from platform
@protocol RenderDestinationProvider
-(void)draw;
-(void)didResize:(CGSize)size;
@end

@interface ForgeMTLView : NSView <NSWindowDelegate>
{
@private
    CVDisplayLinkRef    displayLink;
    CAMetalLayer        *metalLayer;
    
}
@property (weak) id<RenderDestinationProvider> delegate;

-(id) initWithFrame:(NSRect)FrameRect device:(id<MTLDevice>)device display:(int)displayID hdr:(bool)hdr vsync:(bool)vsync;
- (CVReturn)getFrameForTime:(const CVTimeStamp*)outputTime;
@end

@implementation ForgeMTLView

-(id)initWithFrame:(NSRect)FrameRect device:(id<MTLDevice>)device display:(int)in_displayID hdr:(bool)hdr vsync:(bool)vsync
{
    self = [super initWithFrame:FrameRect];
    self.wantsLayer = YES;
    
    metalLayer = [CAMetalLayer layer];
    metalLayer.device =  device;
    metalLayer.framebufferOnly = YES; //todo: optimized way
    metalLayer.pixelFormat = hdr ? MTLPixelFormatRGBA16Float : MTLPixelFormatBGRA8Unorm;
    metalLayer.wantsExtendedDynamicRangeContent = hdr? true : false;
    metalLayer.drawableSize = CGSizeMake(self.frame.size.width, self.frame.size.height);
#if defined(ENABLE_DISPLAY_SYNC_TOGGLE)
	if (@available(macOS 10.13, *))
	{
		metalLayer.displaySyncEnabled = vsync;
	}
#endif
    self.layer = metalLayer;
    
    [self setAutoresizingMask:NSViewWidthSizable|NSViewHeightSizable];
    
    return self;
}

- (CVReturn)getFrameForTime:(const CVTimeStamp*)outputTime
{
    // Called whenever the view needs to render
    // Need to dispatch to main thread as CVDisplayLink uses it's own thread.
    dispatch_sync(dispatch_get_main_queue(), ^{
        [self.delegate draw];
    });
   
    return kCVReturnSuccess;
}

- (void)windowDidResize:(NSNotification *)notification
{
    NSWindow* resizedWindow = [notification object];
    NSRect viewSize = [resizedWindow.contentView frame];
    [self.delegate didResize: viewSize.size];
    metalLayer.drawableSize = CGSizeMake(self.frame.size.width * gRetinaScale.x, self.frame.size.height * gRetinaScale.y);
 }

@end

namespace {
bool isCaptured = false;
}

static WindowsDesc gCurrentWindow;

void adjustWindow(WindowsDesc* winDesc);

static void collectMonitorInfo()
{
	uint32_t displayCount = 0;
	CGError error = CGGetOnlineDisplayList(0, nil, &displayCount);
	if(error != kCGErrorSuccess)
	{
		ASSERT(0);
	}
	
	eastl::vector<CGDirectDisplayID> onlineDisplayIDs(displayCount);
	
	error = CGGetOnlineDisplayList(displayCount, onlineDisplayIDs.data(), &displayCount);
	if(error != kCGErrorSuccess)
	{
		ASSERT(0);
	}
	
	ASSERT(displayCount != 0);
	
	gMonitorCount = displayCount;
	gMonitors = (MonitorDesc*)conf_calloc(displayCount, sizeof(MonitorDesc));
	
	CFMutableDictionaryRef matchingService = IOServiceMatching("IODisplayConnect");
	
	io_iterator_t serviceIterator = 0;
	kern_return_t serviceError = IOServiceGetMatchingServices(kIOMasterPortDefault, matchingService, &serviceIterator);
	if(serviceError != 0)
	{
		ASSERT(0);
	}
	
	io_service_t service = 0;
	uint32_t index = 0;
	CGDirectDisplayID mainDisplayID = CGMainDisplayID();
	while ((service = IOIteratorNext(serviceIterator)) != 0)
	{
		CFDictionaryRef infoDictionary =
			(CFDictionaryRef)IODisplayCreateInfoDictionary(service,
														   kIODisplayOnlyPreferredName);
		
		CFIndex vendorID, productID;
		CFNumberGetValue((CFNumberRef)CFDictionaryGetValue(infoDictionary, CFSTR(kDisplayVendorID)),
						 kCFNumberCFIndexType,
						 &vendorID);
		CFNumberGetValue((CFNumberRef)CFDictionaryGetValue(infoDictionary, CFSTR(kDisplayProductID)),
						 kCFNumberCFIndexType,
						 &productID);
		
		CGDirectDisplayID displayID = 0;
		for(CGDirectDisplayID currentDisplayID : onlineDisplayIDs)
		{
			CFIndex currentVendorID = CGDisplayVendorNumber(currentDisplayID);
			CFIndex currentProductID = CGDisplayModelNumber(currentDisplayID);
			
			if(currentVendorID == vendorID &&
			   currentProductID == productID)
			{
				displayID = currentDisplayID;
				break;
			}
		}
		
		if(displayID == 0) continue;
		
		MonitorDesc& display = gMonitors[index];
		
		display.displayID = displayID;
		
		CFDictionaryRef localizedNames =
			(CFDictionaryRef)CFDictionaryGetValue(infoDictionary,
												  CFSTR(kDisplayProductName));
		void* key = nil;
		CFDictionaryGetKeysAndValues(localizedNames, (const void **)&key, nil);
		
		NSString* displayName = (NSString*)CFDictionaryGetValue(localizedNames, key);
		strcpy(display.publicDisplayName, displayName.UTF8String);
		
		id<MTLDevice> metalDevice = CGDirectDisplayCopyCurrentMetalDevice(displayID);
		strcpy(display.publicAdapterName, metalDevice.name.UTF8String);
		
		CGRect displayBounds = CGDisplayBounds(displayID);
		display.workRect =
		{
			(int32_t)displayBounds.origin.x,
			(int32_t)displayBounds.origin.y,
			(int32_t)displayBounds.size.width,
			(int32_t)displayBounds.size.height
		};
		display.monitorRect = display.workRect;
		
		display.defaultResolution.mWidth = (uint32_t)CGDisplayPixelsWide(displayID);
		display.defaultResolution.mHeight = (uint32_t)CGDisplayPixelsHigh(displayID);
		
		eastl::vector<Resolution> displayResolutions;
		CFArrayRef displayModes = CGDisplayCopyAllDisplayModes(displayID, nil);
		for(CFIndex i = 0; i < CFArrayGetCount(displayModes); ++i)
		{
			CGDisplayModeRef currentDisplayMode = (CGDisplayModeRef)CFArrayGetValueAtIndex(displayModes, i);
			
			Resolution displayResolution;
			displayResolution.mWidth = (uint32_t)CGDisplayModeGetWidth(currentDisplayMode);
			displayResolution.mHeight = (uint32_t)CGDisplayModeGetHeight(currentDisplayMode);
			
			displayResolutions.emplace_back(displayResolution);
		}
		
		qsort(displayResolutions.data(),
			  displayResolutions.size(),
			  sizeof(Resolution),
			  [](const void* lhs, const void* rhs)
		{
			Resolution* pLhs = (Resolution*)lhs;
			Resolution* pRhs = (Resolution*)rhs;
			if (pLhs->mHeight == pRhs->mHeight)
				return (int)(pLhs->mWidth - pRhs->mWidth);
			return (int)(pLhs->mHeight - pRhs->mHeight);
		});
		
		display.resolutionCount = (uint32_t)displayResolutions.size();
		display.resolutions = (Resolution*)conf_calloc(display.resolutionCount, sizeof(Resolution));
		memcpy(display.resolutions, displayResolutions.data(), display.resolutionCount * sizeof(Resolution));
		
		CFRelease(displayModes);
		
		if(displayID == mainDisplayID && index != 0)
		{
			std::swap(gMonitors[0], gMonitors[index]);
		}
		
		CFRelease(infoDictionary);
		
		++index;
	}
}

void setResolution(const MonitorDesc* pMonitor, const Resolution* pMode)
{
	ASSERT(pMonitor != nil);
	ASSERT(pMode != nil);
	
	CGDirectDisplayID display = pMonitor->displayID;
	CFArrayRef displayModes = CGDisplayCopyAllDisplayModes(display, nil);
	
	ASSERT(displayModes != nil);
	
	int64_t smallestDiff = std::numeric_limits<int64_t>::max();
	CGDisplayModeRef bestMode = nil;
	for(CFIndex i = 0; i < CFArrayGetCount(displayModes); ++i)
	{
		CGDisplayModeRef mode = (CGDisplayModeRef)CFArrayGetValueAtIndex(displayModes, i);
		int64_t width = (int64_t)CGDisplayModeGetWidth(mode);
		int64_t height = (int64_t)CGDisplayModeGetHeight(mode);
		
		int64_t desiredWidth = pMode->mWidth;
		int64_t desiredHeight = pMode->mHeight;
		
		int64_t diffw = width - desiredWidth;
		int64_t diffw2 = diffw * diffw;
		
		int64_t diffh = height - desiredHeight;
		int64_t diffh2 = diffh * diffh;
		
		int64_t lenSq = diffw2 + diffh2;
		if(smallestDiff > lenSq)
		{
			smallestDiff = lenSq;
			bestMode = mode;
		}
	}
	
	ASSERT(bestMode != nil);
	
	size_t width = CGDisplayModeGetWidth(bestMode);
	size_t height = CGDisplayModeGetHeight(bestMode);
	if(pMode->mWidth == width && pMode->mHeight == height)
	{
		return;
	}
	
	CGDisplayConfigRef configRef;
	CGError error = CGBeginDisplayConfiguration(&configRef);
	if(error != kCGErrorSuccess)
	{
		ASSERT(0);
	}
	
	error = CGConfigureDisplayWithDisplayMode(configRef, display, bestMode, nil);
	if(error != kCGErrorSuccess)
	{
		ASSERT(0);
	}
	
	error = CGCompleteDisplayConfiguration(configRef, kCGConfigureForSession);
	if(error != kCGErrorSuccess)
	{
		ASSERT(0);
	}
	
	CFRelease(displayModes);
}

void getRecommendedResolution(RectDesc* rect) { *rect = RectDesc{ 0, 0, 1920, 1080 }; }

void requestShutdown() { [[NSApplication sharedApplication] terminate:[NSApplication sharedApplication]]; }

void openWindow(const char* app_name, WindowsDesc* winDesc, id<MTLDevice> device, id<RenderDestinationProvider> delegateRenderProvider)
{
    NSRect ViewRect {0,0, (float)winDesc->windowedRect.right - winDesc->windowedRect.left, (float)winDesc->windowedRect.bottom - winDesc->windowedRect.top };
    NSWindow* Window = [[NSWindow alloc] initWithContentRect:ViewRect
                                         styleMask: (NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable)
                                           backing:NSBackingStoreBuffered
                                             defer:YES];
    [Window setAcceptsMouseMovedEvents:YES];
    [Window setTitle:[NSString stringWithUTF8String:app_name]];
    [Window setMinSize:NSSizeFromCGSize(CGSizeMake(128, 128))];
    
    [Window setOpaque:YES];
    [Window setRestorable:NO];
    [Window invalidateRestorableState];
    [Window makeKeyAndOrderFront: nil];
    [Window makeMainWindow];
    
    // Adjust window size to match retina scaling.
    CGFloat scale = [Window backingScaleFactor];
    gRetinaScale = { (float)scale, (float)scale };
   
    ForgeMTLView *View = [[ForgeMTLView alloc] initWithFrame:ViewRect device: device display:0 hdr:NO vsync:NO];
    [Window setContentView: View];
    [Window setDelegate: View];
    View.delegate = delegateRenderProvider;
    winDesc->handle.window = (void*)CFBridgingRetain(View);

    NSSize windowSize = CGSizeMake(ViewRect.size.width / gRetinaScale.x, ViewRect.size.height / gRetinaScale.y);
    [Window setContentSize:windowSize];
    [Window setCollectionBehavior:NSWindowCollectionBehaviorFullScreenPrimary];
    if (!winDesc->fullScreen)
        [Window center];
}

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

	NSWindow* window = (__bridge NSWindow*)(winDesc->handle.window);
	[window setFrame:winRect display:true];
}

void setWindowSize(WindowsDesc* winDesc, unsigned width, unsigned height) { setWindowRect(winDesc, { 0, 0, (int)width, (int)height }); }

void toggleBorderless(WindowsDesc* winDesc, unsigned width, unsigned height)
{
	if (!winDesc->fullScreen)
	{
		winDesc->borderlessWindow = !winDesc->borderlessWindow;
		setWindowSize(winDesc, width, height);
	}
}

void toggleFullscreen(WindowsDesc* winDesc)
{
    NSWindow* window = (__bridge NSWindow*)(winDesc->handle.window);
	if (!window)
		return;

	bool isFullscreen = ((window.styleMask & NSWindowStyleMaskFullScreen) == NSWindowStyleMaskFullScreen);
	winDesc->fullScreen = !isFullscreen;

	[window toggleFullScreen:window];
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
	winDesc->hide = false;
	NSWindow* window = (__bridge NSWindow*)(winDesc->handle.window);
	[window deminiaturize:nil];
}

void minimizeWindow(WindowsDesc* winDesc)
{
	winDesc->hide = true;
	NSWindow* window = (__bridge NSWindow*)(winDesc->handle.window);
	[window miniaturize:nil];
}

void setMousePositionRelative(const WindowsDesc* winDesc, int32_t x, int32_t y)
{
	CGPoint location;
	location.x = winDesc->windowedRect.left + x;
	location.y = winDesc->windowedRect.bottom - y;
	CGWarpMouseCursorPosition(location);
}

/************************************************************************/
// Time Related Functions
/************************************************************************/
#ifdef MAC_OS_X_VERSION_10_12
#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_12
#define ENABLE_CLOCK_GETTIME
#endif
#endif

unsigned getSystemTime()
{
	long            ms;    // Milliseconds
	time_t          s;     // Seconds
	struct timespec spec;
	
#if defined(ENABLE_CLOCK_GETTIME)
	if (@available(macOS 10.12, *))
	{
		clock_gettime(_CLOCK_MONOTONIC, &spec);
	}
	else
#endif
	{
		// https://stackoverflow.com/questions/5167269/clock-gettime-alternative-in-mac-os-x
		clock_serv_t cclock;
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

int64_t getUSec()
{
	timespec ts;
	
#if defined(ENABLE_CLOCK_GETTIME)
	if (@available(macOS 10.12, *))
	{
		clock_gettime(_CLOCK_MONOTONIC, &ts);
	}
	else
#endif
	{
		// https://stackoverflow.com/questions/5167269/clock-gettime-alternative-in-mac-os-x
		clock_serv_t cclock;
		mach_timespec_t mts;
		host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &cclock);
		clock_get_time(cclock, &mts);
		mach_port_deallocate(mach_task_self(), cclock);
		ts.tv_sec = mts.tv_sec;
		ts.tv_nsec = mts.tv_nsec;
	}
	
	long us = (ts.tv_nsec / 1000);
	us += ts.tv_sec * CLOCKS_PER_SEC;
	return us;
}

int64_t getTimerFrequency()
{
    return CLOCKS_PER_SEC;
}

unsigned getTimeSinceStart() { return (unsigned)time(NULL); }

MonitorDesc* getMonitor(uint32_t index)
{
	ASSERT(gMonitorCount > index);
	return &gMonitors[index];
}

float2 getDpiScale() { return gRetinaScale; }
/************************************************************************/
// App Entrypoint
/************************************************************************/

static IApp* pApp = NULL;

int macOSMain(int argc, const char** argv, IApp* app)
{
	pApp = app;
	
	NSDictionary* info = [[NSBundle mainBundle] infoDictionary];
	NSString* minVersion = info[@"LSMinimumSystemVersion"];
	NSArray* versionStr = [minVersion componentsSeparatedByString:@"."];
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
- (void)update;
- (void)shutdown;

@end

// Our view controller.  Implements the MTKViewDelegate protocol, which allows it to accept
// per-frame update and drawable resize callbacks.  Also implements the RenderDestinationProvider
// protocol, which allows our renderer object to get and set drawable properties such as pixel
// format and sample count
@interface GameController: NSObject<RenderDestinationProvider>

@end

/************************************************************************/
// GameController implementation
/************************************************************************/

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
    
    //register terminate callback
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

// Called whenever view changes orientation or layout is changed
- (void)didResize:(CGSize)size
{
    // On initial resize we might not be having the application which, thus we schedule a resize later on
    if (!_application)
    {
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.5f * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
            [self didResize:size];
        });
    }
    else
        [_application drawRectResized:size];
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
{
	self = [super init];
	if (self)
	{
		extern bool MemAllocInit();
		MemAllocInit();
		
		#if TF_USE_MTUNER
			rmemInit(0);
		#endif
		
		fsInitAPI();
		Log::Init();
		
		collectMonitorInfo();
		
		pSettings = &pApp->mSettings;

		if (pSettings->mWidth == -1 || pSettings->mHeight == -1)
		{
			RectDesc rect = {};
			getRecommendedResolution(&rect);
			pSettings->mWidth = getRectWidth(rect);
			pSettings->mHeight = getRectHeight(rect);
		}

		gCurrentWindow = {};
		gCurrentWindow.windowedRect = { 0, 0, (int)pSettings->mWidth, (int)pSettings->mHeight };
		gCurrentWindow.fullScreen = pSettings->mFullScreen;
		gCurrentWindow.maximized = false;
		openWindow(pApp->GetName(), &gCurrentWindow, device, renderDestinationProvider);

		pSettings->mWidth =
			gCurrentWindow.fullScreen ? getRectWidth(gCurrentWindow.fullscreenRect) : getRectWidth(gCurrentWindow.windowedRect);
		pSettings->mHeight =
			gCurrentWindow.fullScreen ? getRectHeight(gCurrentWindow.fullscreenRect) : getRectHeight(gCurrentWindow.windowedRect);
		pApp->pWindow = &gCurrentWindow;

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
	for(int i = 0; i < gMonitorCount; ++i)
	{
		MonitorDesc& monitor = gMonitors[i];
		conf_free(monitor.resolutions);
	}
	
	if(gMonitorCount > 0)
	{
		conf_free(gMonitors);
	}
	
	pApp->Unload();
	pApp->Exit();
	Log::Exit();

	fsExitAPI();
	
	#if TF_USE_MTUNER
		rmemUnload();
		rmemShutDown();
	#endif
	
	extern void MemAllocExit();
	MemAllocExit();
}
@end
/************************************************************************/
/************************************************************************/
#endif
