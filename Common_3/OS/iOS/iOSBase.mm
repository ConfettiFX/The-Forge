/*
 * Copyright (c) 2018 Confetti Interactive Inc.
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
#include "../Interfaces/IMemoryManager.h"
#include "../Interfaces/IFileSystem.h"
#include "../Interfaces/IApp.h"

#define CONFETTI_WINDOW_CLASS L"confetti"
#define MAX_KEYS 256

#define elementsOf(a) (sizeof(a) / sizeof((a)[0]))

static bool gAppRunning;
static WindowsDesc gCurrentWindow;
static tinystl::vector <MonitorDesc> gMonitors;
static int gCurrentTouchEvent = 0;

static float gRetinaScale = 1.0f;
static int gDeviceWidth;
static int gDeviceHeight;

static tinystl::vector <MonitorDesc> monitors;

namespace PlatformEvents
{
    extern void onTouch(const TouchEventData* pData);
    extern void onTouchMove(const TouchEventData* pData);
}

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

bool isRunning()
{
	return gAppRunning;
}

void getRecommendedResolution(RectDesc* rect)
{
    *rect = RectDesc{ 0, 0, gDeviceWidth, gDeviceHeight };
}

void requestShutDown()
{
    gAppRunning = false;
}

/************************************************************************/
// Time Related Functions
/************************************************************************/

unsigned getSystemTime()
{
    long            ms; // Milliseconds
    time_t          s;  // Seconds
    struct timespec spec;
    
    clock_gettime(CLOCK_REALTIME, &spec);
    
    s  = spec.tv_sec;
    ms = round(spec.tv_nsec / 1.0e6); // Convert nanoseconds to milliseconds
    
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

unsigned getTimeSinceStart()
{
	return (unsigned)time(NULL);
}

/************************************************************************/
// App Entrypoint
/************************************************************************/

static IApp* pApp = NULL;

int iOSMain(int argc, char** argv, IApp* app)
{
    pApp = app;    
    @autoreleasepool {
        return UIApplicationMain(argc, argv, nil, NSStringFromClass([AppDelegate class]));
    }
}

// Protocol abstracting the platform specific view in order to keep the Renderer class independent from platform
@protocol RenderDestinationProvider

@end

// Interface that controls the main updating/rendering loop on Metal appplications.
@interface MetalKitApplication : NSObject

-(nonnull instancetype)initWithMetalDevice:(nonnull id<MTLDevice>)device
                 renderDestinationProvider:(nonnull id<RenderDestinationProvider>)renderDestinationProvider
                                      view:(nonnull MTKView*)view;

- (void)drawRectResized:(CGSize)size;

- (void)update;

@end

// Our view controller.  Implements the MTKViewDelegate protocol, which allows it to accept
// per-frame update and drawable resize callbacks.  Also implements the RenderDestinationProvider
// protocol, which allows our renderer object to get and set drawable properties such as pixel
// format and sample count
@interface GameViewController : UIViewController<MTKViewDelegate, RenderDestinationProvider>

@end

/************************************************************************/
// GameViewController implementation
/************************************************************************/

@implementation GameViewController
{
    MTKView *_view;
    id<MTLDevice> _device;
    MetalKitApplication *_application;
}

- (void)viewDidLoad
{
    [super viewDidLoad];
    
    // Set the view to use the default device
    _device = MTLCreateSystemDefaultDevice();
    _view = (MTKView *)self.view;
    _view.delegate = self;
    _view.device = _device;
    
    // Get the device's width and height.
    gDeviceWidth = _view.drawableSize.width;
    gDeviceHeight = _view.drawableSize.height;
    gRetinaScale = _view.drawableSize.width / _view.frame.size.width;
    
    // Enable multi-touch in our apps.
    [_view setMultipleTouchEnabled:true];
    
    // Kick-off the MetalKitApplication.
    _application = [[MetalKitApplication alloc] initWithMetalDevice:_device renderDestinationProvider:self view:_view];
    
    if(!_device)
    {
        NSLog(@"Metal is not supported on this device");
        self.view = [[UIView alloc] initWithFrame:self.view.frame];
    }
}

// Called whenever view changes orientation or layout is changed
- (void)mtkView:(nonnull MTKView *)view drawableSizeWillChange:(CGSize)size
{
    [_application drawRectResized:view.bounds.size];
}

// Called whenever the view needs to render
- (void)drawInMTKView:(nonnull MTKView *)view
{
    @autoreleasepool
    {
        [_application update];
    }
}

- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(nullable UIEvent *)event {
    TouchEventData multiTouchData;
    multiTouchData.touchesRecorded = 0;
    for (UITouch *touch in touches)
    {
        CGPoint location = [touch locationInView:self.view];
        location.x *= gRetinaScale;
        location.y *= gRetinaScale;
        
        TouchData eventData;
        eventData.x = location.x;
        eventData.screenX = eventData.x / _view.drawableSize.width;
        eventData.y = location.y;
        eventData.screenY = eventData.y / _view.drawableSize.height;
        eventData.deltaX = 0;
        eventData.screenDeltaX = 0;
        eventData.deltaY = 0;
        eventData.screenDeltaY = 0;
        eventData.pressed = true;
        
        multiTouchData.touchData[multiTouchData.touchesRecorded++] = eventData;
    }
    
    PlatformEvents::onTouch(&multiTouchData);
}

- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(nullable UIEvent *)event {
    TouchEventData multiTouchData;
    multiTouchData.touchesRecorded = 0;
    for (UITouch *touch in touches)
    {
        CGPoint location = [touch locationInView:self.view];
        location.x *= gRetinaScale;
        location.y *= gRetinaScale;
        
        TouchData eventData;
        eventData.x = location.x;
        eventData.screenX = eventData.x / _view.drawableSize.width;
        eventData.y = location.y;
        eventData.screenY = eventData.y / _view.drawableSize.height;
        eventData.deltaX = 0;
        eventData.screenDeltaX = 0;
        eventData.deltaY = 0;
        eventData.screenDeltaY = 0;
        eventData.pressed = false;
        
        multiTouchData.touchData[multiTouchData.touchesRecorded++] = eventData;
    }
    
    PlatformEvents::onTouch(&multiTouchData);
}

- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(nullable UIEvent *)event {
    TouchEventData multiTouchData;
    multiTouchData.touchesRecorded = 0;
    for (UITouch *touch in touches)
    {
        CGPoint location = [touch locationInView:self.view];
        location.x *= gRetinaScale;
        location.y *= gRetinaScale;
        
        TouchData eventData;
        eventData.x = location.x;
        eventData.screenX = eventData.x / _view.drawableSize.width;
        eventData.y = location.y;
        eventData.screenY = eventData.y / _view.drawableSize.height;
        eventData.deltaX = 0;
        eventData.screenDeltaX = 0;
        eventData.deltaY = 0;
        eventData.screenDeltaY = 0;
        eventData.pressed = false;
        
        multiTouchData.touchData[multiTouchData.touchesRecorded++] = eventData;
    }
    
    PlatformEvents::onTouch(&multiTouchData);
}

- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(nullable UIEvent *)event {
    TouchEventData multiTouchData;
    multiTouchData.touchesRecorded = 0;
    for (UITouch *touch in touches)
    {
        CGPoint location = [touch locationInView:self.view];
        location.x *= gRetinaScale;
        location.y *= gRetinaScale;
        
        CGPoint prevLocation = [touch previousLocationInView:self.view];
        prevLocation.x *= gRetinaScale;
        prevLocation.y *= gRetinaScale;
        
        TouchData eventData;
        eventData.x = location.x;
        eventData.screenX = eventData.x / _view.drawableSize.width;
        eventData.y = location.y;
        eventData.screenY = eventData.y / _view.drawableSize.height;
        eventData.deltaX = prevLocation.x - location.x;
        eventData.screenDeltaX = eventData.deltaX / _view.drawableSize.width;
        eventData.deltaY = prevLocation.y - location.y;
        eventData.screenDeltaY = eventData.deltaY / _view.drawableSize.height;
        eventData.pressed = true;
		
        multiTouchData.touchData[multiTouchData.touchesRecorded++] = eventData;
    }
    
    PlatformEvents::onTouchMove(&multiTouchData);
}

@end

/************************************************************************/
// MetalKitApplication implementation
/************************************************************************/

// Timer used in the update function.
Timer deltaTimer;
IApp::Settings* pSettings;
uint32_t testingCurrentFrameCount;
uint32_t testingMaxFrameCount = 120;
bool automatedTesting = false;

// Metal application implementation.
@implementation MetalKitApplication{}
-(nonnull instancetype) initWithMetalDevice:(nonnull id<MTLDevice>)device
                  renderDestinationProvider:(nonnull id<RenderDestinationProvider>)renderDestinationProvider
                                       view:(nonnull MTKView*)view
{
    self = [super init];
    if (self)
    {
        
        NSArray *arguments = [[NSProcessInfo processInfo] arguments];
        
        if([arguments containsObject:@"--testing"])
        {
            automatedTesting = true;
        }
        
        
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
        
        @autoreleasepool {
            pApp->Init();
        }
    }
    
    return self;
}

-(void)drawRectResized:(CGSize)size
{
    pApp->mSettings.mWidth = size.width * gRetinaScale;
    pApp->mSettings.mHeight = size.height * gRetinaScale;
    pApp->mSettings.mFullScreen = true;
    pApp->Unload();
    pApp->Load();
}

-(void)update
{
    float deltaTime = deltaTimer.GetMSec(true) / 1000.0f;
    // if framerate appears to drop below about 6, assume we're at a breakpoint and simulate 20fps.
    if (deltaTime > 0.15f)
        deltaTime = 0.05f;
    
    pApp->Update(deltaTime);
    pApp->Draw();
    
    if(automatedTesting)
    {
        testingCurrentFrameCount++;
        if(testingCurrentFrameCount >= testingMaxFrameCount)
        {
            exit(0);
        }
    }
}
@end
/************************************************************************/
/************************************************************************/
#endif
