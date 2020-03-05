/*
 * Copyright (c) 2018-2020 The Forge Interactive Inc.
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

#import "macOSAppDelegate.h"
#define DISPATCH_APPROACH

@interface GameController: NSViewController
-(void)draw;
@end

@interface AppDelegate ()

@end

@implementation AppDelegate
{
    GameController *myController;
    void (^drawBlock)(void);
}

-(void) drawFunc
{
    [myController draw];
    drawBlock();
}

- (void)applicationDidFinishLaunching:(NSNotification*)aNotification
{
    myController = [GameController new];

#ifdef DISPATCH_APPROACH
    __weak AppDelegate *weakSelf = self;
     drawBlock = ^{
        dispatch_async( dispatch_get_main_queue(), ^{
            [weakSelf drawFunc];
        });
    };
    
    drawBlock();
#endif
    
#ifdef LOOP_IN_FINISH_LAUNCHING
    for (;;)
    {
        NSEvent *Event;
        @autoreleasepool
        {
            while ((Event = [NSApp nextEventMatchingMask: NSAnyEventMask untilDate: nil inMode: NSDefaultRunLoopMode dequeue: TRUE]))
                   [NSApp sendEvent: Event];
                   
            [myController draw];
        }
    }
    myController = nil;
#endif
    
}

- (void)applicationWillTerminate:(NSNotification*)aNotification
{
	// Insert code here to tear down your application
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)sender
{
	return YES;
}

@end
