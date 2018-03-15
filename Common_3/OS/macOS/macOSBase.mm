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
#include "../Interfaces/IMemoryManager.h"
#include "../Interfaces/IFileSystem.h"
#include "../Interfaces/IApp.h"

#define CONFETTI_WINDOW_CLASS L"confetti"
#define MAX_KEYS 256
#define MAX_CURSOR_DELTA 200

#define elementsOf(a) (sizeof(a) / sizeof((a)[0]))

namespace
{
    bool isCaptured = false;
}

struct KeyState
{
    bool current;   // What is the current key state?
    bool previous;  // What is the previous key state?
    bool down;      // Was the key down this frame?
    bool released;  // Was the key released this frame?
};

static KeyState gKeys[MAX_KEYS] = { { false, false, false, false } };

static bool gAppRunning;
static WindowsDesc gCurrentWindow;
static float gRetinaScale = 1.0f;

// TODO: Add multiple window/monitor handling functionality to macOS.
//static tinystl::vector <MonitorDesc> gMonitors;
//static tinystl::unordered_map<void*, WindowsDesc*> gWindowMap;

void adjustWindow(WindowsDesc* winDesc);

namespace PlatformEvents
{
    extern bool wantsMouseCapture;
    extern bool skipMouseCapture;
    
	extern void onWindowResize(const WindowResizeEventData* pData);
	extern void onKeyboardChar(const KeyboardCharEventData* pData);
	extern void onKeyboardButton(const KeyboardButtonEventData* pData);
	extern void onMouseMove(const MouseMoveEventData* pData);
	extern void onRawMouseMove(const RawMouseMoveEventData* pData);
	extern void onMouseButton(const MouseButtonEventData* pData);
	extern void onMouseWheel(const MouseWheelEventData* pData);
}

// Update the state of the keys based on state previous frame
void updateKeys(void)
{
	// Calculate each of the key states here
	for (KeyState& element : gKeys)
	{
		element.down = element.current == true;
		element.released = ((element.previous == true) && (element.current == false));
		// Record this state
		element.previous = element.current;
	}
}

// Update the given key
void updateKeyArray(bool pressed, unsigned short keyCode)
{
    KeyboardButtonEventData eventData;
    eventData.key = keyCode;
    eventData.pressed = pressed;
    
    if ((0 <= keyCode) && (keyCode <= MAX_KEYS))
        gKeys[keyCode].current = pressed;
    
    PlatformEvents::onKeyboardButton(&eventData);
}

static bool captureMouse(bool shouldCapture)
{
    if (shouldCapture != isCaptured)
    {
        if (shouldCapture)
        {
            CGDisplayHideCursor(kCGDirectMainDisplay);
            CGAssociateMouseAndMouseCursorPosition(false);
            isCaptured = true;
        }
        else
        {
            CGDisplayShowCursor(kCGDirectMainDisplay);
            CGAssociateMouseAndMouseCursorPosition(true);
            isCaptured = false;
        }
    }
    
    return true;
}

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

bool isRunning()
{
	return gAppRunning;
}

void getRecommendedResolution(RectDesc* rect)
{
    *rect = RectDesc{ 0, 0, 1920, 1080 };
}

void requestShutDown()
{
    gAppRunning = false;
}

// TODO: Add multiple window handling functionality.

void openWindow(const char* app_name, WindowsDesc* winDesc)
{
    
}

void closeWindow(const WindowsDesc* winDesc)
{
    
}

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

void setWindowSize(WindowsDesc* winDesc, unsigned width, unsigned height)
{
    setWindowRect(winDesc, { 0, 0, (int)width, (int)height });
}

void toggleFullscreen(WindowsDesc* winDesc)
{
    winDesc->fullScreen = !winDesc->fullScreen;
    MTKView* view = (MTKView*)CFBridgingRelease(winDesc->handle);
    [view.window toggleFullScreen:nil];
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
    return float2(mouseLoc.x, mouseLoc.y);
}

bool getKeyDown(int key)
{
	return gKeys[key].down;
}

bool getKeyUp(int key)
{
	return gKeys[key].released;
}

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

int macOSMain(int argc, const char** argv, IApp* app)
{
    pApp = app;
    return NSApplicationMain(argc, argv);
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
@interface GameViewController : NSViewController<MTKViewDelegate, RenderDestinationProvider>

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

- (void) viewDidLoad
{
    [super viewDidLoad];
    
    // Set the view to use the default device
    _device = MTLCreateSystemDefaultDevice();
    _view = (MTKView *)self.view;
    _view.delegate = self;
    _view.device = _device;
    [_view.window makeFirstResponder:self];
    isCaptured = false;
    
    // Adjust window size to match retina scaling.
    gRetinaScale = _view.drawableSize.width / _view.frame.size.width;
    NSSize windowSize = CGSizeMake(_view.frame.size.width / gRetinaScale, _view.frame.size.height / gRetinaScale);
    [_view.window setContentSize:windowSize];
    
    // Kick-off the MetalKitApplication.
    _application = [[MetalKitApplication alloc] initWithMetalDevice:_device renderDestinationProvider:self view:_view];
    
    // In order to get mouse move messages (without clicking on the window) we need to set a tracking area covering the whole view
    NSTrackingAreaOptions options = (NSTrackingActiveAlways | NSTrackingInVisibleRect |
                                     NSTrackingMouseEnteredAndExited | NSTrackingMouseMoved);
    NSTrackingArea *area = [[NSTrackingArea alloc] initWithRect:[_view bounds] options:options owner:self userInfo:nil];
    [_view addTrackingArea:area];
    
    if(!_device)
    {
        NSLog(@"Metal is not supported on this device");
        self.view = [[NSView alloc] initWithFrame:self.view.frame];
    }
}

- (BOOL)acceptsFirstResponder
{
    return TRUE;
}

- (BOOL)canBecomeKeyView
{
    return TRUE;
}

- (void)keyDown:(NSEvent *)event
{
    unsigned short keyCode = [event keyCode];
    updateKeyArray(true, keyCode);
}

- (void)keyUp:(NSEvent *)event
{
    unsigned short keyCode = [event keyCode];
    updateKeyArray(false, keyCode);
    
    // Handle special case: ESCAPE key
    if (keyCode == kVK_Escape)
    {
        if (!isCaptured)
        {
            [NSApp terminate:self];
        }
        else
        {
            captureMouse(false);
        }
    }
}

// This method is eneded to handle special keys: ctrl, shift, option...
- (void)flagsChanged:(NSEvent *)event
{
    unsigned short keyCode = [event keyCode];
    bool wasPressedBefore = getKeyDown(keyCode);
    updateKeyArray(!wasPressedBefore, keyCode);
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
        updateKeys();
    }
}

- (void)ReportMouseButtonEvent:(NSEvent*)event button:(MouseButton)button pressed:(bool)pressed
{
    // Translate the cursor position into view coordinates, accounting for the fact that
    // App Kit's default window coordinate space has its origin in the bottom left
    CGPoint location = [self.view convertPoint:[event locationInWindow] fromView:nil];
    location.y = self.view.bounds.size.height - location.y;
    
    // Multiply the mouse coordinates by the retina scale factor.
    location.x *= gRetinaScale;
    location.y *= gRetinaScale;
    
    MouseButtonEventData eventData;
    eventData.button = button;
    eventData.pressed = pressed;
    eventData.x = location.x;
    eventData.y = location.y;
    
    PlatformEvents::onMouseButton(&eventData);
}

- (void)mouseDown:(NSEvent *)event {
    [self ReportMouseButtonEvent: event
                          button: MOUSE_LEFT
                         pressed: true];
    if (PlatformEvents::wantsMouseCapture && !PlatformEvents::skipMouseCapture && !isCaptured)
    {
        captureMouse(true);
    }
}

- (void)mouseUp:(NSEvent *)event {
    [self ReportMouseButtonEvent: event
                          button: MOUSE_LEFT
                         pressed: false];
}

- (void)rightMouseDown:(NSEvent *)event {
    [self ReportMouseButtonEvent: event
                          button: MOUSE_RIGHT
                         pressed: true];
}

- (void)rightMouseUp:(NSEvent *)event {
    [self ReportMouseButtonEvent: event
                          button: MOUSE_RIGHT
                         pressed: false];
}

- (void)ReportMouseMove:(NSEvent *)event
{
    // Translate the cursor position into view coordinates, accounting for the fact that
    // App Kit's default window coordinate space has its origin in the bottom left
    CGPoint location = [self.view convertPoint:[event locationInWindow] fromView:nil];
    location.y = self.view.bounds.size.height - location.y;
    
    // Multiply the mouse coordinates by the retina scale factor.
    location.x *= gRetinaScale;
    location.y *= gRetinaScale;
    
    MouseMoveEventData eventData;
    eventData.x = location.x;
    eventData.y = location.y;
    eventData.deltaX = [event deltaX];
    eventData.deltaY = [event deltaY];
    eventData.captured = isCaptured;
	
	// TODO: Collect raw mouse data in macOS
	RawMouseMoveEventData rawMouseEventData;
	rawMouseEventData.x = [event deltaX];
	rawMouseEventData.y = [event deltaY];
	rawMouseEventData.captured = isCaptured;
	PlatformEvents::onRawMouseMove(&rawMouseEventData);
	
    PlatformEvents::onMouseMove(&eventData);
}

- (void)mouseMoved:(NSEvent *)event {
    [self ReportMouseMove:event];
}

- (void)mouseDragged:(NSEvent *)event {
    [self ReportMouseMove:event];
}

- (void)rightMouseDragged:(NSEvent *)event {
    [self ReportMouseMove:event];
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
        }

		gCurrentWindow = {};
		gCurrentWindow.windowedRect = { 0, 0, (int)pSettings->mWidth, (int)pSettings->mHeight };
		gCurrentWindow.fullScreen = pSettings->mFullScreen;
		gCurrentWindow.maximized = false;
		gCurrentWindow.handle = (void*)CFBridgingRetain(view);
        openWindow(pApp->GetName(), &gCurrentWindow);
        
        pSettings->mWidth = gCurrentWindow.fullScreen ? getRectWidth(gCurrentWindow.fullscreenRect) : getRectWidth(gCurrentWindow.windowedRect);
        pSettings->mHeight = gCurrentWindow.fullScreen ? getRectHeight(gCurrentWindow.fullscreenRect) : getRectHeight(gCurrentWindow.windowedRect);
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
	// TODO: Fullscreen
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
            for (NSWindow *window in [NSApplication sharedApplication].windows) {
                [window close];
            }
            
            [NSApp terminate:nil];
        }
    }
}
@end
/************************************************************************/
/************************************************************************/
#endif
