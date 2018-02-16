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

#include <ctime>

#include "../../ThirdParty/OpenSource/TinySTL/vector.h"

#include "../Interfaces/IOperatingSystem.h"
#include "../Interfaces/IPlatformEvents.h"
#include "../Interfaces/ILogManager.h"
#include "../Interfaces/ITimeManager.h"
#include "../Interfaces/IThread.h"
#include "../Interfaces/IMemoryManager.h"

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

static bool gAppRunning;
static WindowsDesc* gCurrentWindow = nullptr;
static tinystl::vector <MonitorDesc> gMonitors;
static KeyState gKeys[MAX_KEYS] = { { false, false, false, false } };

namespace PlatformEvents
{
    extern bool wantsMouseCapture;
    extern bool skipMouseCapture;
    
	extern void onWindowResize(const WindowResizeEventData* pData);
	extern void onKeyboardChar(const KeyboardCharEventData* pData);
	extern void onKeyboardButton(const KeyboardButtonEventData* pData);
	extern void onMouseMove(const MouseMoveEventData* pData);
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

// TODO: Add mouse capturing functions.
// TODO: Add monitor handling functionality (monitorCallback, collectMonitorInfo).

bool isRunning()
{
	return gAppRunning;
}

void requestShutDown()
{
    gAppRunning = false;
}

void openWindow(const char* app_name, WindowsDesc* winDesc)
{
    // TODO: Support opening multiple windows at the same time.
    gCurrentWindow = winDesc;
}

// Function needed to access the current window descriptor from the GameViewController.
WindowsDesc* getCurrentWindow()
{
    return gCurrentWindow;
}

void closeWindow(const WindowsDesc* winDesc)
{
    // TODO: Implement.
    ASSERT(0);
}

// Andrés: Message handling is the NSView's or UIView's responsibility.
//void handleMessages() {}

void setWindowRect(WindowsDesc* winDesc, const RectDesc& rect)
{
    // TODO: Implement.
    ASSERT(0);
}

void setWindowSize(WindowsDesc* winDesc, unsigned width, unsigned height)
{
    // TODO: Implement.
    ASSERT(0);
}

void toggleFullscreen(WindowsDesc* winDesc)
{
    // TODO: Implement.
    ASSERT(0);
}

void maximizeWindow(WindowsDesc* winDesc)
{
    // TODO: Implement.
    ASSERT(0);
}

void minimizeWindow(WindowsDesc* winDesc)
{
    // TODO: Implement.
    ASSERT(0);
}

void getRecommendedResolution(RectDesc* rect)
{
    *rect = RectDesc{ 0,0,1920,1080 };
}

void setResolution(const MonitorDesc* pMonitor, const Resolution* pRes)
{
    
}

float2 getMousePosition()
{
    // TODO: Implement.
    ASSERT(0);
    return float2(1.0, 1.0);
}

void setMousePositionRelative(const WindowsDesc* winDesc, int32_t x, int32_t y)
{
    CGPoint location;
    location.x = winDesc->windowedRect.left + x;
    location.y = winDesc->windowedRect.bottom - y;
    CGWarpMouseCursorPosition(location);
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

int macOSMain(int argc, char** argv, IApp* app)
{
	pApp = app;
	return NSApplicationMain(argc, argv);
}

#import "MetalKitApplication.h"

// Timer used in the update function.
Timer deltaTimer;
float retinaScale = 1.0f;

// Metal application implementation.
@implementation MetalKitApplication{}
-(nonnull instancetype) initWithMetalDevice:(nonnull id<MTLDevice>)device
renderDestinationProvider : (nonnull id<RenderDestinationProvider>)renderDestinationProvider
	view : (nonnull MTKView*)view
	retinaScalingFactor : (CGFloat)retinaScalingFactor
{
	self = [super init];
	if (self)
	{
		FileSystem::SetCurrentDir(FileSystem::GetProgramDir());

		retinaScale = retinaScalingFactor;

		RectDesc resolution;
		getRecommendedResolution(&resolution);

		WindowsDesc gWindow = {};
		gWindow.windowedRect = resolution;
		gWindow.fullscreenRect = resolution;
		gWindow.fullScreen = false;
		gWindow.maximized = false;
		gWindow.handle = (void*)CFBridgingRetain(view);

		@autoreleasepool {
			const char * appName = "01_Transformations";
			openWindow(appName, &gWindow);

			pApp->pWindow = &gWindow;
			pApp->mSettings.mWidth = getRectWidth(resolution);
			pApp->mSettings.mWidth = getRectHeight(resolution);
			pApp->mSettings.mFullScreen = false;
			pApp->Init();
		}
	}

	return self;
}

-(void)drawRectResized:(CGSize)size
{
	pApp->mSettings.mWidth = size.width * retinaScale;
	pApp->mSettings.mHeight = size.height * retinaScale;
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
}
@end
/************************************************************************/
/************************************************************************/
#endif
