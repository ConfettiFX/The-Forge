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
#include "../../../include/gainput/gainput.h"

#if defined(GAINPUT_PLATFORM_IOS) || defined(GAINPUT_PLATFORM_MAC)

#include "../pad/GainputInputDevicePadImpl.h"
#include "../../../include/gainput/GainputInputDeltaState.h"
#include "../../../include/gainput/GainputHelpers.h"
#include "../../../include/gainput/GainputLog.h"

#include "GainputInputDevicePadAppleGCKit.h"
#include "../../../include/gainput/apple/GainputGCKit.h"

#import <GameKit/GameKit.h>

#if defined(GAINPUT_PLATFORM_MAC)
#import <CoreFoundation/CoreFoundation.h>
#else
#include "../../../include/gainput/apple/GainputIos.h"
#endif

namespace gainput
{
InputDevicePadImplGCKit::GlobalControllerList* InputDevicePadImplGCKit::mappedControllers_;

namespace
{
static GCController* getGcController(void* c)
{
    return static_cast<GCController*>(c);
}

static bool IsValidHardwareController(GCController const * controller)
{
    if (controller == NULL) return false;
    
    NSArray* controllers = [GCController controllers];
    const std::size_t controllerCount = [controllers count];
    
    for (std::size_t i = 0; i < controllerCount; ++i)
    {
        GCController * currentController = controllers[i];
        
        if (currentController == controller)
        {
            return true;
        }
    }
    return false;
}

// Removes all invalid controller mappings (controllers that are not available anymore)
static void CleanDisconnectedControllersFromMapping()
{
    if (!InputDevicePadImplGCKit::mappedControllers_)
    {
        return;
    }
	InputDevicePadImplGCKit::GlobalControllerList& mapping = *InputDevicePadImplGCKit::mappedControllers_;
    for (InputDevicePadImplGCKit::GlobalControllerList::iterator it = mapping.begin(); it != mapping.end(); )
    {
        GCController const* controller = static_cast<GCController*>(*it);
        if (!IsValidHardwareController(controller))
        {
            it = mapping.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

// Returns true if this controller is already mapped to a gainput pad, otherwise false
static bool IsMapped(GCController* controller)
{
    if (0 == controller
        || !InputDevicePadImplGCKit::mappedControllers_)
    {
        return false;
    }
	InputDevicePadImplGCKit::GlobalControllerList& mapping = *InputDevicePadImplGCKit::mappedControllers_;
    for(std::size_t index = 0; index < mapping.size(); ++index)
    {
        GCController const* mapped_controller = static_cast<GCController*>(mapping[index]);
        if (controller == mapped_controller)
        {
            return true;
        }
    }
    return false;
}

/// Returns the first hardware controller that is not yet mapped
static GCController* GetFirstNonMappedController()
{
    NSArray* controllers = [GCController controllers];
    const std::size_t controllerCount = [controllers count];
    for (std::size_t i = 0; i < controllerCount; ++i)
    {
        GCController* currentController = controllers[i];
        if (!IsMapped(currentController))
        {
            return currentController;
        }
    }
    return 0;
}

}

// check if the device itself supports haptics
bool SupportsHapticEngine()
{
#if defined(GAINPUT_GC_HAPTICS)
	id<CHHapticDeviceCapability> capabilities = [CHHapticEngine capabilitiesForHardware];
	return capabilities.supportsHaptics;
#endif
    return false;
}

InputDevicePadImplGCKit::InputDevicePadImplGCKit(InputManager& manager, InputDevice& device, unsigned index, InputState& state, InputState& previousState) :
    manager_(manager),
    device_(device),
    index_(index),
    state_(state),
    previousState_(previousState),
    deviceState_(InputDevice::DS_UNAVAILABLE),
    deltaState_(NULL),
	deviceChangeCb(NULL),
    isMicro_(false),
    isNormal_(false),
    isExtended_(false),
    supportsMotion_(false),
    isRemote_(false),
    gcController_(NULL),
	controllerName_("")
#ifdef GAINPUT_GC_HAPTICS
	,hapticMotorLeft(NULL)
	,hapticMotorRight(NULL)
#endif
{
    (void)&this->manager_;
    (void)&this->previousState_;
    device_.SetDeadZone(PadButtonGyroscopeX, 0.1);
    device_.SetDeadZone(PadButtonGyroscopeY, 0.1);
    device_.SetDeadZone(PadButtonGyroscopeZ, 0.1);

    if (!mappedControllers_)
    {
        mappedControllers_ = manager.GetAllocator().New<GlobalControllerList>(manager.GetAllocator());
    }

	__block InputDevicePadImplGCKit*devPad = this;
	deviceConnectedObserver = [[NSNotificationCenter defaultCenter]
									addObserverForName:GCControllerDidConnectNotification
									object:nil
									queue: [NSOperationQueue mainQueue]
									usingBlock:^(NSNotification *note)
									{
										devPad->SetupController();
									}
								];

	deviceDisconnectedObserver= [[NSNotificationCenter defaultCenter]
									addObserverForName:GCControllerDidDisconnectNotification
									object:nil
									queue: [NSOperationQueue mainQueue]
									usingBlock:^(NSNotification *note)
									{
										if(note.object)
										{
											if(note.object == gcController_)
											{
												devPad->deviceState_ = InputDevice::DS_UNAVAILABLE;
												devPad->gcController_ = NULL;
												devPad->manager_.RemoveDevice(devPad->device_.GetDeviceId());
												devPad->controllerName_ = "";
												devPad->device_.OverrideDeviceId(-1);
#ifdef GAINPUT_GC_HAPTICS
												GainputGCHapticMotor* leftMotorDevice = (GainputGCHapticMotor*)devPad->hapticMotorLeft;
												GainputGCHapticMotor* rightMotorDevice = (GainputGCHapticMotor*)devPad->hapticMotorRight;
												if(leftMotorDevice)
													[leftMotorDevice cleanup];
												if(rightMotorDevice)
													[rightMotorDevice cleanup];
												devPad->hapticMotorLeft = nil;
												devPad->hapticMotorRight = nil;
#endif
											}
										}
									}
								];
}


void InputDevicePadImplGCKit::SetOnDeviceChangeCallBack(void(*onDeviceChange)(const char*, bool added, int controllerId))
{
    deviceChangeCb = onDeviceChange;
    if(deviceState_ == InputDevice::DS_OK)
    {
        if(deviceChangeCb)
            (*deviceChangeCb)(controllerName_, true, index_);
    }
}

InputDevicePadImplGCKit::~InputDevicePadImplGCKit()
{
	if (mappedControllers_)
	{
		manager_.GetAllocator().Delete(mappedControllers_);
		mappedControllers_ = NULL;
	}
	id<NSObject> connectObserver =(id<NSObject>)deviceConnectedObserver;
	[[NSNotificationCenter defaultCenter] removeObserver:connectObserver];

	id<NSObject> disconnectObserver = (id<NSObject>)deviceDisconnectedObserver;
	[[NSNotificationCenter defaultCenter] removeObserver:disconnectObserver];
}


const char* InputDevicePadImplGCKit::GetDeviceName()
{
	// just store and re-use controller name when registering the controller
	// in case we lose connection
	return controllerName_;
}


bool InputDevicePadImplGCKit::SetRumbleEffect(float leftMotorIntensity, float rightMotorIntensity, uint32_t duration_ms, bool targetOwningDevice)
{
#if defined(GAINPUT_GC_HAPTICS)
	if(deviceState_ != InputDevice::DS_OK && targetOwningDevice == false)
		return false;

	float durationSec = (float)duration_ms / 1000.0f;

#ifdef GAINPUT_PLATFORM_IOS
	if(targetOwningDevice)
	{
		// swap values if left motor is 0
		// otherwise there's no rumble at all.
		if(leftMotorIntensity == 0.0f && rightMotorIntensity > 0.0f)
		{
			leftMotorIntensity = rightMotorIntensity;
			rightMotorIntensity = 0.0f;
		}

		//use max intensity since we only have a single motor
		[GainputView setPhoneHaptics:leftMotorIntensity sharpness:rightMotorIntensity seconds:durationSec];
	}
	else if (!targetOwningDevice)
#endif
	{
		if(hapticMotorLeft)
		{
			GainputGCHapticMotor* leftMotorDevice = (GainputGCHapticMotor*)hapticMotorLeft;
			[leftMotorDevice setIntensity:leftMotorIntensity sharpness:1.0f duration:durationSec];
		}

		if(hapticMotorRight)
		{
			GainputGCHapticMotor* RightMotorDevice = (GainputGCHapticMotor*)hapticMotorRight;
			[RightMotorDevice setIntensity:rightMotorIntensity sharpness:1.0f duration:durationSec];
		}
	}
	
	return true;
#endif

	return false;
}

void InputDevicePadImplGCKit::SetupController()
{

	GCController* newIncController = GetFirstNonMappedController();

	if(!newIncController || newIncController.isSnapshot)
	{
		return;
	}
	if(!gcController_)
	{
		gcController_ = (void*)newIncController;
	}
	GCController* controller = getGcController(gcController_);
	// check if we need to replace current controller with this one.
	if (controller != newIncController)
	{
		// our current controller is valid
		if(controller.isAttachedToDevice)
		{
			return;
		}

		CleanDisconnectedControllersFromMapping();
		gcController_ = (void*) newIncController;
		controller = newIncController;
	}

	GCControllerPlayerIndex newIndex = static_cast<GCControllerPlayerIndex>(index_);
	if (controller.playerIndex != newIndex)
	{
		controller.playerIndex = newIndex;
	}
	isRemote_ = false;
	isExtended_ = [controller extendedGamepad] != 0;
	isMicro_ = false;
	isNormal_ = [controller extendedGamepad] != 0;
	supportsMotion_ = [controller motion] != 0;
	if (!isRemote_)
	{
		UpdateGamepad_();
	}
	
	//more accurate name starting from 10.15 (i.e: DualShock, DualSense, Xbox, Switch Pro ...)
	controllerName_  = [controller.productCategory cStringUsingEncoding:NSASCIIStringEncoding];

#if defined(GAINPUT_GC_HAPTICS)
	if([controller haptics] != nil)
	{
		if(hapticMotorLeft != nil)
		{
			[(GainputGCHapticMotor*)hapticMotorLeft cleanup];
			hapticMotorLeft = nil;
		}
		if(hapticMotorRight != nil)
		{
			[(GainputGCHapticMotor*)hapticMotorRight cleanup];
			hapticMotorRight = nil;
		}
		hapticMotorLeft = [[GainputGCHapticMotor alloc] initWithController:controller locality:(GCHapticsLocalityLeftHandle)];
		hapticMotorRight = [[GainputGCHapticMotor alloc] initWithController:controller locality:(GCHapticsLocalityRightHandle)];
	}
#endif

	deviceState_ = InputDevice::DS_OK;
	mappedControllers_->push_back(gcController_);

	DeviceId newId = manager_.GetNextId();
	device_.OverrideDeviceId(newId);
	manager_.AddDevice(newId, &device_);
}

void InputDevicePadImplGCKit::Update(InputDeltaState* delta)
{
	if (!gcController_)
	{
		deviceState_ = InputDevice::DS_UNAVAILABLE;
		isExtended_ = false;
		supportsMotion_ = false;
		return;
	}

	deltaState_ = delta;
}

void InputDevicePadImplGCKit::UpdateGamepad_()
{
	GCController* controller = getGcController(gcController_);

	if (isExtended_)
	{
		GCExtendedGamepad* gamepadExtended = [controller extendedGamepad];
		controller.extendedGamepad.leftTrigger.valueChangedHandler = ^(GCControllerButtonInput * _Nonnull button, float value, BOOL pressed)
		{
			HandleButton(device_, state_, deltaState_, PadButtonL2, pressed);
			HandleAxis(device_, state_, deltaState_, PadButtonAxis4, value);
		};
		controller.extendedGamepad.rightTrigger.valueChangedHandler = ^(GCControllerButtonInput * _Nonnull button, float value, BOOL pressed)
		{
			HandleButton(device_, state_, deltaState_, PadButtonR2, pressed);
			HandleAxis(device_, state_, deltaState_, PadButtonAxis5, value);
		};
		controller.extendedGamepad.leftShoulder.valueChangedHandler = ^(GCControllerButtonInput * _Nonnull button, float value, BOOL pressed)
		{
			HandleButton(device_, state_, deltaState_, PadButtonL1, pressed);
		};
		controller.extendedGamepad.rightShoulder.valueChangedHandler = ^(GCControllerButtonInput * _Nonnull button, float value, BOOL pressed)
		{
			HandleButton(device_, state_, deltaState_, PadButtonR1, pressed);
		};
		controller.extendedGamepad.buttonA.pressedChangedHandler = ^(GCControllerButtonInput * _Nonnull button, float value, BOOL pressed)
		{
			HandleButton(device_, state_, deltaState_, PadButtonA, pressed);
		};
		controller.extendedGamepad.buttonB.valueChangedHandler = ^(GCControllerButtonInput * _Nonnull button, float value, BOOL pressed)
		{
			HandleButton(device_, state_, deltaState_, PadButtonB, pressed);
		};
		controller.extendedGamepad.buttonX.valueChangedHandler = ^(GCControllerButtonInput * _Nonnull button, float value, BOOL pressed)
		{
			HandleButton(device_, state_, deltaState_, PadButtonX, pressed);
		};
		controller.extendedGamepad.buttonY.valueChangedHandler = ^(GCControllerButtonInput * _Nonnull button, float value, BOOL pressed)
		{
			HandleButton(device_, state_, deltaState_, PadButtonY, pressed);
		};

		controller.extendedGamepad.dpad.valueChangedHandler = ^(GCControllerDirectionPad * _Nonnull dpad, float xValue, float yValue)
		{
				HandleButton(device_, state_, deltaState_, PadButtonLeft, 	dpad.left.pressed);
				HandleButton(device_, state_, deltaState_, PadButtonRight, 	dpad.right.pressed);
				HandleButton(device_, state_, deltaState_, PadButtonUp, 	dpad.up.pressed);
				HandleButton(device_, state_, deltaState_, PadButtonDown, 	dpad.down.pressed);
		};

		gamepadExtended.leftThumbstickButton.valueChangedHandler = ^(GCControllerButtonInput * _Nonnull button, float value, BOOL pressed)
		{
			HandleButton(device_, state_, deltaState_, PadButtonL3, pressed);
		};
		gamepadExtended.rightThumbstickButton.valueChangedHandler = ^(GCControllerButtonInput * _Nonnull button, float value, BOOL pressed)
		{
			HandleButton(device_, state_, deltaState_, PadButtonR3, pressed);
		};

		gamepadExtended.buttonOptions.valueChangedHandler = ^(GCControllerButtonInput * _Nonnull button, float value, BOOL pressed)
		{
			HandleButton(device_, state_, deltaState_, PadButtonSelect, pressed);
		};
		gamepadExtended.buttonMenu.valueChangedHandler = ^(GCControllerButtonInput * _Nonnull button, float value, BOOL pressed)
		{
			HandleButton(device_, state_, deltaState_, PadButtonStart, pressed);
		};
		
#if defined(GAINPUT_PLATFORM_IOS) || (defined(MAC_OS_VERSION_11_0) && MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_VERSION_11_0)
		if (IOS14_RUNTIME)
		{
			gamepadExtended.buttonHome.valueChangedHandler = ^(GCControllerButtonInput * _Nonnull button, float value, BOOL pressed)
			{
				HandleButton(device_, state_, deltaState_, PadButtonHome, pressed);
			};

			if ([gamepadExtended isKindOfClass:[GCDualShockGamepad class]])
			{
				GCDualShockGamepad* dualShockGamepad = (GCDualShockGamepad*)gamepadExtended;

				dualShockGamepad.touchpadButton.valueChangedHandler = ^(GCControllerButtonInput * _Nonnull button, float value, BOOL pressed)
				{
					HandleButton(device_, state_, deltaState_, PadButton17, pressed);
				};

				dualShockGamepad.touchpadPrimary.valueChangedHandler = ^(GCControllerDirectionPad * _Nonnull dpad, float xValue, float yValue)
				{
					HandleAxis(device_, state_, deltaState_, PadButtonAxis6, xValue);
					HandleAxis(device_, state_, deltaState_, PadButtonAxis7, yValue);
				};

				dualShockGamepad.touchpadSecondary.valueChangedHandler = ^(GCControllerDirectionPad * _Nonnull dpad, float xValue, float yValue)
				{
					HandleAxis(device_, state_, deltaState_, PadButtonAxis8, xValue);
					HandleAxis(device_, state_, deltaState_, PadButtonAxis9, yValue);
				};
			}
		}
#endif

#if (defined(GAINPUT_PLATFORM_IOS) && defined(__IPHONE_14_5)) || (defined(MAC_OS_VERSION_11_3) && MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_VERSION_11_3)
		if (IOS17_RUNTIME)
		{
			if ([gamepadExtended isKindOfClass:[GCDualSenseGamepad class]])
			{
				GCDualSenseGamepad* dualSensekGamepad = (GCDualSenseGamepad*)gamepadExtended;

				dualSensekGamepad.touchpadButton.valueChangedHandler = ^(GCControllerButtonInput * _Nonnull button, float value, BOOL pressed)
				{
					HandleButton(device_, state_, deltaState_, PadButton17, pressed);
				};

				dualSensekGamepad.touchpadPrimary.valueChangedHandler = ^(GCControllerDirectionPad * _Nonnull dpad, float xValue, float yValue)
				{
					HandleAxis(device_, state_, deltaState_, PadButtonAxis6, xValue);
					HandleAxis(device_, state_, deltaState_, PadButtonAxis7, yValue);
				};

				dualSensekGamepad.touchpadSecondary.valueChangedHandler = ^(GCControllerDirectionPad * _Nonnull dpad, float xValue, float yValue)
				{
					HandleAxis(device_, state_, deltaState_, PadButtonAxis8, xValue);
					HandleAxis(device_, state_, deltaState_, PadButtonAxis9, yValue);
				};
			}
		}
#endif

		if(this->isExtended_)
		{
			gamepadExtended.leftThumbstick.valueChangedHandler = ^(GCControllerDirectionPad * _Nonnull dpad, float xValue, float yValue) {
					HandleAxis(device_, state_, deltaState_, PadButtonLeftStickX, xValue);
					HandleAxis(device_, state_, deltaState_, PadButtonLeftStickY, yValue);
			};
			gamepadExtended.rightThumbstick.valueChangedHandler = ^(GCControllerDirectionPad * _Nonnull dpad, float xValue, float yValue) {
					HandleAxis(device_, state_, deltaState_, PadButtonRightStickX, xValue);
					HandleAxis(device_, state_, deltaState_, PadButtonRightStickY, yValue);
			};
		}

		if (GCMotion* motion = [controller motion])
		{
			motion.valueChangedHandler = ^(GCMotion * _Nonnull motion)
			{

				HandleAxis(device_, state_, deltaState_, PadButtonAccelerationX, motion.userAcceleration.x);
				HandleAxis(device_, state_, deltaState_, PadButtonAccelerationY, motion.userAcceleration.y);
				HandleAxis(device_, state_, deltaState_, PadButtonAccelerationZ, motion.userAcceleration.z);

				HandleAxis(device_, state_, deltaState_, PadButtonGravityX, motion.gravity.x);
				HandleAxis(device_, state_, deltaState_, PadButtonGravityY, motion.gravity.y);
				HandleAxis(device_, state_, deltaState_, PadButtonGravityZ, motion.gravity.z);

				 const float gyroX = 2.0f * (motion.attitude.x * motion.attitude.z + motion.attitude.w * motion.attitude.y);
				 const float gyroY = 2.0f * (motion.attitude.y * motion.attitude.z - motion.attitude.w * motion.attitude.x);
				 const float gyroZ = 1.0f - 2.0f * (motion.attitude.x * motion.attitude.x + motion.attitude.y * motion.attitude.y);
				 HandleAxis(device_, state_, deltaState_, PadButtonGyroscopeX, gyroX);
				 HandleAxis(device_, state_, deltaState_, PadButtonGyroscopeY, gyroY);
				 HandleAxis(device_, state_, deltaState_, PadButtonGyroscopeZ, gyroZ);
			};
		}

	}
}

bool InputDevicePadImplGCKit::IsValidButton(DeviceButtonId deviceButton) const
{
	if (supportsMotion_ && deviceButton >= PadButtonAccelerationX && deviceButton <= PadButtonMagneticFieldZ)
	{
		return true;
	}
	if (isExtended_)
	{
		return (deviceButton >= PadButtonLeftStickX && deviceButton <= PadButtonAxis5)
			|| (deviceButton >= PadButtonStart && deviceButton <= PadButtonHome);
	}
	return (deviceButton >= PadButtonStart && deviceButton <= PadButtonHome);
}

}

#endif
