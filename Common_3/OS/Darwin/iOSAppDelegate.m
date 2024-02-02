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

#import "iOSAppDelegate.h"

#include "../Interfaces/IOperatingSystem.h"

#include "../../Graphics/GraphicsConfig.h"

@interface GameController: NSObject
- (void)draw;
- (void)onFocusChanged:(BOOL)focused;
- (void)onActiveChanged:(BOOL)active;
@end

@interface AppDelegate ()
{
    GameController* myController;
}
@end

@implementation AppDelegate

- (id)init
{
    if (self = [super init])
    {
        self.displayLinkRefreshRate = -1;
        self.window = nil;
    }
    return self;
}

- (void)drawFunc:(CADisplayLink*)sender
{
    @autoreleasepool
    {
        [myController draw];
    }
}

- (void)InitializeDisplayLinkRefreshRate:(NSInteger)initialRefreshRate
{
    self.displayLinkRefreshRate = initialRefreshRate;
}

- (void)AddDisplayLink
{
    // in case it's been setup already
    [self RemoveDisplayLink];

    NSInteger framerateTarget = [[UIScreen mainScreen] maximumFramesPerSecond];

    if (self.displayLinkRefreshRate > 0 && self.displayLinkRefreshRate <= framerateTarget)
        framerateTarget = self.displayLinkRefreshRate;

    self.displayLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(drawFunc:)];

#if defined(ENABLE_PREFERRED_FRAME_RATE_RANGE)
    if (IOS17_RUNTIME)
    {
        self.displayLink.preferredFrameRateRange = CAFrameRateRangeMake(1, framerateTarget, framerateTarget);
    }
    else
#endif
    {
        [self.displayLink setPreferredFramesPerSecond:framerateTarget];
    }
    self.displayLink.paused = false;
    // add the display link to the run loop (will be called 60 times per second)
    [self.displayLink addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSDefaultRunLoopMode];
}

- (void)RemoveDisplayLink
{
    if (self.displayLink)
    {
        [self.displayLink invalidate];
        self.displayLink = nil;
    }
}

- (BOOL)application:(UIApplication*)application didFinishLaunchingWithOptions:(NSDictionary*)launchOptions
{
    myController = [GameController new];
    [self AddDisplayLink];
    return YES;
}

- (void)applicationWillResignActive:(UIApplication*)application
{
    // Sent when the application is about to move from active to inactive state. This can occur for certain types of temporary interruptions
    // (such as an incoming phone call or SMS message) or when the user quits the application and it begins the transition to the background
    // state. Use this method to pause ongoing tasks, disable timers, and invalidate graphics rendering callbacks. Games should use this
    // method to pause the game.
    [myController onActiveChanged:FALSE];
    self.displayLink.paused = true;
}

- (void)applicationDidEnterBackground:(UIApplication*)application
{
    [myController onFocusChanged:FALSE];
    self.displayLink.paused = true;
}

- (void)applicationWillEnterForeground:(UIApplication*)application
{
    self.displayLink.paused = false;
    [myController onFocusChanged:TRUE];
    [self AddDisplayLink];
}

- (void)applicationDidBecomeActive:(UIApplication*)application
{
    // Restart any tasks that were paused (or not yet started) while the application was inactive. If the application was previously in the
    // background, optionally refresh the user interface.
    [myController onActiveChanged:TRUE];
    [self AddDisplayLink];
}

- (void)applicationWillTerminate:(UIApplication*)application
{
    // Called when the application is about to terminate. Save data if appropriate. See also applicationDidEnterBackground:.
    [self RemoveDisplayLink];
    self.window = nil;
    myController = nil;
}

@end
