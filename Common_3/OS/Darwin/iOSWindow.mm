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

#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import <QuartzCore/CoreAnimation.h>
#import <UIKit/UIKit.h>

#include "../../Utilities/ThirdParty/OpenSource/Nothings/stb_ds.h"

#include "../../Application/Interfaces/IApp.h"
#include "../../Utilities/Interfaces/ILog.h"
#include "../Interfaces/IOperatingSystem.h"
#include "../Interfaces/IInput.h"

#include "../../Utilities/Math/MathTypes.h"

IApp* pWindowAppRef = NULL;

float2 gRetinaScale = { 1.0f, 1.0f };
int    gDeviceWidth;
int    gDeviceHeight;

MonitorDesc* gMonitors = NULL;

//------------------------------------------------------------------------
// STATIC STRUCTS
//------------------------------------------------------------------------

@protocol ForgeViewDelegate <NSObject>
@required
- (void)drawRectResized:(CGSize)size;
@end

@interface ForgeMTLViewController: UIViewController
- (id)initWithFrame:(CGRect)FrameRect device:(id<MTLDevice>)device hdr:(bool)hdr;
@end

@interface ForgeMTLView: UIView
+ (Class)layerClass;
- (void)layoutSubviews;
- (void)resizeDrawable:(CGFloat)scaleFactor;
@property(nonatomic, nullable) id<ForgeViewDelegate> delegate;
@end

@implementation ForgeMTLView

// UIView override to determine what kind of backing layer we use
// CAMetalLayer for metal based apps
+ (Class)layerClass
{
    return [CAMetalLayer class];
}

- (BOOL)opaque
{
    return true;
}

// UIView override to update layout of all the subviews
// this includes main and input views.
- (void)layoutSubviews
{
    [super layoutSubviews];
    [self resizeDrawable:gRetinaScale[0]];
}

// Resize the CAMetalLayer to match the retina scale that we request.
// This is used to determine the size based on the update scale factor
- (void)resizeDrawable:(CGFloat)scaleFactor
{
    CGSize newSize = self.bounds.size;
    newSize.width *= scaleFactor;
    newSize.height *= scaleFactor;

    if (newSize.width <= 0 || newSize.width <= 0)
    {
        return;
    }

    if (newSize.width == ((CAMetalLayer*)(self.layer)).drawableSize.width &&
        newSize.height == ((CAMetalLayer*)(self.layer)).drawableSize.height)
    {
        return;
    }

    // update globals to match newest scale factor.
    gRetinaScale[0] = scaleFactor;
    gRetinaScale[1] = scaleFactor;
    gDeviceWidth = (int)newSize.width;
    gDeviceHeight = (int)newSize.height;
    ((CAMetalLayer*)(self.layer)).drawableSize = newSize;
    ((CAMetalLayer*)(self.layer)).contentsScale = scaleFactor;
    self.contentScaleFactor = scaleFactor;
    self.window.contentScaleFactor = scaleFactor;
    // Make sure app is notified of resize
    [_delegate drawRectResized:self.bounds.size];
}

@end

@implementation ForgeMTLViewController

- (id)initWithFrame:(CGRect)FrameRect device:(id<MTLDevice>)device hdr:(bool)hdr
{
    self = [super init];
    self.view = [[ForgeMTLView alloc] initWithFrame:FrameRect];

    CAMetalLayer* metalLayer = (CAMetalLayer*)self.view.layer;
    metalLayer.device = device;
    metalLayer.framebufferOnly = YES; // todo: optimized way
    // set opaque to allow for improved performance and avoiding composition with various layers
    metalLayer.opaque = true;
    metalLayer.pixelFormat = hdr ? MTLPixelFormatRGBA16Float : MTLPixelFormatBGRA8Unorm;
    metalLayer.drawableSize = CGSizeMake(self.view.frame.size.width, self.view.frame.size.height);
    // ensure content scale of layer matches either native or the app defined scale.
    metalLayer.contentsScale = [[UIScreen mainScreen] nativeScale];
    return self;
}

- (BOOL)prefersStatusBarHidden
{
    return pWindowAppRef->mSettings.mShowStatusBar ? NO : YES;
}

- (BOOL)prefersHomeIndicatorAutoHidden
{
    // when YES is passed then it's using default behavior
    // when NO is passed, it's deferred based on gestures below.
    return pWindowAppRef->mSettings.mShowStatusBar ? YES : NO;
}

- (UIRectEdge)preferredScreenEdgesDeferringSystemGestures
{
    // If status bar is shown then we use default rect edge none which allows tapping anywhere.
    // If status bar is hidden, then we need to specific only top and bottom otherwise gestures still
    // might trigger notification / background manager.
    return pWindowAppRef->mSettings.mShowStatusBar ? UIRectEdgeNone : UIRectEdgeBottom | UIRectEdgeTop;
}

static void ProcessTouches(UIView* view, NSSet<UITouch*>* touches, uint8_t phase)
{
    extern void platformTouchBeganEvent(const int32_t id, const int32_t pos[2]);
    extern void platformTouchEndedEvent(const int32_t id, const int32_t pos[2]);
    extern void platformTouchMovedEvent(const int32_t id, const int32_t pos[2]);

    for (UITouch* touch in touches)
    {
        CGPoint viewPos = [touch locationInView:view];
        int32_t id = (int32_t)((intptr_t)touch);
        int32_t pos[2] = {};
        pos[0] = viewPos.x * gRetinaScale.x;
        pos[1] = viewPos.y * gRetinaScale.y;
        if (phase & 0x1)
        {
            platformTouchBeganEvent(id, pos);
        }
        else if (phase & 0x2)
        {
            platformTouchEndedEvent(id, pos);
        }
        else
        {
            platformTouchMovedEvent(id, pos);
        }
    }
}

- (void)touchesBegan:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event
{
    ProcessTouches(self.view, touches, 0x1);
}

- (void)touchesEnded:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event
{
    ProcessTouches(self.view, touches, 0x2);
}

- (void)touchesCancelled:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event
{
    ProcessTouches(self.view, touches, 0x2);
}

- (void)touchesMoved:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event
{
    ProcessTouches(self.view, touches, 0x4);
}

@end

//------------------------------------------------------------------------
// STATIC HELPER FUNCTIONS
//------------------------------------------------------------------------

//------------------------------------------------------------------------
// WINDOW HANDLING INTERFACE FUNCTIONS
//------------------------------------------------------------------------

void openWindow(const char* app_name, WindowDesc* winDesc, id<MTLDevice> device)
{
    // Adjust window size to match retina scaling.
    CGFloat scale = [[UIScreen mainScreen] nativeScale];
    if (pWindowAppRef->mSettings.mContentScaleFactor >= 1.0f)
        gRetinaScale = { (float)pWindowAppRef->mSettings.mContentScaleFactor, (float)pWindowAppRef->mSettings.mContentScaleFactor };
    else
        gRetinaScale = { (float)scale, (float)scale };

    CGRect ViewRect = [[UIScreen mainScreen] bounds];

    UIWindow* Window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
    [Window setContentScaleFactor:gRetinaScale[0]];
    // set opaque to allow for improved performance and avoiding composition with various layers
    [Window setOpaque:YES];
    [Window makeKeyAndVisible];
    winDesc->handle.window = (void*)CFBridgingRetain(Window);
    gDeviceWidth = UIScreen.mainScreen.bounds.size.width * gRetinaScale.x;
    gDeviceHeight = UIScreen.mainScreen.bounds.size.height * gRetinaScale.y;

    ForgeMTLViewController* ViewController = [[ForgeMTLViewController alloc] initWithFrame:ViewRect device:device hdr:NO];

    // Update retina scale for View
    [(ForgeMTLView*)ViewController.view setContentScaleFactor:gRetinaScale[0]];
    // Update RootViewController with our ForgeMTLViewController
    [Window setRootViewController:ViewController];
}

void closeWindow(WindowDesc* winDesc)
{
    // No-op
}

void setWindowRect(WindowDesc* winDesc, const RectDesc* rect)
{
    // No-op
}

void setWindowSize(WindowDesc* winDesc, unsigned width, unsigned height)
{
    // No-op
}

void setWindowClientRect(WindowDesc* winDesc, const RectDesc* rect)
{
    // No-op
}

void setWindowClientSize(WindowDesc* winDesc, unsigned width, unsigned height)
{
    // No-op
}

void setWindowed(WindowDesc* winDesc)
{
    // No-op
}

void setBorderless(WindowDesc* winDesc)
{
    // No-op
}

void toggleFullscreen(WindowDesc* winDesc)
{
    // No-op
}

void setFullscreen(WindowDesc* pWindow)
{
    // No-op
}

void showWindow(WindowDesc* winDesc)
{
    // No-op
}

void hideWindow(WindowDesc* winDesc)
{
    // No-op
}

void maximizeWindow(WindowDesc* winDesc)
{
    // No-op
}

void minimizeWindow(WindowDesc* winDesc)
{
    // No-op
}

void centerWindow(WindowDesc* winDesc)
{
    // No-op
}

//------------------------------------------------------------------------
// CURSOR AND MOUSE HANDLING INTERFACE FUNCTIONS
//------------------------------------------------------------------------

void* createCursor(const char* path) { return NULL; }

void setCursor(void* cursor)
{
    // No-op
}

void showCursor(void)
{
    // No-op
}

void hideCursor(void)
{
    // No-op
}

bool isCursorInsideTrackingArea(void) { return true; }

void setMousePositionRelative(const WindowDesc* winDesc, int32_t x, int32_t y)
{
    // No-op
}

void setMousePositionAbsolute(int32_t x, int32_t y)
{
    // No-op
}

void captureCursor(WindowDesc* winDesc, bool bEnable)
{
    ASSERT(winDesc);

    if (winDesc->cursorCaptured != bEnable)
    {
        winDesc->cursorCaptured = bEnable;
    }
}

//------------------------------------------------------------------------
// MONITOR AND RESOLUTION HANDLING INTERFACE FUNCTIONS
//------------------------------------------------------------------------

MonitorDesc* getMonitor(uint32_t index)
{
    ASSERT(arrlenu(gMonitors) > index);
    return &gMonitors[index];
}

uint32_t getMonitorCount(void) { return 1; }

uint32_t getActiveMonitorIdx(void) { return 0; }

void getDpiScale(float array[2])
{
    array[0] = gRetinaScale.x;
    array[1] = gRetinaScale.y;
}

void getMonitorDpiScale(uint32_t monitorIndex, float dpiScale[2])
{
    UNREF_PARAM(monitorIndex);
    getDpiScale(dpiScale);
}

void getRecommendedResolution(RectDesc* rect) { *rect = RectDesc{ 0, 0, gDeviceWidth, gDeviceHeight }; }

void getRecommendedWindowRect(WindowDesc* winDesc, RectDesc* rect) { getRecommendedResolution(rect); }

void setResolution(const MonitorDesc* pMonitor, const Resolution* pRes)
{
    // No-op
}

bool getResolutionSupport(const MonitorDesc* pMonitor, const Resolution* pRes) { return false; }

//------------------------------------------------------------------------
// MONITOR AND RESOLUTION HANDLING INTERFACE FUNCTIONS
//------------------------------------------------------------------------

#endif
