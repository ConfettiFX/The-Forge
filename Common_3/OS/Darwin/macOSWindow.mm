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

#ifdef __APPLE__

#include "../../Application/Config.h"

#import <Cocoa/Cocoa.h>
#include <Foundation/Foundation.h> // Needed to check for debugger
#import <IOKit/graphics/IOGraphicsLib.h>
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#include <sys/sysctl.h> // Needed for debugger check

#include "../../Utilities/ThirdParty/OpenSource/Nothings/stb_ds.h"

#include "../../Application/Interfaces/IApp.h"
#include "../../Utilities/Interfaces/ILog.h"
#include "../Interfaces/IOperatingSystem.h"

#include "../../Graphics/GraphicsConfig.h"
#include "../../Utilities/Math/MathTypes.h"

bool gCursorInsideTrackingArea = true;

MonitorDesc*  gMonitors = nullptr;
uint32_t      gMonitorCount = 0;
float2*       gDPIScales = nullptr;
NSWindowLevel gDefaultWindowLevel = NSPopUpMenuWindowLevel;

extern WindowDesc             gCurrentWindow;
extern CustomMessageProcessor sCustomProc;

//------------------------------------------------------------------------
// STATIC STRUCTS
//------------------------------------------------------------------------

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
    // take into screen origin to get correct center coords
    // otherwise we get center of resolution and end up off
    flipped.y = screenFrame.size.height + screenFrame.origin.y - coord.y - windowFrame.size.height;

    return flipped;
}

- (RectDesc)setRectFromNSRect:(NSRect)nsrect
{
    float          dpiScale[2];
    const uint32_t monitorIdx = getActiveMonitorIdx();
    getMonitorDpiScale(monitorIdx, dpiScale);
    return { (int32_t)(nsrect.origin.x * dpiScale[0]), (int32_t)(nsrect.origin.y * dpiScale[1]),
             (int32_t)((nsrect.origin.x + nsrect.size.width) * dpiScale[0]),
             (int32_t)((nsrect.origin.y + nsrect.size.height) * dpiScale[1]) };
}

- (void)updateClientRect:(WindowDesc*)winDesc
{
    NSRect viewSize = [self.contentView frame];
    winDesc->clientRect = [self setRectFromNSRect:viewSize];
}

NSRect getCenteredWindowRect(WindowDesc* winDesc);

- (void)updateWindowRect:(WindowDesc*)winDesc
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
    NSResponder* responder = [self firstResponder];
    [super setStyleMask:style];

    [self hideFullScreenButton];
    [self makeFirstResponder:responder];
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

            default:
                break;
            }
        }
    }
}

@end

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

@implementation ForgeMTLView

- (BOOL)opaque
{
    return true;
}

- (id)initWithFrame:(NSRect)FrameRect device:(id<MTLDevice>)device display:(int)in_displayID hdr:(bool)hdr vsync:(bool)vsync
{
    self = [super initWithFrame:FrameRect];
    self.wantsLayer = YES;

    metalLayer = [CAMetalLayer layer];
    metalLayer.device = device;
    metalLayer.framebufferOnly = YES;
    metalLayer.opaque = YES;
    metalLayer.pixelFormat = hdr ? MTLPixelFormatRGBA16Float : MTLPixelFormatBGRA8Unorm;
    metalLayer.wantsExtendedDynamicRangeContent = hdr ? true : false;
    metalLayer.drawableSize = CGSizeMake(self.frame.size.width, self.frame.size.height);
    metalLayer.contentsScale = [[NSScreen mainScreen] backingScaleFactor];
#if defined(ENABLE_DISPLAY_SYNC_TOGGLE)
    metalLayer.displaySyncEnabled = vsync;
#endif
    self.layer = metalLayer;

    [self setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];

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

- (void)windowDidBecomeMain:(NSNotification*)notification
{
    [self.delegate didFocusChange:true];
    if (notification && sCustomProc)
    {
        sCustomProc(&gCurrentWindow, (__bridge void*)notification);
    }
}

- (void)windowDidResignMain:(NSNotification*)notification
{
    [self.delegate didFocusChange:false];
    if (notification && sCustomProc)
    {
        sCustomProc(&gCurrentWindow, (__bridge void*)notification);
    }
}

- (void)windowDidMiniaturize:(NSNotification*)notification
{
    [self.delegate didMiniaturize];
}

- (void)windowDidDeminiaturize:(NSNotification*)notification
{
    [self.delegate didDeminiaturize];
}

void windowMovedOrResized(RectDesc* centeredRect)
{
    if (!gCurrentWindow.fullScreen)
    {
        const int windowDecorationMaxSize = 60;
        gCurrentWindow.centered = gCurrentWindow.windowedRect.left == centeredRect->left &&
                                  gCurrentWindow.windowedRect.right == centeredRect->right &&
                                  abs(gCurrentWindow.windowedRect.top - centeredRect->top) < windowDecorationMaxSize &&
                                  abs(gCurrentWindow.windowedRect.bottom - centeredRect->bottom) < windowDecorationMaxSize;
    }

    gCurrentWindow.mWndX = gCurrentWindow.windowedRect.left;
    gCurrentWindow.mWndY = gCurrentWindow.windowedRect.top;
    gCurrentWindow.mWndW = getRectWidth(&gCurrentWindow.windowedRect);
    gCurrentWindow.mWndH = getRectHeight(&gCurrentWindow.windowedRect);
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

    float          dpiScale[2];
    const uint32_t monitorIdx = getActiveMonitorIdx();
    getMonitorDpiScale(monitorIdx, dpiScale);
    metalLayer.drawableSize = CGSizeMake(self.frame.size.width * dpiScale[0], self.frame.size.height * dpiScale[1]);

    if (!gCurrentWindow.fullScreen)
    {
        [window updateClientRect:&gCurrentWindow];
        [window updateWindowRect:&gCurrentWindow];

        NSRect   centeredWindowRectNS = getCenteredWindowRect(&gCurrentWindow);
        RectDesc centeredWindowRect = [window setRectFromNSRect:centeredWindowRectNS];
        windowMovedOrResized(&centeredWindowRect);
    }

    gCurrentWindow.maximized = [window isZoomed];
    gCurrentWindow.minimized = false;
}

- (void)windowDidEndLiveResize:(NSNotification*)notification
{
    if (gCurrentWindow.fullScreen)
        return;

    ForgeNSWindow* window = [notification object];
    NSRect         viewSize = [window.contentView frame];

    [self.delegate didResize:viewSize.size];

    float          dpiScale[2];
    const uint32_t monitorIdx = getActiveMonitorIdx();
    getMonitorDpiScale(monitorIdx, dpiScale);
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

    NSRect   centeredWindowRectNS = getCenteredWindowRect(&gCurrentWindow);
    RectDesc centeredWindowRect = [window setRectFromNSRect:centeredWindowRectNS];
    windowMovedOrResized(&centeredWindowRect);
}

- (void)windowDidChangeScreen:(NSNotification*)notification
{
    ForgeNSWindow* window = [notification object];
    NSScreen*      screen = [window screen];

    NSRect frame = [screen frame];
    gCurrentWindow.fullscreenRect = [window setRectFromNSRect:frame];
}

- (NSApplicationPresentationOptions)window:(NSWindow*)window
      willUseFullScreenPresentationOptions:(NSApplicationPresentationOptions)proposedOptions
{
    // Return correct presentation options when in fullscreen.
    if (gCurrentWindow.fullScreen)
    {
        return NSApplicationPresentationFullScreen | NSApplicationPresentationHideDock | NSApplicationPresentationHideMenuBar;
    }

    return proposedOptions;
}

- (void)onActivation:(NSNotification*)notification
{
    NSRunningApplication* runningApplication = [notification.userInfo objectForKey:@"NSWorkspaceApplicationKey"];

    if ([[NSRunningApplication currentApplication] isEqual:runningApplication])
    {
        if (gCurrentWindow.fullScreen)
        {
            [self.window setLevel:NSNormalWindowLevel];
        }
        else
        {
            [self.window setLevel:gDefaultWindowLevel];
        }
    }
}

- (void)onDeactivation:(NSNotification*)notification
{
    NSRunningApplication* runningApplication = [notification.userInfo objectForKey:@"NSWorkspaceApplicationKey"];

    if ([[NSRunningApplication currentApplication] isEqual:runningApplication])
    {
        [self.window setLevel:NSNormalWindowLevel];
    }
}

- (void)windowDidEnterFullScreen:(NSNotification*)notification
{
    gCurrentWindow.fullScreen = true;
}

- (void)windowDidExitFullScreen:(NSNotification*)notification
{
    ForgeNSWindow* window = [notification object];
    gCurrentWindow.fullScreen = false;

    if (window && gCurrentWindow.centered)
        centerWindow(&gCurrentWindow);
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
    NSTrackingAreaOptions options = (NSTrackingCursorUpdate | NSTrackingInVisibleRect | NSTrackingMouseEnteredAndExited |
                                     NSTrackingMouseMoved | NSTrackingActiveInKeyWindow);
    NSTrackingArea*       trackingArea = [[NSTrackingArea alloc] initWithRect:bounds options:options owner:self userInfo:nil];

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

//------------------------------------------------------------------------
// STATIC HELPER FUNCTIONS
//------------------------------------------------------------------------

NSScreen* getNSScreenFromIndex(uint32_t monitorIndex)
{
    // use mainscreen by default
    NSScreen* displayScreen = [NSScreen mainScreen];

    for (NSScreen* screen : [NSScreen screens])
    {
        NSDictionary*     screenDictionary = [screen deviceDescription];
        NSNumber*         idAsNumber = [screenDictionary objectForKey:@"NSScreenNumber"];
        CGDirectDisplayID screenID = [idAsNumber unsignedIntValue];

        if (gMonitors[monitorIndex].displayID == screenID)
        {
            displayScreen = screen;
            break;
        }
    }

    return displayScreen;
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

NSWindowStyleMask PrepareStyleMask(WindowDesc* winDesc)
{
    NSWindowStyleMask styleMask = 0u;

    ForgeMTLView* view = (__bridge ForgeMTLView*)(gCurrentWindow.handle.window);
    if (view != nil)
    {
        ForgeNSWindow* window = (ForgeNSWindow*)view.window;
        styleMask = window.styleMask;
    }

    // Enforce resizable mask for fullscreen
    // requirement for direct-to-display presentation
    // otherwise display is not able to acquire direct mode in fullscreen.
    if (gCurrentWindow.noresizeFrame && !gCurrentWindow.fullScreen)
    {
        styleMask &= ~NSWindowStyleMaskResizable;
    }
    else
    {
        styleMask |= NSWindowStyleMaskResizable;
    }

    NSWindowStyleMask fullScreenStyleMask = NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable;
    NSWindowStyleMask borderlessStyleMask = NSWindowStyleMaskBorderless;
    NSWindowStyleMask windowedStyleMask = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable;

    if (gCurrentWindow.fullScreen)
    {
        styleMask &= ~windowedStyleMask;
        styleMask &= ~borderlessStyleMask;
        styleMask |= fullScreenStyleMask;
    }
    else if (gCurrentWindow.borderlessWindow)
    {
        styleMask &= ~windowedStyleMask;
        styleMask &= ~fullScreenStyleMask;
        styleMask |= borderlessStyleMask;
    }
    else
    {
        styleMask &= ~borderlessStyleMask;
        styleMask &= ~fullScreenStyleMask;
        styleMask |= windowedStyleMask;
    }

    return styleMask;
}

static NSString* screenNameForDisplay(CGDirectDisplayID displayID, NSScreen* screen)
{
    NSString* screenName = screen.localizedName;
    return screenName;
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

    CGDirectDisplayID* onlineDisplayIDs = NULL;
    arrsetlen(onlineDisplayIDs, displayCount);

    error = CGGetOnlineDisplayList(displayCount, onlineDisplayIDs, &displayCount);
    if (error != kCGErrorSuccess)
    {
        ASSERT(0);
    }

    ASSERT(displayCount != 0);

    gMonitorCount = displayCount;
    gMonitors = (MonitorDesc*)tf_calloc(displayCount, sizeof(MonitorDesc));
    gDPIScales = (float2*)tf_calloc(displayCount, sizeof(float2));

    NSArray<NSScreen*>* screens = [NSScreen screens];
    CGDirectDisplayID   mainDisplayID = CGMainDisplayID();

    for (uint32_t displayIndex = 0; displayIndex < displayCount; ++displayIndex)
    {
        CGDirectDisplayID displayID = onlineDisplayIDs[displayIndex];

        NSScreen* displayScreen = nil;
        for (NSScreen* screen : screens)
        {
            NSDictionary*     screenDictionary = [screen deviceDescription];
            NSNumber*         idAsNumber = [screenDictionary objectForKey:@"NSScreenNumber"];
            CGDirectDisplayID screenID = [idAsNumber unsignedIntValue];

            if (displayID == screenID)
            {
                displayScreen = screen;
                break;
            }
        }

        if (displayScreen == nil)
        {
            continue; // Current display iterated (based on displayCount) on is not used as a display screen.
        }

        float dpiScale = (float)[displayScreen backingScaleFactor];
        gDPIScales[displayIndex] = { (float)dpiScale, (float)dpiScale };

        MonitorDesc& display = gMonitors[displayIndex];

        display.displayID = displayID;

        CGSize physicalSize = CGDisplayScreenSize(displayID);
        display.physicalSize[0] = physicalSize.width;
        display.physicalSize[1] = physicalSize.height;

        NSRect frameRect = [displayScreen frame];
        display.defaultResolution.mWidth = frameRect.size.width;
        display.defaultResolution.mHeight = frameRect.size.height;

        NSString* displayName = screenNameForDisplay(displayID, displayScreen);
        // weird edge case where displayScreen's name is null causing a crash in strcpy=
        if (displayName == NULL)
        {
            displayName = @"Unknown";
        }
        strcpy(display.publicDisplayName, displayName.UTF8String);

        id<MTLDevice> metalDevice = CGDirectDisplayCopyCurrentMetalDevice(displayID);
        strcpy(display.publicAdapterName, metalDevice.name.UTF8String);

        // our rects use left / top / right / bottom so need to compute right and bottom
        // by using left + width and top + height
        // use frame of screen
        display.workRect = { (int32_t)frameRect.origin.x, (int32_t)frameRect.origin.y, (int32_t)(frameRect.origin.x + frameRect.size.width),
                             (int32_t)(frameRect.origin.y + frameRect.size.height) };
        display.monitorRect = display.workRect;

        Resolution* displayResolutions = NULL;

        CFStringRef  keys[1] = { kCGDisplayShowDuplicateLowResolutionModes };
        CFBooleanRef values[1] = { kCFBooleanTrue };

        CFDictionaryRef options = CFDictionaryCreate(kCFAllocatorDefault, (const void**)keys, (const void**)values, 1,
                                                     &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

        CFArrayRef displayModes = CGDisplayCopyAllDisplayModes(displayID, options);
        arrsetcap(displayResolutions, CFArrayGetCount(displayModes));
        for (CFIndex i = 0; i < CFArrayGetCount(displayModes); ++i)
        {
            CGDisplayModeRef currentDisplayMode = (CGDisplayModeRef)CFArrayGetValueAtIndex(displayModes, i);

            Resolution displayResolution;
            displayResolution.mWidth = (uint32_t)CGDisplayModeGetWidth(currentDisplayMode);
            displayResolution.mHeight = (uint32_t)CGDisplayModeGetHeight(currentDisplayMode);

            bool duplicate = false;
            // filter out duplicate resolutions
            for (int j = 0; j < arrlen(displayResolutions); j++)
            {
                if (displayResolutions[j].mWidth == displayResolution.mWidth && displayResolutions[j].mHeight == displayResolution.mHeight)
                {
                    duplicate = true;
                    break;
                }
            }

            if (!duplicate)
                arrpush(displayResolutions, displayResolution);
        }

        qsort(displayResolutions, arrlen(displayResolutions), sizeof(Resolution),
              [](const void* lhs, const void* rhs)
              {
                  Resolution* pLhs = (Resolution*)lhs;
                  Resolution* pRhs = (Resolution*)rhs;
                  int         isBigger = pLhs->mHeight * pLhs->mWidth - pRhs->mWidth * pRhs->mHeight;
                  if (isBigger > 0)
                      return 1;
                  if (isBigger < 0)
                      return -1;
                  return 0;
              });

        display.resolutions = displayResolutions;

        Resolution highest = display.resolutions[arrlen(display.resolutions) - 1];

        display.dpi[0] = (uint32_t)((highest.mWidth * 25.4) / display.physicalSize[0]);
        display.dpi[1] = (uint32_t)((highest.mHeight * 25.4) / display.physicalSize[1]);

        CFRelease(displayModes);
        CFRelease(options);

        if (displayID == mainDisplayID && displayIndex != 0)
        {
            std::swap(gMonitors[0], gMonitors[displayIndex]);
        }
    }
    arrfree(onlineDisplayIDs);
}

//------------------------------------------------------------------------
// WINDOW HANDLING INTERFACE FUNCTIONS
//------------------------------------------------------------------------
bool isDebuggerAttached()
{
    static BOOL debuggerIsAttached = false;

    static dispatch_once_t debuggerPredicate;
    // only run once so will not detect if we detach or attach mid run.
    dispatch_once(&debuggerPredicate, ^{
        struct kinfo_proc info;
        size_t            info_size = sizeof(info);
        int               name[4];

        name[0] = CTL_KERN;
        name[1] = KERN_PROC;
        name[2] = KERN_PROC_PID;
        name[3] = getpid(); // from unistd.h, included by Foundation

        if (sysctl(name, 4, &info, &info_size, NULL, 0) == -1)
        {
            NSLog(@"ERROR: Checking for a running debugger via sysctl() failed: %s", strerror(errno));
            debuggerIsAttached = false;
        }

        if (!debuggerIsAttached && (info.kp_proc.p_flag & P_TRACED) != 0)
            debuggerIsAttached = true;
    });

    // this will wait for the dispatch once the first time it's ran otherwise will use cached value.
    return debuggerIsAttached;
}

void openWindow(const char* app_name, WindowDesc* winDesc, id<MTLDevice> device, id<RenderDestinationProvider> delegateRenderProvider,
                int32_t monitorIndex)
{
    NSWindowStyleMask styleMask = PrepareStyleMask(winDesc);

    NSRect    viewRect{ { (float)gCurrentWindow.windowedRect.left, (float)gCurrentWindow.windowedRect.top },
                     { (float)getRectWidth(&gCurrentWindow.clientRect), (float)getRectHeight(&gCurrentWindow.clientRect) } };
    NSScreen* activeScreen = getNSScreenFromIndex(monitorIndex);

    NSRect styleAdjustedRect = [NSWindow frameRectForContentRect:viewRect styleMask:styleMask];
    // divide by retina factor
    // otherwise position is all off
    styleAdjustedRect.origin.x /= [activeScreen backingScaleFactor];
    styleAdjustedRect.origin.y /= [activeScreen backingScaleFactor];

    // init window with the right size and the given Screen to ensure the correct window properties are setup from the start.
    ForgeNSWindow* window = [[ForgeNSWindow alloc] initWithContentRect:styleAdjustedRect
                                                             styleMask:styleMask
                                                               backing:NSBackingStoreBuffered
                                                                 defer:YES
                                                                screen:activeScreen];
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
    gCurrentWindow.handle.window = (void*)CFBridgingRetain(view);

    float          dpiScale[2];
    const uint32_t monitorIdx = getActiveMonitorIdx();
    getMonitorDpiScale(monitorIdx, dpiScale);
    NSSize windowSize = CGSizeMake(viewRect.size.width / dpiScale[0], viewRect.size.height / dpiScale[1]);
    [window setContentSize:windowSize];

    NSRect                bounds = [view bounds];
    NSTrackingAreaOptions options = (NSTrackingCursorUpdate | NSTrackingInVisibleRect | NSTrackingMouseEnteredAndExited |
                                     NSTrackingMouseMoved | NSTrackingActiveInKeyWindow);
    NSTrackingArea*       trackingArea = [[NSTrackingArea alloc] initWithRect:bounds options:options owner:view userInfo:nil];

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
    // NSPopupWindowLevel is the default as it allows the window
    // to be on top of the taskbar.
    if (isDebuggerAttached())
    {
        gDefaultWindowLevel = NSNormalWindowLevel;
        LOGF(LogLevel::eINFO,
             "Setting Window level to NSNormalWindowLevel instead of NSPopupWindowLevel to allow debugger window to be on top of app.");
    }
    [view.window setLevel:gDefaultWindowLevel];

    styleAdjustedRect.origin = [window convetToTopLeft:styleAdjustedRect.origin];
    if (gCurrentWindow.centered)
    {
        centerWindow(winDesc);
    }
    else
    {
        [window setFrameOrigin:styleAdjustedRect.origin];

        RectDesc newRect = [window setRectFromNSRect:styleAdjustedRect];
        windowMovedOrResized(&newRect);
    }

    [window updateWindowRect:winDesc];

    if (gCurrentWindow.fullScreen)
    {
        gCurrentWindow.fullScreen = false;
        setFullscreen(winDesc);
    }

    setMousePositionRelative(winDesc, styleAdjustedRect.size.width / 2.0 / dpiScale[0], styleAdjustedRect.size.height / 2.0 / dpiScale[1]);
}

void closeWindow(WindowDesc* winDesc)
{
    // No-op
}

void setWindowRect(WindowDesc* winDesc, const RectDesc* pRect)
{
    ForgeMTLView* view = (__bridge ForgeMTLView*)(gCurrentWindow.handle.window);
    if (view == nil)
    {
        return;
    }

    gCurrentWindow.windowedRect = *pRect;

    if (gCurrentWindow.centered)
    {
        centerWindow(winDesc);
    }
    else
    {
        NSRect         frameRect;
        float          dpiScale[2];
        const uint32_t monitorIdx = getActiveMonitorIdx();
        getMonitorDpiScale(monitorIdx, dpiScale);
        frameRect.origin.x = pRect->left / dpiScale[0];
        frameRect.origin.y = (getRectHeight(&gCurrentWindow.fullscreenRect) - pRect->bottom) / dpiScale[1];
        frameRect.size.width = (float)getRectWidth(pRect) / dpiScale[0];
        frameRect.size.height = (float)getRectHeight(pRect) / dpiScale[1];

        ForgeNSWindow* window = (ForgeNSWindow*)view.window;
        window.styleMask = PrepareStyleMask(winDesc);

        [window setFrame:frameRect display:true];

        RectDesc newRect = [window setRectFromNSRect:frameRect];
        windowMovedOrResized(&newRect);
    }
}

void setWindowSize(WindowDesc* winDesc, unsigned width, unsigned height)
{
    RectDesc desc{ 0, 0, (int)width, (int)height };
    setWindowRect(winDesc, &desc);
}

void toggleBorderless(WindowDesc* winDesc, unsigned width, unsigned height)
{
    ForgeMTLView* view = (__bridge ForgeMTLView*)(gCurrentWindow.handle.window);
    if (view == nil)
    {
        return;
    }

    ForgeNSWindow* window = (ForgeNSWindow*)view.window;
    gCurrentWindow.borderlessWindow = !gCurrentWindow.borderlessWindow;
    if (gCurrentWindow.fullScreen)
    {
        return;
    }

    window.styleMask = PrepareStyleMask(winDesc);
    NSRect         contentBounds = view.bounds;
    float          dpiScale[2];
    const uint32_t monitorIdx = getActiveMonitorIdx();
    getMonitorDpiScale(monitorIdx, dpiScale);
    uint32_t currentWidth = (uint32_t)(contentBounds.size.width * dpiScale[0] + 1e-6);
    uint32_t currentHeight = (uint32_t)(contentBounds.size.height * dpiScale[1] + 1e-6);
    if (currentWidth != width || currentHeight != height)
    {
        setWindowSize(winDesc, width, height);
    }
}

void toggleFullscreen(WindowDesc* winDesc)
{
    ForgeMTLView* view = (__bridge ForgeMTLView*)(gCurrentWindow.handle.window);
    if (view == nil)
    {
        return;
    }

    ForgeNSWindow* window = (ForgeNSWindow*)view.window;

    bool isFullscreen = !(window.styleMask & NSWindowStyleMaskFullScreen);

    float          dpiScale[2];
    const uint32_t monitorIdx = getActiveMonitorIdx();
    getMonitorDpiScale(monitorIdx, dpiScale);
    if (isFullscreen)
    {
        gCurrentWindow.fullScreen = isFullscreen;
        window.styleMask = PrepareStyleMask(winDesc);

        NSSize size = { (CGFloat)getRectWidth(&gCurrentWindow.fullscreenRect) / dpiScale[0],
                        (CGFloat)getRectHeight(&gCurrentWindow.fullscreenRect) / dpiScale[1] };
        [window setContentSize:size];

        [view.window setLevel:NSNormalWindowLevel];

        gCurrentWindow.mWndX = 0;
        gCurrentWindow.mWndY = 0;
        gCurrentWindow.mWndW = getRectWidth(&gCurrentWindow.fullscreenRect);
        gCurrentWindow.mWndH = getRectHeight(&gCurrentWindow.fullscreenRect);
        gCurrentWindow.mWindowMode = WM_FULLSCREEN;
    }

    [window toggleFullScreen:window];

    if (!isFullscreen)
    {
        NSSize size = { (CGFloat)getRectWidth(&gCurrentWindow.clientRect) / dpiScale[0],
                        (CGFloat)getRectHeight(&gCurrentWindow.clientRect) / dpiScale[1] };
        [window setContentSize:size];

        gCurrentWindow.fullScreen = isFullscreen;
        window.styleMask = PrepareStyleMask(winDesc);

        [view.window setLevel:gDefaultWindowLevel];

        gCurrentWindow.mWndX = gCurrentWindow.windowedRect.left;
        gCurrentWindow.mWndY = gCurrentWindow.windowedRect.top;
        gCurrentWindow.mWndW = getRectWidth(&gCurrentWindow.clientRect);
        gCurrentWindow.mWndH = getRectHeight(&gCurrentWindow.clientRect);
        gCurrentWindow.mWindowMode = gCurrentWindow.borderlessWindow ? WM_BORDERLESS : WM_WINDOWED;
    }
}

void setWindowed(WindowDesc* winDesc, unsigned width, unsigned height)
{
    winDesc->maximized = false;

    if (winDesc->fullScreen)
    {
        toggleFullscreen(winDesc);
    }
    if (winDesc->borderlessWindow)
    {
        toggleBorderless(winDesc, getRectWidth(&winDesc->clientRect), getRectHeight(&winDesc->clientRect));
    }
    winDesc->mWindowMode = WindowMode::WM_WINDOWED;
}

void setBorderless(WindowDesc* winDesc, unsigned width, unsigned height)
{
    if (winDesc->fullScreen)
    {
        toggleFullscreen(winDesc);
        if (!winDesc->borderlessWindow)
            toggleBorderless(winDesc, width, height);
        winDesc->mWindowMode = WindowMode::WM_BORDERLESS;
    }
    else if (!winDesc->borderlessWindow)
    {
        winDesc->mWindowMode = WindowMode::WM_BORDERLESS;
        toggleBorderless(winDesc, width, height);
        if (!winDesc->borderlessWindow)
            winDesc->mWindowMode = WindowMode::WM_WINDOWED;
    }
}

void setFullscreen(WindowDesc* winDesc)
{
    if (!winDesc->fullScreen)
    {
        toggleFullscreen(winDesc);
        winDesc->mWindowMode = WindowMode::WM_FULLSCREEN;
    }
}

void showWindow(WindowDesc* winDesc)
{
    gCurrentWindow.hide = false;
    ForgeMTLView* view = (__bridge ForgeMTLView*)(gCurrentWindow.handle.window);
    if (view == nil)
    {
        return;
    }

    [[NSApplication sharedApplication] unhide:nil];
}

void hideWindow(WindowDesc* winDesc)
{
    gCurrentWindow.hide = true;
    ForgeMTLView* view = (__bridge ForgeMTLView*)(gCurrentWindow.handle.window);
    if (view == nil)
    {
        return;
    }

    [[NSApplication sharedApplication] hide:nil];
}

void maximizeWindow(WindowDesc* winDesc)
{
    gCurrentWindow.hide = false;
    ForgeMTLView* view = (__bridge ForgeMTLView*)(gCurrentWindow.handle.window);
    if (view == nil)
    {
        return;
    }

    ForgeNSWindow* window = (ForgeNSWindow*)view.window;
    // don't call zoom again if it's already zoomed
    // zoomed == maximized
    if (![window isZoomed])
        [window zoom:nil];

    gCurrentWindow.maximized = [window isZoomed];
}

void minimizeWindow(WindowDesc* winDesc)
{
    gCurrentWindow.hide = true;
    ForgeMTLView* view = (__bridge ForgeMTLView*)(gCurrentWindow.handle.window);
    if (view == nil)
    {
        return;
    }

    ForgeNSWindow* window = (ForgeNSWindow*)view.window;
    [window miniaturize:nil];
    gCurrentWindow.minimized = true;
}

NSRect getCenteredWindowRect(WindowDesc* winDesc)
{
    ForgeMTLView* view = (__bridge ForgeMTLView*)(gCurrentWindow.handle.window);
    if (view == nil)
    {
        return NSRect();
    }

    uint32_t fsHalfWidth = getRectWidth(&gCurrentWindow.fullscreenRect) >> 1;
    uint32_t fsHalfHeight = getRectHeight(&gCurrentWindow.fullscreenRect) >> 1;
    uint32_t windowWidth = getRectWidth(&gCurrentWindow.clientRect);
    uint32_t windowHeight = getRectHeight(&gCurrentWindow.clientRect);
    uint32_t windowHalfWidth = windowWidth >> 1;
    uint32_t windowHalfHeight = windowHeight >> 1;

    // use int32_t in case of window larger than full size.
    int32_t X = fsHalfWidth - windowHalfWidth;
    int32_t Y = fsHalfHeight - windowHalfHeight;

    NSRect         contentRect;
    float          dpiScale[2];
    const uint32_t monitorIdx = getActiveMonitorIdx();
    getMonitorDpiScale(monitorIdx, dpiScale);
    // take into account left position otherwise we might end up on the wrong screen
    contentRect.origin.x = (float)(gCurrentWindow.fullscreenRect.left + X) / dpiScale[0];
    contentRect.origin.y =
        (float)(gCurrentWindow.fullscreenRect.top + getRectHeight(&gCurrentWindow.fullscreenRect) - (float)(Y + windowHeight)) /
        dpiScale[1];
    contentRect.size.width = (float)(windowWidth) / dpiScale[0];
    contentRect.size.height = (float)(windowHeight) / dpiScale[1];

    ForgeNSWindow* window = (ForgeNSWindow*)view.window;
    window.styleMask = PrepareStyleMask(winDesc);
    NSRect styleAdjustedRect = [NSWindow frameRectForContentRect:contentRect styleMask:window.styleMask];

    return styleAdjustedRect;
}

void centerWindow(WindowDesc* winDesc)
{
    ForgeMTLView* view = (__bridge ForgeMTLView*)(gCurrentWindow.handle.window);
    if (view == nil)
    {
        return;
    }

    gCurrentWindow.centered = true;

    NSRect styleAdjustedRect = getCenteredWindowRect(winDesc);

    ForgeNSWindow* window = (ForgeNSWindow*)view.window;
    [window setFrame:styleAdjustedRect display:true];

    RectDesc newRect = [window setRectFromNSRect:styleAdjustedRect];
    windowMovedOrResized(&newRect);
}

//------------------------------------------------------------------------
// CURSOR AND MOUSE HANDLING INTERFACE FUNCTIONS
//------------------------------------------------------------------------

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
    if (gCurrentWindow.mCursorHidden)
    {
        gCurrentWindow.mCursorHidden = false;
        [NSCursor unhide];
    }
}

void hideCursor()
{
    if (!gCurrentWindow.mCursorHidden)
    {
        gCurrentWindow.mCursorHidden = true;
        [NSCursor hide];
    }
}

void captureCursor(WindowDesc* winDesc, bool bEnable)
{
    ASSERT(winDesc);

    if (gCurrentWindow.cursorCaptured != bEnable)
    {
        if (bEnable)
        {
            hideCursor();
            CGAssociateMouseAndMouseCursorPosition(false);
        }
        else
        {
            showCursor();
            CGAssociateMouseAndMouseCursorPosition(true);
        }

        gCurrentWindow.cursorCaptured = bEnable;
    }
}

bool isCursorInsideTrackingArea() { return gCursorInsideTrackingArea; }

void setMousePositionRelative(const WindowDesc* winDesc, int32_t x, int32_t y)
{
    ForgeMTLView* view = (__bridge ForgeMTLView*)(gCurrentWindow.handle.window);
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

    CGPoint location;
    location.x = x;
    location.y = y;
    // use view and window to convert point to be at right location
    // this takes into account screen frame and positioning of multiple monitors
    location = [view convertPoint:location toView:nil];
    location = [forgeWindow convertRectToScreen:(NSRect){ .origin = location }].origin;
    CGWarpMouseCursorPosition(location);
}

void setMousePositionAbsolute(int32_t x, int32_t y)
{
    CGPoint location;
    location.x = x;
    location.y = y;
    CGWarpMouseCursorPosition(location);
}

//------------------------------------------------------------------------
// MONITOR AND RESOLUTION HANDLING INTERFACE FUNCTIONS
//------------------------------------------------------------------------

MonitorDesc* getMonitor(uint32_t index)
{
    ASSERT(gMonitorCount > index);
    return &gMonitors[index];
}

uint32_t getMonitorCount() { return gMonitorCount; }

uint32_t getActiveMonitorIdx(void)
{
    NSScreen*     screen = nil;
    ForgeMTLView* view = (__bridge ForgeMTLView*)(gCurrentWindow.handle.window);
    if (view != nil)
    {
        ForgeNSWindow* forgeWindow = (ForgeNSWindow * __nullable) view.window;
        screen = [forgeWindow screen];
    }

    if (screen == nil)
    {
        screen = [NSScreen mainScreen];
    }

    for (uint32_t i = 0; i < gMonitorCount; ++i)
    {
        NSString* screenName = screenNameForDisplay(gMonitors[i].displayID, screen);
        if (strcmp(screenName.UTF8String, gMonitors[i].publicDisplayName) == 0)
        {
            return i;
        }
    }

    return 0;
}

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

void getMonitorDpiScale(uint32_t monitorIndex, float dpiScale[2])
{
    ASSERT(monitorIndex < gMonitorCount);

    // get the appropriate backing scale factor
    // in case monitor index doesn't exist in list of NSScreen we default to mainscreen
    NSScreen* displayScreen = getNSScreenFromIndex(monitorIndex);

    // if display screen was found
    // should always be true except if display gets disconnected but gMonitors doesn't get updated.
    // for that case just
    dpiScale[0] = dpiScale[1] = [displayScreen backingScaleFactor];
}

void getRecommendedResolution(RectDesc* rect)
{
    float          dpiScale[2]{};
    const uint32_t monitorIdx = getActiveMonitorIdx();
    getMonitorDpiScale(monitorIdx, dpiScale);
    // use screen 0 instead of main screen as this can get called without an active window.
    // mainscreen refers to the key window where the mouse currently is.
    // use 75% of full screen rect
    // NSRect mainScreenRect = [[NSScreen main][0] frame];
    NSRect mainScreenRect = [[NSScreen mainScreen] frame];
    *rect = RectDesc{ 0, 0, (int32_t)(mainScreenRect.size.width * dpiScale[0] * 0.75),
                      (int32_t)(mainScreenRect.size.height * dpiScale[1] * 0.75) };
}

void getRecommendedWindowRect(WindowDesc* winDesc, RectDesc* rect) { getRecommendedResolution(rect); }

void setResolution(const MonitorDesc* pMonitor, const Resolution* pMode)
{
    ASSERT(pMonitor != nil);
    ASSERT(pMode != nil);

    CGDirectDisplayID display = pMonitor->displayID;

    CGRect currBounds = CGDisplayBounds(display);
    if (pMode->mWidth == currBounds.size.width && pMode->mHeight == currBounds.size.height)
    {
        return;
    }

    CGDisplayModeRef bestMode = FindClosestResolution(display, pMode);
    ASSERT(bestMode != nil);

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

bool getResolutionSupport(const MonitorDesc* pMonitor, const Resolution* pRes) { return false; }

#endif
