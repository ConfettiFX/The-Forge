#include "../../../include/gainput/gainput.h"

#if defined(GAINPUT_PLATFORM_IOS) || defined(GAINPUT_PLATFORM_TVOS)

#include "GainputInputDevicePadImpl.h"
#include "../../../include/gainput/GainputInputDeltaState.h"
#include "../../../include/gainput/GainputHelpers.h"
#include "../../../include/gainput/GainputLog.h"

#include "GainputInputDevicePadIos.h"
#include "../../../include/gainput/GainputIos.h"

namespace gainput
{
InputDevicePadImplIos::GlobalControllerList* InputDevicePadImplIos::mappedControllers_;

namespace
{
static bool isAppleTvRemote(GCController* controller)
{
#if defined(GAINPUT_IOS_HAPTICS)
    if (!controller)
    {
        return false;
    }
    GCGamepad* gamepad = [controller gamepad];
    GCMicroGamepad* microGamepad = [controller microGamepad];
    GCExtendedGamepad* extgamepad = [controller extendedGamepad];
    GCMotion* motion = [controller motion];
    
    return
        microGamepad != NULL &&
        motion != NULL &&
        gamepad == NULL &&
        extgamepad == NULL;
#else
    return false;
#endif
}

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
    if (!InputDevicePadImplIos::mappedControllers_)
    {
        return;
    }
    InputDevicePadImplIos::GlobalControllerList& mapping = *InputDevicePadImplIos::mappedControllers_;
    for (InputDevicePadImplIos::GlobalControllerList::iterator it = mapping.begin(); it != mapping.end(); )
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
        || !InputDevicePadImplIos::mappedControllers_)
    {
        return false;
    }
    InputDevicePadImplIos::GlobalControllerList& mapping = *InputDevicePadImplIos::mappedControllers_;
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
#if defined(GAINPUT_IOS_HAPTICS)
    if(@available(iOS 13.0, *))
    {
        id<CHHapticDeviceCapability> capabilities = [CHHapticEngine capabilitiesForHardware];
        return capabilities.supportsHaptics;
    }
#endif
    return false;
}

InputDevicePadImplIos::InputDevicePadImplIos(InputManager& manager, InputDevice& device, unsigned index, InputState& state, InputState& previousState) :
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
    gcController_(NULL)
#ifdef GAINPUT_IOS_HAPTICS
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

	__block InputDevicePadImplIos*devPad = this;
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
                                                if(deviceChangeCb)
                                                    (*deviceChangeCb)(GetDeviceName(), false, devPad->index_);
												devPad->deviceState_ = InputDevice::DS_UNAVAILABLE;
												devPad->gcController_ = NULL;
                                                if (@available(iOS 13.0, *)) {
                                                    IOSHapticMotor* leftMotorDevice = (IOSHapticMotor*)devPad->hapticMotorLeft;
                                                    IOSHapticMotor* rightMotorDevice = (IOSHapticMotor*)devPad->hapticMotorRight;
													if(leftMotorDevice)
                                                    	[leftMotorDevice cleanup];
                                                    if(rightMotorDevice)
														[rightMotorDevice cleanup];
                                                    devPad->hapticMotorLeft = nil;
                                                    devPad->hapticMotorRight = nil;
                                                }
											}
										}
									}
								];
}


void InputDevicePadImplIos::SetOnDeviceChangeCallBack(void(*onDeviceChange)(const char*, bool added, int controllerId))
{
    deviceChangeCb = onDeviceChange;
    if(deviceState_ == InputDevice::DS_OK)
    {
        if(deviceChangeCb)
            (*deviceChangeCb)(GetDeviceName(), true, index_);
    }
}

InputDevicePadImplIos::~InputDevicePadImplIos()
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


const char* InputDevicePadImplIos::GetDeviceName()
{
    if(deviceState_ != InputDevice::DS_OK)
        return nullptr;
    
    GCController* controller = getGcController(gcController_);
    if(controller)
    {
        return [controller.vendorName cStringUsingEncoding:NSASCIIStringEncoding];
    }
    
    return nullptr;
}


bool InputDevicePadImplIos::SetRumbleEffect(float leftMotorIntensity, float rightMotorIntensity, uint32_t duration_ms, bool targetOwningDevice)
{
#if defined(GAINPUT_IOS_HAPTICS)
	if(deviceState_ != InputDevice::DS_OK && targetOwningDevice == false)
		return false;

	float durationSec = (float)duration_ms / 1000.0f;

	if (@available(iOS 13, *))
	{
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
        {
            if(hapticMotorLeft)
            {
				IOSHapticMotor* leftMotorDevice = (IOSHapticMotor*)hapticMotorLeft;
                [leftMotorDevice setIntensity:leftMotorIntensity sharpness:1.0f duration:durationSec];
            }

            if(hapticMotorRight)
            {
				IOSHapticMotor* RightMotorDevice = (IOSHapticMotor*)hapticMotorRight;
                [RightMotorDevice setIntensity:rightMotorIntensity sharpness:1.0f duration:durationSec];
            }
        }
		return true;
	}
#endif

	return false;
}

void InputDevicePadImplIos::SetupController()
{

	GCController* newIncController = GetFirstNonMappedController();

	if(!newIncController)
		return;
	if(@available(iOS 13.0, *))
	{
		if(newIncController.isSnapshot)
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
	isRemote_ = isAppleTvRemote(controller);
	isExtended_ = [controller extendedGamepad] != 0;
#if defined(GAINPUT_PLATFORM_TVOS)
	isMicro_ = [controller microGamepad] != 0;
#else
	isMicro_ = false;
#endif
	isNormal_ = [controller extendedGamepad] != 0;
	supportsMotion_ = [controller motion] != 0;

	if (!isRemote_)
	{
		UpdateGamepad_();
	}

#if defined(GAINPUT_IOS_HAPTICS)
    if(@available(iOS 14.0, *))
    {
        if([controller haptics] != nil)
        {
            if(hapticMotorLeft != nil)
            {
                [(IOSHapticMotor*)hapticMotorLeft cleanup];
                hapticMotorLeft = nil;
            }
            if(hapticMotorRight != nil)
            {
                [(IOSHapticMotor*)hapticMotorRight cleanup];
                hapticMotorRight = nil;
            }
            hapticMotorLeft = [[IOSHapticMotor alloc] initWithController:controller locality:(GCHapticsLocalityLeftHandle)];
            hapticMotorRight = [[IOSHapticMotor alloc] initWithController:controller locality:(GCHapticsLocalityRightHandle)];
        }
    }
#endif

	deviceState_ = InputDevice::DS_OK;
	mappedControllers_->push_back(gcController_);
    
    if(deviceChangeCb)
        (*deviceChangeCb)(GetDeviceName(), true, index_);

}

void InputDevicePadImplIos::Update(InputDeltaState* delta)
{
	if (!gcController_)
	{
		deviceState_ = InputDevice::DS_UNAVAILABLE;
		isExtended_ = false;
		supportsMotion_ = false;
		return;
	}

	deltaState_ = delta;

	if (isRemote_)
	{
		UpdateRemote_(deltaState_);
	}
}

void InputDevicePadImplIos::UpdateRemote_(InputDeltaState* delta)
{
#if defined(GAINPUT_PLATFORM_TVOS)
    GCController* controller = getGcController(gcController_);
    
    if (isMicro_)
    {
        GCMicroGamepad* gamepad = [controller microGamepad];
        gamepad.reportsAbsoluteDpadValues  = YES;
        
        HandleButton(device_, state_, delta, PadButtonA, gamepad.buttonA.pressed); // Force push on touch area
        HandleButton(device_, state_, delta, PadButtonX, gamepad.buttonX.pressed); // Play/Pause Button
        
        HandleAxis(device_, state_, delta, PadButtonAxis30, gamepad.dpad.xAxis.value); // Touch area X
        HandleAxis(device_, state_, delta, PadButtonAxis31, gamepad.dpad.yAxis.value); // Touch area Y
    }
    
    if (supportsMotion_)
    {
        GCMotion* motion = [controller motion];
        
        HandleAxis(device_, state_, delta, PadButtonAccelerationX, motion.userAcceleration.x);
        HandleAxis(device_, state_, delta, PadButtonAccelerationY, motion.userAcceleration.y);
        HandleAxis(device_, state_, delta, PadButtonAccelerationZ, motion.userAcceleration.z);
        
        HandleAxis(device_, state_, delta, PadButtonGravityX, motion.gravity.x);
        HandleAxis(device_, state_, delta, PadButtonGravityY, motion.gravity.y);
        HandleAxis(device_, state_, delta, PadButtonGravityZ, motion.gravity.z);
        
        // The Siri Remote does not have a gyro.
        HandleAxis(device_, state_, delta, PadButtonGyroscopeX, 0.0f);
        HandleAxis(device_, state_, delta, PadButtonGyroscopeY, 0.0f);
        HandleAxis(device_, state_, delta, PadButtonGyroscopeZ, 0.0f);
    }
#endif
}

void InputDevicePadImplIos::UpdateGamepad_()
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

		if (@available(iOS 12.1, tvOS 12.1, *))
		{
			gamepadExtended.leftThumbstickButton.valueChangedHandler = ^(GCControllerButtonInput * _Nonnull button, float value, BOOL pressed)
			{
				HandleButton(device_, state_, deltaState_, PadButtonL3, pressed);
			};
			gamepadExtended.rightThumbstickButton.valueChangedHandler = ^(GCControllerButtonInput * _Nonnull button, float value, BOOL pressed)
			{
				HandleButton(device_, state_, deltaState_, PadButtonR3, pressed);
			};
		}

		if (@available(iOS 13.0, *))
		{
			gamepadExtended.buttonOptions.valueChangedHandler = ^(GCControllerButtonInput * _Nonnull button, float value, BOOL pressed)
			{
				HandleButton(device_, state_, deltaState_, PadButtonSelect, pressed);
			};
			gamepadExtended.buttonMenu.valueChangedHandler = ^(GCControllerButtonInput * _Nonnull button, float value, BOOL pressed)
			{
				HandleButton(device_, state_, deltaState_, PadButtonStart, pressed);
			};
		}

		if (@available(iOS 14.0, *))
		{
			gamepadExtended.buttonHome.valueChangedHandler = ^(GCControllerButtonInput * _Nonnull button, float value, BOOL pressed)
			{
				HandleButton(device_, state_, deltaState_, PadButtonHome, pressed);
			};
		}

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

		controller.controllerPausedHandler = ^(GCController * _Nonnull controller) {
			//do nothing
		};

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

				if (@available(iOS 11.0, *))
				{
					 const float gyroX = 2.0f * (motion.attitude.x * motion.attitude.z + motion.attitude.w * motion.attitude.y);
					 const float gyroY = 2.0f * (motion.attitude.y * motion.attitude.z - motion.attitude.w * motion.attitude.x);
					 const float gyroZ = 1.0f - 2.0f * (motion.attitude.x * motion.attitude.x + motion.attitude.y * motion.attitude.y);
					 HandleAxis(device_, state_, deltaState_, PadButtonGyroscopeX, gyroX);
					 HandleAxis(device_, state_, deltaState_, PadButtonGyroscopeY, gyroY);
					 HandleAxis(device_, state_, deltaState_, PadButtonGyroscopeZ, gyroZ);
				}
			};
		}

	}
#if defined(GAINPUT_PLATFORM_TVOS)
	else if (isMicro_)
	{
		GCMicroGamepad * gamepad =  [controller microGamepad];
		gamepad.reportsAbsoluteDpadValues  = YES;
		HandleButton(device_, state_, deltaState_, PadButtonA, gamepad.buttonA.pressed); // Force push on touch area
		HandleButton(device_, state_, deltaState_, PadButtonX, gamepad.buttonX.pressed); // Play/Pause Button

		HandleAxis(device_, state_, deltaState_, PadButtonAxis30, gamepad.dpad.xAxis.value); // Touch area X
		HandleAxis(device_, state_, deltaState_, PadButtonAxis31, gamepad.dpad.yAxis.value); // Touch area Y
	}
#endif
}

bool InputDevicePadImplIos::IsValidButton(DeviceButtonId deviceButton) const
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
