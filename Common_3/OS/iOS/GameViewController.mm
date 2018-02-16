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

extern void updateTouchEvent(int numTaps);

extern WindowsDesc* getCurrentWindow();

namespace PlatformEvents
{
    extern void onTouch(const TouchEventData* pData);
    extern void onTouchMove(const TouchMoveEventData* pData);
}

@implementation GameViewController
{
    MTKView *_view;
    id<MTLDevice> _device;
    MetalKitApplication *_application;
    CGFloat _retinaScale;
}

- (void)viewDidLoad
{
    [super viewDidLoad];
    
    // Set the view to use the default device
    _device = MTLCreateSystemDefaultDevice();
    _view = (MTKView *)self.view;
    _view.delegate = self;
    _view.device = _device;
    
    // Adjust window size to match retina scaling.
    _retinaScale = _view.drawableSize.width / _view.frame.size.width;
    
    // Kick-off the MetalKitApplication.
    _application = [[MetalKitApplication alloc] initWithMetalDevice:_device renderDestinationProvider:self view:_view retinaScalingFactor:_retinaScale];
    
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

// Methods to get and set state of the our ultimate render destination (i.e. the drawable)
# pragma mark RenderDestinationProvider implementation

- (MTLRenderPassDescriptor*) currentRenderPassDescriptor
{
    return _view.currentRenderPassDescriptor;
}

- (MTLPixelFormat) colorPixelFormat
{
    return _view.colorPixelFormat;
}

- (void) setColorPixelFormat: (MTLPixelFormat) pixelFormat
{
    _view.colorPixelFormat = pixelFormat;
}

- (MTLPixelFormat) depthStencilPixelFormat
{
    return _view.depthStencilPixelFormat;
}

- (void) setDepthStencilPixelFormat: (MTLPixelFormat) pixelFormat
{
    _view.depthStencilPixelFormat = pixelFormat;
}

- (NSUInteger) sampleCount
{
    return _view.sampleCount;
}

- (void) setSampleCount:(NSUInteger) sampleCount
{
    _view.sampleCount = sampleCount;
}

- (id<MTLDrawable>) currentDrawable
{
    return _view.currentDrawable;
}

- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(nullable UIEvent *)event {
    for (UITouch *touch in touches)
    {
        CGPoint location = [touch locationInView:self.view];
        location.x *= _retinaScale;
        location.y *= _retinaScale;
        
        TouchEventData eventData;
        eventData.x = location.x;
        eventData.y = location.y;
        eventData.radius = (int32_t)(touch.majorRadius - touch.majorRadiusTolerance);
        eventData.pressed = true;
        
        PlatformEvents::onTouch(&eventData);
    }
}

- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(nullable UIEvent *)event {
    for (UITouch *touch in touches)
    {
        CGPoint location = [touch locationInView:self.view];
        location.x *= _retinaScale;
        location.y *= _retinaScale;
        
        TouchEventData eventData;
        eventData.x = location.x;
        eventData.y = location.y;
        eventData.radius = (int32_t)(touch.majorRadius - touch.majorRadiusTolerance);
        eventData.pressed = false;
        
        PlatformEvents::onTouch(&eventData);
    }
}

- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(nullable UIEvent *)event {
    for (UITouch *touch in touches)
    {
        CGPoint location = [touch locationInView:self.view];
        location.x *= _retinaScale;
        location.y *= _retinaScale;
        
        TouchEventData eventData;
        eventData.x = location.x;
        eventData.y = location.y;
        eventData.radius = (int32_t)(touch.majorRadius - touch.majorRadiusTolerance);
        eventData.pressed = false;
        
        PlatformEvents::onTouch(&eventData);
    }
}

- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(nullable UIEvent *)event {
    for (UITouch *touch in touches)
    {
        CGPoint location = [touch locationInView:self.view];
        location.x *= _retinaScale;
        location.y *= _retinaScale;
        
        CGPoint prevLocation = [touch previousLocationInView:self.view];
        prevLocation.y = self.view.bounds.size.height - prevLocation.y;
        prevLocation.x *= _retinaScale;
        prevLocation.y *= _retinaScale;
        
        TouchMoveEventData eventData;
        eventData.x = location.x;
        eventData.y = location.y;
        eventData.deltaX = prevLocation.x - location.x;
        eventData.deltaY = prevLocation.y - location.y;
        eventData.radius = (int32_t)(touch.majorRadius - touch.majorRadiusTolerance);
        
        PlatformEvents::onTouchMove(&eventData);
    }
}

@end


