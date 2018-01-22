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

#import "GameViewController.h"
#import "../Interfaces/IOperatingSystem.h"

extern void updateKeyArray(bool pressed, unsigned short keyCode);
extern void updateKeys(void);

extern WindowsDesc* getCurrentWindow();

namespace PlatformEvents
{
    extern bool wantsMouseCapture;
    extern bool skipMouseCapture;
    extern void onMouseMove(const MouseMoveEventData* pData);
    extern void onMouseButton(const MouseButtonEventData* pData);
}

@implementation GameViewController
{
    MTKView *_view;
    id<MTLDevice> _device;
    MetalKitApplication *_application;
    CGFloat _retinaScale;
    bool _isCaptured;
}

- (void) viewDidLoad
{
    [super viewDidLoad];

    // Set the view to use the default device
    _device = MTLCreateSystemDefaultDevice();
    _view = (MTKView *)self.view;
    _view.delegate = self;
    _view.device = _device;
    _isCaptured = false;
    [_view.window makeFirstResponder:self];
    
    // Adjust window size to match retina scaling.
    _retinaScale = _view.drawableSize.width / _view.frame.size.width;
    NSSize windowSize = CGSizeMake(_view.frame.size.width / _retinaScale, _view.frame.size.height / _retinaScale);
    [_view.window setContentSize:windowSize];

    // Kick-off the MetalKitApplication.
    _application = [[MetalKitApplication alloc] initWithMetalDevice:_device renderDestinationProvider:self view:_view retinaScalingFactor:_retinaScale];

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
        if (!_isCaptured)
        {
            [NSApp terminate:self];
        }
        else
        {
            [self CaptureMouse:false];
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
    location.x *= _retinaScale;
    location.y *= _retinaScale;
    
    MouseButtonEventData eventData;
    eventData.button = button;
    eventData.pressed = pressed;
    eventData.x = location.x;
    eventData.y = location.y;
    eventData.pWindow = *getCurrentWindow();
    
    PlatformEvents::onMouseButton(&eventData);
}

// Handle capture/release cursor
- (void)CaptureMouse:(bool)shouldCapture
{
    if (shouldCapture != _isCaptured)
    {
        if (shouldCapture)
        {
            CGDisplayHideCursor(kCGDirectMainDisplay);
            CGAssociateMouseAndMouseCursorPosition(false);
            _isCaptured = true;
        }
        else
        {
            CGDisplayShowCursor(kCGDirectMainDisplay);
            CGAssociateMouseAndMouseCursorPosition(true);
            _isCaptured = false;
        }
    }
}

- (void)mouseDown:(NSEvent *)event {
    [self ReportMouseButtonEvent: event
                          button: MOUSE_LEFT
                         pressed: true];
    if (PlatformEvents::wantsMouseCapture && !PlatformEvents::skipMouseCapture && !_isCaptured)
    {
        [self CaptureMouse: true];
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
    location.x *= _retinaScale;
    location.y *= _retinaScale;
    
    MouseMoveEventData eventData;
    eventData.x = location.x;
    eventData.y = location.y;
    eventData.deltaX = [event deltaX];
    eventData.deltaY = [event deltaY];
    eventData.pWindow = *getCurrentWindow();
    eventData.captured = _isCaptured;
    
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
