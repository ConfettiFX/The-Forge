/*
 * Copyright (c) 2018-2021 The Forge Interactive Inc.
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

#include "../Math/MathTypes.h"

#include "../Interfaces/IOperatingSystem.h"
#include "../Interfaces/ILog.h"
#include "../Interfaces/ITime.h"
#include "../Interfaces/IThread.h"
#include "../Interfaces/IFileSystem.h"
#include "../Interfaces/IProfiler.h"
#include "../Interfaces/IApp.h"

#include "../Interfaces/IScripting.h"
#include "../Interfaces/IFont.h"
#include "../Interfaces/IUI.h"

#include "../../Renderer/IRenderer.h"

#include "../Interfaces/IMemory.h"

#define FORGE_WINDOW_CLASS L"The Forge"
#define MAX_KEYS 256
#define MAX_CURSOR_DELTA 200

#define elementsOf(a) (sizeof(a) / sizeof((a)[0]))

float2*            gDPIScales = nullptr;
MonitorDesc*       gMonitors = nullptr;
uint32_t           gMonitorCount = 0;
bool               gCursorVisible = true;
bool               gCursorInsideTrackingArea = true;
static WindowsDesc gCurrentWindow;

static uint8_t gResetScenario = RESET_SCENARIO_NONE;

@interface ForgeApplication: NSApplication
@end

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

void HideMenuBar()
{
	NSApplication* app = [NSApplication sharedApplication];
	app.presentationOptions = NSApplicationPresentationDefault | NSApplicationPresentationHideMenuBar | NSApplicationPresentationHideDock;
}

void AutoHideMenuBar()
{
	NSApplication* app = [NSApplication sharedApplication];
	app.presentationOptions = NSApplicationPresentationDefault | NSApplicationPresentationAutoHideMenuBar;
}

@interface ForgeNSWindow: NSWindow
{
}

- (CGFloat)titleBarHeight;

@end

@implementation ForgeNSWindow
{
}

- (BOOL)canBecomeKeyWindow;
{
	return YES;
}

- (BOOL)canBecomeMainWindow;
{
	return YES;
}

- (BOOL)acceptsFirstResponder
{
	return YES;
}

- (BOOL)acceptsFirstMouse:(NSEvent*)event
{
	return YES;
}

- (CGFloat)titleBarHeight
{
	CGFloat contentHeight = [self contentRectForFrameRect:self.frame].size.height;
	return self.frame.size.height - contentHeight;
}

- (CGPoint)convetToBottomLeft:(CGPoint)coord
{
	NSScreen* screen = [self screen];
	if (screen == nil)
	{
		return CGPointZero;
	}

	NSView* view = self.contentView;
	BOOL    isFlipped = [view isFlipped];
	if (isFlipped)
	{
		return coord;
	}

	NSRect screenFrame = [screen visibleFrame];
	NSRect windowFrame = [self frame];

	CGPoint flipped;
	flipped.x = coord.x;
	flipped.y = screenFrame.origin.y + screenFrame.size.height - coord.y - windowFrame.size.height;

	return flipped;
}

- (CGPoint)convetToTopLeft:(CGPoint)coord
{
	NSScreen* screen = [self screen];
	if (screen == nil)
	{
		return CGPointZero;
	}

	NSView* view = self.contentView;
	BOOL    isFlipped = [view isFlipped];
	if (isFlipped)
	{
		return coord;
	}

	NSRect screenFrame = [screen frame];
	NSRect windowFrame = [self frame];

	CGPoint flipped;
	flipped.x = coord.x;
	flipped.y = screenFrame.size.height - coord.y - windowFrame.size.height;

	return flipped;
}

- (RectDesc)setRectFromNSRect:(NSRect)nsrect
{
	float dpiScale[2];
	getDpiScale(dpiScale);
	return { (int32_t)(nsrect.origin.x * dpiScale[0]), (int32_t)(nsrect.origin.y * dpiScale[1]),
			 (int32_t)((nsrect.origin.x + nsrect.size.width) * dpiScale[0]),
			 (int32_t)((nsrect.origin.y + nsrect.size.height) * dpiScale[1]) };
}

- (void)updateClientRect:(WindowsDesc*)winDesc
{
	NSRect viewSize = [self.contentView frame];
	winDesc->clientRect = [self setRectFromNSRect:viewSize];
}

- (void)updateWindowRect:(WindowsDesc*)winDesc
{
	NSRect windowSize = [self frame];

	CGPoint origin = [self convetToTopLeft:windowSize.origin];
	windowSize.origin = origin;
	winDesc->windowedRect = [self setRectFromNSRect:windowSize];
}

- (void)hideFullScreenButton
{
	// Removing fullscreen button from styles to avoid a potential state
	// mismatch between the macOS api and the project. Fullscreen mode
	// should be enabled only through the toggleFullScreen
	NSButton* button = [self standardWindowButton:NSWindowZoomButton];
	if (button == nil)
	{
		return;
	}

	[button setHidden:YES];
	[button setEnabled:NO];
}

- (void)setStyleMask:(NSWindowStyleMask)style
{
	NSResponder*      responder = [self firstResponder];
	NSWindowStyleMask previousStyle = [super styleMask];
	[super setStyleMask:style];
	[self makeFirstResponder:responder];

	[self hideFullScreenButton];

	// Borderless flag causes a lot of issues on macOS as far as
	// macos 10.15. Firt, it changes first responder under the hood
	// silently, skipping regular routines (hence setStyleMask override).
	// Second, it puts the responder in a weird state where it loses
	// the following mouse/keyboard message after style change.
	// To correctly and consistently pipe all events, we spin here,
	// waiting for the "up" event to occur if we got here via the
	// input "down" message.
	if (style == NSWindowStyleMaskBorderless || previousStyle == NSWindowStyleMaskBorderless || style & NSWindowStyleMaskFullScreen ||
		previousStyle & NSWindowStyleMaskFullScreen)
	{
		[self waitOnUpSpinning];
	}
}

- (void)waitOnUpSpinning
{
	// See "Cocoa Event Handling Guide" for reference.
	NSView*     firstResponderView = (NSView * __nullable)[self firstResponder];
	NSEvent*    current = [self currentEvent];
	NSEventType currentEventType = [current type];

	if (currentEventType == NSEventTypeLeftMouseDown || currentEventType == NSEventTypeRightMouseDown ||
		currentEventType == NSEventTypeOtherMouseDown || currentEventType == NSEventTypeKeyDown || currentEventType == NSEventTypePressure)
	{
		BOOL waitingForUp = YES;
		while (waitingForUp)
		{
			NSEvent* event =
				[self nextEventMatchingMask:NSEventMaskLeftMouseUp | NSEventMaskRightMouseUp | NSEventMaskOtherMouseUp | NSEventMaskKeyUp];

			switch ([event type])
			{
				case NSEventTypeLeftMouseUp:
				case NSEventTypeRightMouseUp:
				case NSEventTypeOtherMouseUp:
				{
					[firstResponderView mouseUp:event];
					waitingForUp = NO;
					break;
				}

				case NSEventTypeKeyUp:
				{
					[firstResponderView keyUp:event];
					waitingForUp = NO;
					break;
				}

				default: break;
			}
		}
	}
}

@end

// Protocol abstracting the platform specific view in order to keep the Renderer class independent from platform
@protocol RenderDestinationProvider
- (void)draw;
- (void)didResize:(CGSize)size;
- (void)didMiniaturize;
- (void)didDeminiaturize;
@end

@interface ForgeMTLView: NSView<NSWindowDelegate>
{
	@private
	CVDisplayLinkRef displayLink;
	CAMetalLayer*    metalLayer;
}
@property(weak) id<RenderDestinationProvider> delegate;

- (id)initWithFrame:(NSRect)FrameRect device:(id<MTLDevice>)device display:(int)displayID hdr:(bool)hdr vsync:(bool)vsync;
- (CVReturn)getFrameForTime:(const CVTimeStamp*)outputTime;

@end

@implementation ForgeMTLView

- (id)initWithFrame:(NSRect)FrameRect device:(id<MTLDevice>)device display:(int)in_displayID hdr:(bool)hdr vsync:(bool)vsync
{
	self = [super initWithFrame:FrameRect];
	self.wantsLayer = YES;

	metalLayer = [CAMetalLayer layer];
	metalLayer.device = device;
	metalLayer.framebufferOnly = YES;    //todo: optimized way
	metalLayer.pixelFormat = hdr ? MTLPixelFormatRGBA16Float : MTLPixelFormatBGRA8Unorm;
	metalLayer.wantsExtendedDynamicRangeContent = hdr ? true : false;
	metalLayer.drawableSize = CGSizeMake(self.frame.size.width, self.frame.size.height);
#if defined(ENABLE_DISPLAY_SYNC_TOGGLE)
	if (@available(macOS 10.13, *))
	{
		metalLayer.displaySyncEnabled = vsync;
	}
#endif
	self.layer = metalLayer;

	[self setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];

	return self;
}

- (CVReturn)getFrameForTime:(const CVTimeStamp*)outputTime
{
	// Called whenever the view needs to render
	// Need to dispatch to main thread as CVDisplayLink uses it's own thread.
	dispatch_sync(dispatch_get_main_queue(), ^{ [self.delegate draw]; });

	return kCVReturnSuccess;
}

- (void)windowDidMiniaturize:(NSNotification *)notification
{
    [self.delegate didMiniaturize];
}

- (void)windowDidDeminiaturize:(NSNotification *)notification
{
    [self.delegate didDeminiaturize];
}

- (void)windowDidResize:(NSNotification*)notification
{
	ForgeNSWindow* window = [notification object];
	if ([window inLiveResize])
	{
		return;
	}

	NSRect viewSize = [window.contentView frame];

	[self.delegate didResize:viewSize.size];

	float dpiScale[2];
	getDpiScale(dpiScale);
	metalLayer.drawableSize = CGSizeMake(self.frame.size.width * dpiScale[0], self.frame.size.height * dpiScale[1]);

	if (!gCurrentWindow.fullScreen)
	{
		[window updateClientRect:&gCurrentWindow];
		[window updateWindowRect:&gCurrentWindow];
	}
}

- (void)windowDidEndLiveResize:(NSNotification*)notification
{
	if (gCurrentWindow.fullScreen)
		return;

	ForgeNSWindow* window = [notification object];
	NSRect         viewSize = [window.contentView frame];

	[self.delegate didResize:viewSize.size];

	float dpiScale[2];
	getDpiScale(dpiScale);
	metalLayer.drawableSize = CGSizeMake(self.frame.size.width * dpiScale[0], self.frame.size.height * dpiScale[1]);

	[window updateClientRect:&gCurrentWindow];
	[window updateWindowRect:&gCurrentWindow];
}

- (void)windowDidMove:(NSNotification*)notification
{
	if (gCurrentWindow.fullScreen)
	{
		return;
	}

	ForgeNSWindow* window = [notification object];

	[window updateClientRect:&gCurrentWindow];
	[window updateWindowRect:&gCurrentWindow];
}

- (void)windowDidChangeScreen:(NSNotification*)notification
{
	ForgeNSWindow* window = [notification object];
	NSScreen*      screen = [window screen];

	NSRect frame = [screen frame];
	gCurrentWindow.fullscreenRect = [window setRectFromNSRect:frame];
}

- (void)onActivation:(NSNotification*)notification
{
	NSRunningApplication* runningApplication = [notification.userInfo objectForKey:@"NSWorkspaceApplicationKey"];

	if ([[NSRunningApplication currentApplication] isEqual:runningApplication])
	{
		if (gCurrentWindow.fullScreen)
		{
			[self.window setLevel:NSNormalWindowLevel];
			HideMenuBar();
		}
		else
		{
			[self.window setLevel:NSPopUpMenuWindowLevel];
			AutoHideMenuBar();
		}
	}
}

- (void)onDeactivation:(NSNotification*)notification
{
	NSRunningApplication* runningApplication = [notification.userInfo objectForKey:@"NSWorkspaceApplicationKey"];

	if ([[NSRunningApplication currentApplication] isEqual:runningApplication])
	{
		[self.window setLevel:NSNormalWindowLevel];
		AutoHideMenuBar();
	}
}

- (void)windowDidEnterFullScreen:(NSNotification*)notification
{
	HideMenuBar();
}

- (void)windowDidExitFullScreen:(NSNotification*)notification
{
	AutoHideMenuBar();
}

- (CAMetalLayer*)metalLayer
{
	return metalLayer;
}

- (void)updateTrackingAreas
{
	NSArray<NSTrackingArea*>* trackingAreas = [self trackingAreas];
	if ([trackingAreas count] != 0)
	{
		[self removeTrackingArea:trackingAreas[0]];
	}

	NSRect                bounds = [self bounds];
	NSTrackingAreaOptions options =
		(NSTrackingCursorUpdate | NSTrackingInVisibleRect | NSTrackingMouseEnteredAndExited | NSTrackingMouseMoved |
		 NSTrackingActiveInKeyWindow);
	NSTrackingArea* trackingArea = [[NSTrackingArea alloc] initWithRect:bounds options:options owner:self userInfo:nil];

	[self addTrackingArea:trackingArea];
	[super updateTrackingAreas];
}

- (void)mouseEntered:(NSEvent*)nsEvent
{
	gCursorInsideTrackingArea = true;
}

- (void)mouseExited:(NSEvent*)nsEvent
{
	gCursorInsideTrackingArea = false;
}

@end

namespace {
bool isCaptured = false;
}

static NSString* screenNameForDisplay(CGDirectDisplayID displayID, NSScreen* screen)
{
  if (@available(macOS 10.15, *)) {
    NSString* screenName = screen.localizedName;
    return screenName;
  }
  else {
    NSString *screenName = nil;
    NSDictionary *deviceInfo = (__bridge NSDictionary *)IODisplayCreateInfoDictionary(CGDisplayIOServicePort(displayID), kIODisplayOnlyPreferredName);
    NSDictionary *localizedNames = [deviceInfo objectForKey:[NSString stringWithUTF8String:kDisplayProductName]];
    if ([localizedNames count] > 0) {
      screenName = [localizedNames objectForKey:[[localizedNames allKeys] objectAtIndex:0]];
    }
    return screenName;
  }
}

NSWindowStyleMask PrepareStyleMask(WindowsDesc* winDesc)
{
	uint32_t fullScreenHeight = getRectHeight(&winDesc->fullscreenRect);

	NSWindowStyleMask styleMask = NSWindowStyleMaskBorderless;
	if (!winDesc->fullScreen && !winDesc->borderlessWindow && getRectHeight(&winDesc->clientRect) != fullScreenHeight)
	{
		NSWindowStyleMask noResizeWindow = !winDesc->noresizeFrame ? NSWindowStyleMaskResizable : 0;

		styleMask = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable | noResizeWindow;
	}

	return styleMask;
}

void collectMonitorInfo()
{
	if (gMonitorCount != 0)
	{
		return;
	}

	uint32_t displayCount = 0;
	CGError  error = CGGetOnlineDisplayList(0, nil, &displayCount);
	if (error != kCGErrorSuccess)
	{
		ASSERT(0);
	}

	eastl::vector<CGDirectDisplayID> onlineDisplayIDs(displayCount);

	error = CGGetOnlineDisplayList(displayCount, onlineDisplayIDs.data(), &displayCount);
	if (error != kCGErrorSuccess)
	{
		ASSERT(0);
	}

	ASSERT(displayCount != 0);

	gMonitorCount = displayCount;
	gMonitors = (MonitorDesc*)tf_calloc(displayCount, sizeof(MonitorDesc));
	gDPIScales = (float2*)tf_calloc(displayCount, sizeof(float2));
		
	NSArray<NSScreen*>* screens = [NSScreen screens];
	CGDirectDisplayID mainDisplayID = CGMainDisplayID();
    
    for (uint32_t displayIndex = 0; displayIndex < displayCount; ++displayIndex)
    {
        CGDirectDisplayID displayID = onlineDisplayIDs[displayIndex];

		NSScreen* displayScreen = nil;
		for (NSScreen* screen : screens)
		{
			NSDictionary*     screenDictionary = [screen deviceDescription];
			NSNumber*         idAsNumber = [screenDictionary objectForKey:@"NSScreenNumber"];
			CGDirectDisplayID screenID = [idAsNumber unsignedIntValue];

			if(displayID == screenID)
			{
				displayScreen = screen;
				break;
			}
		}

		ASSERT(displayScreen != nil);

		float dpiScale = (float)[displayScreen backingScaleFactor];
        gDPIScales[displayIndex] = { (float)dpiScale, (float)dpiScale };

		MonitorDesc& display = gMonitors[displayIndex];

		display.displayID = displayID;

		NSRect frameRect = [displayScreen frame];
		display.defaultResolution.mWidth = frameRect.size.width;
		display.defaultResolution.mHeight = frameRect.size.height;

        NSString* displayName = screenNameForDisplay(displayID, displayScreen);
		strcpy(display.publicDisplayName, displayName.UTF8String);

		id<MTLDevice> metalDevice = CGDirectDisplayCopyCurrentMetalDevice(displayID);
		strcpy(display.publicAdapterName, metalDevice.name.UTF8String);

		CGRect displayBounds = CGDisplayBounds(displayID);
		display.workRect = { (int32_t)displayBounds.origin.x, (int32_t)displayBounds.origin.y, (int32_t)displayBounds.size.width,
							 (int32_t)displayBounds.size.height };
		display.monitorRect = display.workRect;

		eastl::vector<Resolution> displayResolutions;
		CFArrayRef                displayModes = CGDisplayCopyAllDisplayModes(displayID, nil);
		for (CFIndex i = 0; i < CFArrayGetCount(displayModes); ++i)
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
		display.resolutions = (Resolution*)tf_calloc(display.resolutionCount, sizeof(Resolution));
		memcpy(display.resolutions, displayResolutions.data(), display.resolutionCount * sizeof(Resolution));

		CFRelease(displayModes);

		if(displayID == mainDisplayID && displayIndex != 0)
		{
			std::swap(gMonitors[0], gMonitors[displayIndex]);
		}
	}
}

CGDisplayModeRef FindClosestResolution(CGDirectDisplayID display, const Resolution* pMode)
{
	CFArrayRef displayModes = CGDisplayCopyAllDisplayModes(display, nil);
	ASSERT(displayModes != nil);

	int64_t smallestDiff = std::numeric_limits<int64_t>::max();

	CGDisplayModeRef bestMode = nil;
	for (CFIndex i = 0; i < CFArrayGetCount(displayModes); ++i)
	{
		CGDisplayModeRef mode = (CGDisplayModeRef)CFArrayGetValueAtIndex(displayModes, i);
		int64_t          width = (int64_t)CGDisplayModeGetWidth(mode);
		int64_t          height = (int64_t)CGDisplayModeGetHeight(mode);

		int64_t desiredWidth = pMode->mWidth;
		int64_t desiredHeight = pMode->mHeight;

		int64_t diffw = width - desiredWidth;
		int64_t diffw2 = diffw * diffw;

		int64_t diffh = height - desiredHeight;
		int64_t diffh2 = diffh * diffh;

		int64_t lenSq = diffw2 + diffh2;
		if (smallestDiff > lenSq)
		{
			smallestDiff = lenSq;
			bestMode = mode;
		}
	}

	CFRelease(displayModes);

	return bestMode;
}

void setResolution(const MonitorDesc* pMonitor, const Resolution* pMode)
{
	ASSERT(pMonitor != nil);
	ASSERT(pMode != nil);

	CGDirectDisplayID display = pMonitor->displayID;
	CGDisplayModeRef  bestMode = FindClosestResolution(display, pMode);
	ASSERT(bestMode != nil);

	size_t width = CGDisplayModeGetWidth(bestMode);
	size_t height = CGDisplayModeGetHeight(bestMode);
	if (pMode->mWidth == width && pMode->mHeight == height)
	{
		return;
	}

	CGDisplayConfigRef configRef;
	CGError            error = CGBeginDisplayConfiguration(&configRef);
	if (error != kCGErrorSuccess)
	{
		ASSERT(0);
	}

	error = CGConfigureDisplayWithDisplayMode(configRef, display, bestMode, nil);
	if (error != kCGErrorSuccess)
	{
		ASSERT(0);
	}

	error = CGCompleteDisplayConfiguration(configRef, kCGConfigureForSession);
	if (error != kCGErrorSuccess)
	{
		ASSERT(0);
	}
}

void getRecommendedResolution(RectDesc* rect)
{
	float dpiScale[2];
	getDpiScale(dpiScale);
	NSRect mainScreenRect = [[NSScreen mainScreen] frame];
	*rect = RectDesc{ 0, 0, (int32_t)(mainScreenRect.size.width * dpiScale[0] + 1e-6),
					  (int32_t)(mainScreenRect.size.height * dpiScale[1] + 1e-6) };
}

void setCustomMessageProcessor(CustomMessageProcessor proc) {}

void onRequestReload()
{
	gResetScenario |= RESET_SCENARIO_RELOAD;
}
void onDeviceLost()
{
	// NOT SUPPORTED ON THIS PLATFORM
}

void onAPISwitch()
{
	// NOT SUPPORTED ON THIS PLATFORM
}

void requestShutdown()
{
	ForgeMTLView* view = (__bridge ForgeMTLView*)(gCurrentWindow.handle.window);
	if (view == nil)
	{
		return;
	}

	NSNotificationCenter* notificationCenter = [[NSWorkspace sharedWorkspace] notificationCenter];
	[notificationCenter removeObserver:view name:NSWorkspaceDidActivateApplicationNotification object:nil];

	[notificationCenter removeObserver:view name:NSWorkspaceDidDeactivateApplicationNotification object:nil];

	[[NSApplication sharedApplication] terminate:[NSApplication sharedApplication]];
}

void openWindow(const char* app_name, WindowsDesc* winDesc, id<MTLDevice> device, id<RenderDestinationProvider> delegateRenderProvider)
{
	NSWindowStyleMask styleMask = PrepareStyleMask(winDesc);

	NSRect viewRect{ 0, 0, (float)getRectWidth(&winDesc->clientRect), (float)getRectHeight(&winDesc->clientRect) };

	NSRect styleAdjustedRect = [NSWindow frameRectForContentRect:viewRect styleMask:styleMask];

	ForgeNSWindow* window = [[ForgeNSWindow alloc] initWithContentRect:styleAdjustedRect
															 styleMask:styleMask
															   backing:NSBackingStoreBuffered
																 defer:YES];
	[window hideFullScreenButton];

	ForgeMTLView* view = [[ForgeMTLView alloc] initWithFrame:viewRect device:device display:0 hdr:NO vsync:NO];

	NSNotificationCenter* notificationCenter = [[NSWorkspace sharedWorkspace] notificationCenter];
	[notificationCenter addObserver:view selector:@selector(onActivation:) name:NSWorkspaceDidActivateApplicationNotification object:nil];

	[notificationCenter addObserver:view
						   selector:@selector(onDeactivation:)
							   name:NSWorkspaceDidDeactivateApplicationNotification
							 object:nil];

	[window setContentView:view];
	[window setDelegate:view];
	[window setCollectionBehavior:NSWindowCollectionBehaviorFullScreenPrimary];

	view.delegate = delegateRenderProvider;
	winDesc->handle.window = (void*)CFBridgingRetain(view);

	float dpiScale[2];
	getDpiScale(dpiScale);
	NSSize windowSize = CGSizeMake(viewRect.size.width / dpiScale[0], viewRect.size.height / dpiScale[1]);
	[window setContentSize:windowSize];

	NSRect                bounds = [view bounds];
	NSTrackingAreaOptions options =
		(NSTrackingCursorUpdate | NSTrackingInVisibleRect | NSTrackingMouseEnteredAndExited | NSTrackingMouseMoved |
		 NSTrackingActiveInKeyWindow);
	NSTrackingArea* trackingArea = [[NSTrackingArea alloc] initWithRect:bounds options:options owner:view userInfo:nil];

	[view addTrackingArea:trackingArea];
	[window makeFirstResponder:view];

	[window setAcceptsMouseMovedEvents:YES];
	[window setTitle:[NSString stringWithUTF8String:app_name]];
	[window setMinSize:NSSizeFromCGSize(CGSizeMake(128, 128))];

	[window setOpaque:YES];
	[window setRestorable:NO];
	[window invalidateRestorableState];
	[window makeMainWindow];
	[window setReleasedWhenClosed:NO];
	[window makeKeyAndOrderFront:nil];

	[NSApp activateIgnoringOtherApps:YES];
	[view.window setLevel:NSPopUpMenuWindowLevel];

	styleAdjustedRect.origin = [window convetToTopLeft:styleAdjustedRect.origin];
	if (winDesc->centered)
	{
		centerWindow(winDesc);
	}
	else
	{
		[window setFrameOrigin:styleAdjustedRect.origin];
	}

	[window updateWindowRect:winDesc];

	if (winDesc->fullScreen)
	{
		winDesc->fullScreen = false;
		toggleFullscreen(winDesc);
	}

	setMousePositionRelative(winDesc, styleAdjustedRect.size.width / 2.0 / dpiScale[0], styleAdjustedRect.size.height / 2.0 / dpiScale[1]);
}

void closeWindow(const WindowsDesc* winDesc) {}

void setWindowRect(WindowsDesc* winDesc, const RectDesc* pRect)
{
	ForgeMTLView* view = (__bridge ForgeMTLView*)(winDesc->handle.window);
	if (view == nil)
	{
		return;
	}

	int clientWidthStart = (getRectWidth(&winDesc->windowedRect) - getRectWidth(&winDesc->clientRect)) >> 1;
	int clientHeightStart = getRectHeight(&winDesc->windowedRect) - getRectHeight(&winDesc->clientRect) - clientWidthStart;

	winDesc->clientRect = *pRect;

	if (winDesc->centered)
	{
		centerWindow(winDesc);
	}
	else
	{
		NSRect contentRect;
		float  dpiScale[2];
		getDpiScale(dpiScale);
		contentRect.origin.x = pRect->left / dpiScale[0];
		contentRect.origin.y = (getRectHeight(&gCurrentWindow.fullscreenRect) - pRect->bottom - clientHeightStart) / dpiScale[1];
		contentRect.size.width = (float)getRectWidth(pRect) / dpiScale[0];
		contentRect.size.height = (float)getRectHeight(pRect) / dpiScale[1];

		ForgeNSWindow* window = (ForgeNSWindow*)view.window;
		window.styleMask = PrepareStyleMask(winDesc);

		NSRect styleAdjustedRect = [NSWindow frameRectForContentRect:contentRect styleMask:window.styleMask];

		[window setFrame:styleAdjustedRect display:true];
	}
}

void setWindowSize(WindowsDesc* winDesc, unsigned width, unsigned height)
{
	RectDesc desc{ 0, 0, (int)width, (int)height };
	setWindowRect(winDesc, &desc);
}

void toggleBorderless(WindowsDesc* winDesc, unsigned width, unsigned height)
{
	ForgeMTLView* view = (__bridge ForgeMTLView*)(winDesc->handle.window);
	if (view == nil)
	{
		return;
	}

	ForgeNSWindow* window = (ForgeNSWindow*)view.window;
	winDesc->borderlessWindow = !winDesc->borderlessWindow;
	if (winDesc->fullScreen)
	{
		return;
	}

	window.styleMask = PrepareStyleMask(winDesc);
	NSRect contentBounds = view.bounds;
	float  dpiScale[2];
	getDpiScale(dpiScale);
	uint32_t currentWidth = (uint32_t)(contentBounds.size.width * dpiScale[0] + 1e-6);
	uint32_t currentHeight = (uint32_t)(contentBounds.size.height * dpiScale[1] + 1e-6);
	if (currentWidth != width || currentHeight != height)
	{
		setWindowSize(winDesc, width, height);
	}
}

void toggleFullscreen(WindowsDesc* winDesc)
{
	ForgeMTLView* view = (__bridge ForgeMTLView*)(winDesc->handle.window);
	if (view == nil)
	{
		return;
	}

	ForgeNSWindow* window = (ForgeNSWindow*)view.window;

	bool isFullscreen = !(window.styleMask & NSWindowStyleMaskFullScreen);

	float dpiScale[2];
	getDpiScale(dpiScale);
	if (isFullscreen)
	{
		winDesc->fullScreen = isFullscreen;
		window.styleMask = PrepareStyleMask(winDesc);

		NSSize size = { (CGFloat)getRectWidth(&gCurrentWindow.fullscreenRect) / dpiScale[0],
						(CGFloat)getRectHeight(&gCurrentWindow.fullscreenRect) / dpiScale[1] };
		[window setContentSize:size];

		[view.window setLevel:NSNormalWindowLevel];
		HideMenuBar();
	}

	[window toggleFullScreen:window];

	if (!isFullscreen)
	{
		NSSize size = { (CGFloat)getRectWidth(&gCurrentWindow.clientRect) / dpiScale[0],
						(CGFloat)getRectHeight(&gCurrentWindow.clientRect) / dpiScale[1] };
		[window setContentSize:size];

		winDesc->fullScreen = isFullscreen;
		window.styleMask = PrepareStyleMask(winDesc);

		[view.window setLevel:NSPopUpMenuWindowLevel];
		AutoHideMenuBar();
	}
}

void showWindow(WindowsDesc* winDesc)
{
	winDesc->hide = false;
	ForgeMTLView* view = (__bridge ForgeMTLView*)(winDesc->handle.window);
	if (view == nil)
	{
		return;
	}

	[[NSApplication sharedApplication] unhide:nil];
}

void hideWindow(WindowsDesc* winDesc)
{
	winDesc->hide = true;
	ForgeMTLView* view = (__bridge ForgeMTLView*)(winDesc->handle.window);
	if (view == nil)
	{
		return;
	}

	[[NSApplication sharedApplication] hide:nil];
}

void maximizeWindow(WindowsDesc* winDesc)
{
	winDesc->hide = false;
	ForgeMTLView* view = (__bridge ForgeMTLView*)(winDesc->handle.window);
	if (view == nil)
	{
		return;
	}

	ForgeNSWindow* window = (ForgeNSWindow*)view.window;
	[window deminiaturize:nil];
}

void minimizeWindow(WindowsDesc* winDesc)
{
	winDesc->hide = true;
	ForgeMTLView* view = (__bridge ForgeMTLView*)(winDesc->handle.window);
	if (view == nil)
	{
		return;
	}

	ForgeNSWindow* window = (ForgeNSWindow*)view.window;
	[window miniaturize:nil];
}

void centerWindow(WindowsDesc* winDesc)
{
	ForgeMTLView* view = (__bridge ForgeMTLView*)(winDesc->handle.window);
	if (view == nil)
	{
		return;
	}

	winDesc->centered = true;

	uint32_t fsHalfWidth = getRectWidth(&winDesc->fullscreenRect) >> 1;
	uint32_t fsHalfHeight = getRectHeight(&winDesc->fullscreenRect) >> 1;
	uint32_t windowWidth = getRectWidth(&winDesc->clientRect);
	uint32_t windowHeight = getRectHeight(&winDesc->clientRect);
	uint32_t windowHalfWidth = windowWidth >> 1;
	uint32_t windowHalfHeight = windowHeight >> 1;

	uint32_t X = fsHalfWidth - windowHalfWidth;
	uint32_t Y = fsHalfHeight - windowHalfHeight;

	NSRect contentRect;
	float  dpiScale[2];
	getDpiScale(dpiScale);
	contentRect.origin.x = X / dpiScale[0];
	contentRect.origin.y = (getRectHeight(&gCurrentWindow.fullscreenRect) - (Y + windowHeight)) / dpiScale[1];
	contentRect.size.width = (float)(windowWidth) / dpiScale[0];
	contentRect.size.height = (float)(windowHeight) / dpiScale[1];

	ForgeNSWindow* window = (ForgeNSWindow*)view.window;
	window.styleMask = PrepareStyleMask(winDesc);

	NSRect styleAdjustedRect = [NSWindow frameRectForContentRect:contentRect styleMask:window.styleMask];

	[window setFrame:styleAdjustedRect display:true];
}

void* createCursor(const char* path)
{
	BOOL exists = [[NSFileManager defaultManager] fileExistsAtPath:[NSString stringWithUTF8String:path]];
	if (!exists)
	{
		return nil;
	}

	NSImage* cursorImage = [[NSImage alloc] initByReferencingFile:[NSString stringWithUTF8String:path]];
	if (cursorImage == nil)
	{
		return nil;
	}

	NSCursor* cursor = [[NSCursor alloc] initWithImage:cursorImage hotSpot:NSZeroPoint];
	return (void*)CFBridgingRetain(cursor);
}

void setCursor(void* cursor)
{
	NSCursor* currentCursor = (__bridge NSCursor*)(cursor);
	[currentCursor set];
}

void showCursor()
{
	if (!gCursorVisible)
	{
		gCursorVisible = true;
		[NSCursor unhide];
	}
}

void hideCursor()
{
	if (gCursorVisible)
	{
		gCursorVisible = false;
		[NSCursor hide];
	}
}

bool isCursorInsideTrackingArea() { return gCursorInsideTrackingArea; }

void setMousePositionRelative(const WindowsDesc* winDesc, int32_t x, int32_t y)
{
	ForgeMTLView* view = (__bridge ForgeMTLView*)(winDesc->handle.window);
	if (view == nil)
	{
		CGWarpMouseCursorPosition(CGPointZero);
		return;
	}

	ForgeNSWindow* forgeWindow = (ForgeNSWindow * __nullable) view.window;
	NSScreen*      screen = [forgeWindow screen];
	if (screen == nil)
	{
		CGWarpMouseCursorPosition(CGPointZero);
		return;
	}

	CGFloat         titleBarHeight = 0.0;
	const RectDesc* windowRect = nil;
	if (winDesc->fullScreen)
	{
		windowRect = &winDesc->fullscreenRect;
	}
	else
	{
		titleBarHeight = [forgeWindow titleBarHeight];
		windowRect = &winDesc->windowedRect;
	}

	CGPoint location;
	float   dpiScale[2];
	getDpiScale(dpiScale);
	location.x = windowRect->left / dpiScale[0] + x;
	location.y = windowRect->top / dpiScale[1] + titleBarHeight + y;
	CGWarpMouseCursorPosition(location);
}

void setMousePositionAbsolute(int32_t x, int32_t y)
{
	CGPoint location;
	location.x = x;
	location.y = y;
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
		clock_serv_t    cclock;
		mach_timespec_t mts;
		host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &cclock);
		clock_get_time(cclock, &mts);
		mach_port_deallocate(mach_task_self(), cclock);
		spec.tv_sec = mts.tv_sec;
		spec.tv_nsec = mts.tv_nsec;
	}

	s = spec.tv_sec;
	ms = round(spec.tv_nsec / CLOCKS_PER_SEC);    // Convert nanoseconds to milliseconds
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
		clock_serv_t    cclock;
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

int64_t getTimerFrequency() { return CLOCKS_PER_SEC; }

unsigned getTimeSinceStart() { return (unsigned)time(NULL); }

MonitorDesc* getMonitor(uint32_t index)
{
	ASSERT(gMonitorCount > index);
	return &gMonitors[index];
}

uint32_t getMonitorCount() { return gMonitorCount; }

void getDpiScale(float array[2])
{
	if (gCurrentWindow.forceLowDPI)
	{
		array[0] = 1.f;
		array[1] = 1.f;
		return;
	}
	array[0] = gDPIScales[0].x;
	array[1] = gDPIScales[0].y;
}
/************************************************************************/
// App Entrypoint
/************************************************************************/

static IApp* pApp = NULL;

bool initBaseSubsystems()
{
	// Not exposed in the interface files / app layer
	extern bool platformInitFontSystem();
	extern bool platformInitUserInterface();
	extern void platformInitLuaScriptingSystem();

#ifdef USE_FORGE_FONTS
	if (!platformInitFontSystem())
		return false;
#endif

#ifdef USE_FORGE_UI
	if (!platformInitUserInterface())
		return false;
#endif

#ifdef USE_FORGE_SCRIPTING
	platformInitLuaScriptingSystem();
#endif

	return true;
}

void updateBaseSubsystems(float deltaTime)
{
	// Not exposed in the interface files / app layer
	extern void platformUpdateLuaScriptingSystem();
	extern void platformUpdateUserInterface(float deltaTime);

#ifdef USE_FORGE_SCRIPTING
	platformUpdateLuaScriptingSystem();
#endif

#ifdef USE_FORGE_UI
	platformUpdateUserInterface(deltaTime);
#endif
}

void exitBaseSubsystems()
{
	// Not exposed in the interface files / app layer
	extern void platformExitFontSystem();
	extern void platformExitUserInterface();
	extern void platformExitLuaScriptingSystem();

#ifdef USE_FORGE_UI
	platformExitUserInterface();
#endif

#ifdef USE_FORGE_FONTS
	platformExitFontSystem();
#endif

#ifdef USE_FORGE_SCRIPTING
	platformExitLuaScriptingSystem();
#endif
}

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
	// On initial resize we might not be having the application which, thus we schedule a resize later on
	if (!_application)
	{
		dispatch_after(
			dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.5f * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{ [self didResize:size]; });
	}
	else
		[_application drawRectResized:size];
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

    // Call update once after minimizing so that IApp can react.
    if (!gCurrentWindow.minimized || focusChanged)
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
uint32_t frameCounter;
uint32_t targetFrameCount = 240;
char benchmarkOutput[1024] = { "\0" };
#endif

// Metal application implementation.
@implementation MetalKitApplication
{
}
- (nonnull instancetype)initWithMetalDevice:(nonnull id<MTLDevice>)device
				  renderDestinationProvider:(nonnull id<RenderDestinationProvider>)renderDestinationProvider
{
    initTimer(&deltaTimer);
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
		initLog(pApp->GetName(), eALL);

		collectMonitorInfo();

		pSettings = &pApp->mSettings;
		if (pSettings->mMonitorIndex < 0)
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

		if (!gCurrentWindow.fullScreen)
		{
			AutoHideMenuBar();
		}

		RectDesc fullScreenRect = {};
		getRecommendedResolution(&fullScreenRect);
		if (pSettings->mWidth <= 0 || pSettings->mHeight <= 0)
		{
			MonitorDesc* windowMonitor = getMonitor(pSettings->mMonitorIndex);
			uint32_t     oneBeforeLast = windowMonitor->resolutionCount - 2;
			if (oneBeforeLast == ~0)
			{
				oneBeforeLast = 0;
			}

			pSettings->mWidth = windowMonitor->resolutions[oneBeforeLast].mWidth;
			pSettings->mHeight = windowMonitor->resolutions[oneBeforeLast].mHeight;
		}

		gCurrentWindow.clientRect = { 0, 0, (int)pSettings->mWidth, (int)pSettings->mHeight };
		gCurrentWindow.fullscreenRect = fullScreenRect;

		openWindow(pApp->GetName(), &gCurrentWindow, device, renderDestinationProvider);

		pSettings->mFocused = true;

		pSettings->mWidth =
			gCurrentWindow.fullScreen ? getRectWidth(&gCurrentWindow.fullscreenRect) : getRectWidth(&gCurrentWindow.clientRect);
		pSettings->mHeight =
			gCurrentWindow.fullScreen ? getRectHeight(&gCurrentWindow.fullscreenRect) : getRectHeight(&gCurrentWindow.clientRect);
		pApp->pWindow = &gCurrentWindow;

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
			
			//if base subsystem fails then exit the app
			if (!initBaseSubsystems())
			{
				for (ForgeNSWindow* window in [NSApplication sharedApplication].windows)
				{
					[window close];
				}

				exit(1);
			}

			//if app init fails then exit the app
			if (!pApp->Init())
			{
				for (ForgeNSWindow* window in [NSApplication sharedApplication].windows)
				{
					[window close];
				}

				exit(1);
			}

			pApp->mSettings.mInitialized = true;

			//if load fails then exit the app
			if (!pApp->Load())
			{
				for (ForgeNSWindow* window in [NSApplication sharedApplication].windows)
				{
					[window close];
				}

				exit(1);
			}
		}

#ifdef AUTOMATED_TESTING
	if (pSettings->mBenchmarking) setAggregateFrames(targetFrameCount / 2);
#endif
		
		
	}

	return self;
}

- (void)drawRectResized:(CGSize)size
{
	float dpiScale[2];
	getDpiScale(dpiScale);
	int32_t newWidth = (int32_t)(size.width * dpiScale[0] + 1e-6);
	int32_t newHeight = (int32_t)(size.height * dpiScale[1] + 1e-6);

	if (newWidth != pApp->mSettings.mWidth || newHeight != pApp->mSettings.mHeight)
	{
		pApp->mSettings.mWidth = newWidth;
		pApp->mSettings.mHeight = newHeight;

		onRequestReload();
	}
}

void errorMessagePopup(const char* title, const char* msg, void* windowHandle)
{
#ifndef AUTOMATED_TESTING
	NSAlert* alert = [[NSAlert alloc] init];

	[alert setMessageText:[NSString stringWithCString:title encoding:[NSString defaultCStringEncoding]]];
	[alert setInformativeText:[NSString stringWithCString:msg encoding:[NSString defaultCStringEncoding]]];
	[alert setAlertStyle:NSAlertStyleCritical];
	[alert addButtonWithTitle:@"OK"];
	[alert beginSheetModalForWindow:[NSApplication sharedApplication].windows[0] completionHandler:nil];
#endif
}

- (void)update
{
	if (gResetScenario & RESET_SCENARIO_RELOAD)
	{
		pApp->Unload();
		pApp->Load();

		gResetScenario &= ~RESET_SCENARIO_RELOAD;
		return;
	}

	float deltaTime = getTimerMSec(&deltaTimer, true) / 1000.0f;
	// if framerate appears to drop below about 6, assume we're at a breakpoint and simulate 20fps.
	if (deltaTime > 0.15f)
		deltaTime = 0.05f;
	
	// UPDATE BASE INTERFACES
	updateBaseSubsystems(deltaTime);

	// UPDATE APP
	pApp->Update(deltaTime);
	pApp->Draw();

#ifdef AUTOMATED_TESTING
	frameCounter++;
	if (frameCounter >= targetFrameCount)
	{
		for (ForgeNSWindow* window in [NSApplication sharedApplication].windows)
		{
			[window close];
		}

		[NSApp terminate:nil];
	}
#endif
}

- (void)shutdown
{
	for (int i = 0; i < gMonitorCount; ++i)
	{
		MonitorDesc& monitor = gMonitors[i];
		tf_free(monitor.resolutions);
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
	
	pApp->Unload();
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
/************************************************************************/
/************************************************************************/
#endif
