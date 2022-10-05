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

#ifdef __APPLE__

#import <UIKit/UIKit.h>
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>

#include "../../Application/Config.h"

#include "../../Application/Interfaces/IApp.h"
#include "../Interfaces/IOperatingSystem.h"
#include "../../Utilities/Interfaces/ILog.h"

#include "../../Utilities/Math/MathTypes.h"

#include "../../Utilities/ThirdParty/OpenSource/Nothings/stb_ds.h"

IApp* pWindowAppRef = NULL;

float2 gRetinaScale = { 1.0f, 1.0f };
int     gDeviceWidth;
int     gDeviceHeight;

MonitorDesc* gMonitors = NULL;

//------------------------------------------------------------------------
// STATIC STRUCTS
//------------------------------------------------------------------------

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

@implementation ForgeMTLView

+ (Class)layerClass
{
	if (@available(iOS 13.0, *))
	{
		return [CAMetalLayer class];
	}
	else
	{
		return [CALayer class];
	}

}

- (void)layoutSubviews
{
	[_delegate drawRectResized:self.bounds.size];
	if (pWindowAppRef->mSettings.mContentScaleFactor >= 1.f)
	{
		[self setContentScaleFactor:pWindowAppRef->mSettings.mContentScaleFactor];
		for (UIView* subview in self.subviews)
		{
			[subview setContentScaleFactor:pWindowAppRef->mSettings.mContentScaleFactor];
		}
		if (@available(iOS 13.0, *))
		{
			((CAMetalLayer*)(self.layer)).drawableSize = CGSizeMake(
				self.frame.size.width * pWindowAppRef->mSettings.mContentScaleFactor, self.frame.size.height * pWindowAppRef->mSettings.mContentScaleFactor);
		}
	}
	else
	{
		[self setContentScaleFactor:gRetinaScale.x];
		for (UIView* subview in self.subviews)
		{
			[subview setContentScaleFactor:gRetinaScale.x];
		}
		if (@available(iOS 13.0, *))
		{
			((CAMetalLayer*)(self.layer)).drawableSize =
				CGSizeMake(self.frame.size.width * gRetinaScale.x, self.frame.size.height * gRetinaScale.y);
		}
	}
}
@end

@implementation ForgeMTLViewController

- (id)initWithFrame:(CGRect)FrameRect device:(id<MTLDevice>)device display:(int)in_displayID hdr:(bool)hdr vsync:(bool)vsync
{
	self = [super init];
	self.view = [[ForgeMTLView alloc] initWithFrame:FrameRect];

	if (@available(iOS 13.0, *))
	{
		CAMetalLayer* metalLayer = (CAMetalLayer*)self.view.layer;
		metalLayer.device = device;
		metalLayer.framebufferOnly = YES;    //todo: optimized way
		metalLayer.pixelFormat = hdr ? MTLPixelFormatRGBA16Float : MTLPixelFormatBGRA8Unorm;
		metalLayer.drawableSize = CGSizeMake(self.view.frame.size.width, self.view.frame.size.height);
	}

	return self;
}

- (BOOL)prefersStatusBarHidden
{
	return pWindowAppRef->mSettings.mShowStatusBar ? NO : YES;
}

- (BOOL) prefersHomeIndicatorAutoHidden
{
    // when YES is passed then it's using default behavior
    // when NO is passed, it's deferred based on gestures below.
    return pWindowAppRef->mSettings.mShowStatusBar ? YES : NO;
}

- (UIRectEdge) preferredScreenEdgesDeferringSystemGestures
{
    // If status bar is shown then we use default rect edge none which allows tapping anywhere.
    // If status bar is hidden, then we need to specific only top and bottom otherwise gestures still
    // might trigger notification / background manager.
    return pWindowAppRef->mSettings.mShowStatusBar ? UIRectEdgeNone : UIRectEdgeBottom | UIRectEdgeTop;
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
	CGRect    ViewRect{ { 0, 0 }, { 1280, 720 } };    //Initial default values
	UIWindow* Window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];

	[Window setOpaque:YES];
	[Window makeKeyAndVisible];
	winDesc->handle.window = (void*)CFBridgingRetain(Window);

	// Adjust window size to match retina scaling.
	CGFloat scale = UIScreen.mainScreen.scale;
	if (pWindowAppRef->mSettings.mContentScaleFactor >= 1.0f)
		gRetinaScale = { (float)pWindowAppRef->mSettings.mContentScaleFactor, (float)pWindowAppRef->mSettings.mContentScaleFactor };
	else
		gRetinaScale = { (float)scale, (float)scale };

	gDeviceWidth = UIScreen.mainScreen.bounds.size.width * gRetinaScale.x;
	gDeviceHeight = UIScreen.mainScreen.bounds.size.height * gRetinaScale.y;

	ForgeMTLViewController* ViewController = [[ForgeMTLViewController alloc] initWithFrame:ViewRect
																					device:device
																				   display:0
																					   hdr:NO
																					 vsync:NO];
	[Window setRootViewController:ViewController];
}

void closeWindow(const WindowDesc* winDesc)
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

void toggleBorderless(WindowDesc* winDesc, unsigned width, unsigned height)
{
	// No-op
}

void toggleFullscreen(WindowDesc* pWindow)
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

void* createCursor(const char* path)
{
	return NULL;
}

void  setCursor(void* cursor)
{
	// No-op
}

void  showCursor(void)
{
	// No-op
}

void  hideCursor(void)
{
	// No-op
}

bool  isCursorInsideTrackingArea(void)
{
	return true;
}

void  setMousePositionRelative(const WindowDesc* winDesc, int32_t x, int32_t y)
{
	// No-op
}

void  setMousePositionAbsolute(int32_t x, int32_t y)
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

uint32_t getMonitorCount(void)
{
	return 1;
}

void getDpiScale(float array[2])
{
	array[0] = gRetinaScale.x;
	array[1] = gRetinaScale.y;
}

void getRecommendedResolution(RectDesc* rect)
{
	*rect = RectDesc{ 0, 0, gDeviceWidth, gDeviceHeight };
}

void setResolution(const MonitorDesc* pMonitor, const Resolution* pRes)
{
	// No-op
}

bool getResolutionSupport(const MonitorDesc* pMonitor, const Resolution* pRes)
{
	return false;
}

//------------------------------------------------------------------------
// MONITOR AND RESOLUTION HANDLING INTERFACE FUNCTIONS
//------------------------------------------------------------------------

#endif
