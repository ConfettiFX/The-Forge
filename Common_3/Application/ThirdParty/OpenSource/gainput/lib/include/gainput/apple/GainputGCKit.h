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
#ifndef GAINPUTGCKIT_H_
#define GAINPUTGCKIT_H_

namespace gainput
{
    class InputManager;
	struct InputGestureConfig;
}

#ifdef GAINPUT_GC_HAPTICS
#import <CoreHaptics/CoreHaptics.h>
#import <GameController/GameController.h>
// Haptic motor interface
// Applies to both gamepad motors (if initWithController)
// Or applies to actual iphone device (if initWithIOSDevice)
// Device Vibration doesn't work with ipads
// controller vibration only available from ios 14.0+
// phone vibration only available from ios 13.0+
IOS14_API
@interface GainputGCHapticMotor : NSObject
-(void)cleanup;
-(bool)setIntensity:(float)intensity sharpness:(float)sharpness duration:(float)duration;
-(id _Nullable) initWithController:(GCController* _Nonnull)controller locality:(GCHapticsLocality _Nonnull )locality API_AVAILABLE(macos(11.0), ios(14.0));
-(id _Nullable) initWithIOSDevice;
@end
#endif
#endif

