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
#ifndef GAINPUTIOS_H_
#define GAINPUTIOS_H_

#import <UIKit/UIKit.h>

namespace gainput
{
    class InputManager;
	struct InputGestureConfig;
}

/// [IOS ONLY] UIKit view that captures keyboard inputs.
/**
 * In order to enable keyboard input on iOS devices (i.e. make the InputDeviceTouch work),
 * an instance of this view has to be attached as a subview to GainputView below.
 * TODO:  CUstomization, Modifier keys
 * TODO: Behave as regular keyboard with full events
 */
@interface KeyboardView : UITextView<UIKeyInput>
- (id _Nonnull) initWithFrame:(CGRect)frame inputManager:(gainput::InputManager&)inputManager;
@property(nonatomic, readonly) BOOL hasText;
@end


/// [IOS ONLY] UIKit view that captures touch inputs.
/**
 * In order to enable touch input on iOS devices (i.e. make the InputDeviceTouch work),
 * an instance of this view has to be attached as a subview to the view of your application.
 * Note that, for touches to work in portrait and landscape mode, the subview has to be
 * rotated correctly with its parent.
 */
@interface GainputView : UIView
@property (nullable, nonatomic, strong) KeyboardView * pKeyboardView;
- (id _Nonnull)initWithFrame:(CGRect)frame inputManager:(gainput::InputManager&)inputManager;
- (BOOL)setVirtualKeyboard:(int)inputType;
-(void) addGestureMapping:
(unsigned)gestureId
withConfig:(gainput::GestureConfig&)gestureConfig;
+ (bool)setPhoneHaptics:(float)intensity sharpness:(float)sharpness seconds:(float)duration;
@end

#endif

