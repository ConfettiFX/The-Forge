#include "../../../include/gainput/gainput.h"

#if defined(GAINPUT_PLATFORM_IOS) || defined(GAINPUT_PLATFORM_TVOS)

#include "GainputInputDevicePadImpl.h"
#include "../../../include/gainput/GainputInputDeltaState.h"
#include "../../../include/gainput/GainputHelpers.h"
#include "../../../include/gainput/GainputLog.h"

#include "GainputInputDevicePadIos.h"

#import <GameController/GameController.h>

namespace gainput
{

InputDevicePadImplIos::GlobalControllerList* InputDevicePadImplIos::mappedControllers_;

namespace
{
static bool isAppleTvRemote(GCController* controller)
{
#if defined(GAINPUT_PLATFORM_TVOS)
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

InputDevicePadImplIos::InputDevicePadImplIos(InputManager& manager, InputDevice& device, unsigned index, InputState& state, InputState& previousState) :
    manager_(manager),
    device_(device),
    index_(index),
    state_(state),
    previousState_(previousState),
    deviceState_(InputDevice::DS_UNAVAILABLE),
    pausePressed_(false),
    isMicro_(false),
    isNormal_(false),
    isExtended_(false),
    supportsMotion_(false),
    isRemote_(false),
    gcController_(0)
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
}

InputDevicePadImplIos::~InputDevicePadImplIos()
{
	if (mappedControllers_)
	{
		manager_.GetAllocator().Delete(mappedControllers_);
		mappedControllers_ = NULL;
	}
}

void InputDevicePadImplIos::Update(InputDeltaState* delta)
{
    bool validHardwareController = IsValidHardwareController(getGcController(gcController_));
    
    if (!validHardwareController)
    {
        CleanDisconnectedControllersFromMapping();
        GCController * firstNonMappedController = GetFirstNonMappedController();
        
        if (firstNonMappedController)
        {
            GCControllerPlayerIndex newIndex = static_cast<GCControllerPlayerIndex>(index_);
            if (firstNonMappedController.playerIndex != newIndex)
            {
                firstNonMappedController.playerIndex = newIndex;
            }

            // register pause menu button handler
            __block InputDevicePadImplIos* block_deviceImpl = this;
            firstNonMappedController.controllerPausedHandler = ^(GCController* controller)
            {
                block_deviceImpl->pausePressed_ = true;
            };
        }
        else
        {
            deviceState_ = InputDevice::DS_UNAVAILABLE;
            return;
        }
        gcController_ = firstNonMappedController;
    }
    
    GAINPUT_ASSERT(gcController_);
    if (!gcController_)
    {
        deviceState_ = InputDevice::DS_UNAVAILABLE;
        isExtended_ = false;
        supportsMotion_ = false;
        return;
    }

    deviceState_ = InputDevice::DS_OK;

    GCController* controller = getGcController(gcController_);

    isRemote_ = isAppleTvRemote(controller);
    isExtended_ = [controller extendedGamepad] != 0;
#if defined(GAINPUT_PLATFORM_TVOS)
    isMicro_ = [controller microGamepad] != 0;
#else
    isMicro_ = false;
#endif
    isNormal_ = [controller extendedGamepad] != 0;
    supportsMotion_ = [controller motion] != 0;
    
    if (isRemote_)
    {
        UpdateRemote_(delta);
    }
    else
    {
        UpdateGamepad_(delta);
    }
    
    HandleButton(device_, state_, delta, PadButtonHome, pausePressed_);
    pausePressed_ = false;
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

void InputDevicePadImplIos::UpdateGamepad_(InputDeltaState* delta)
{
    GCController* controller = getGcController(gcController_);

    if (isExtended_)
    {
        GCExtendedGamepad* gamepad = [controller extendedGamepad];
        
        HandleButton(device_, state_, delta, PadButtonL1, gamepad.leftShoulder.pressed);
        HandleButton(device_, state_, delta, PadButtonR1, gamepad.rightShoulder.pressed);
        
        HandleButton(device_, state_, delta, PadButtonLeft, gamepad.dpad.left.pressed);
        HandleButton(device_, state_, delta, PadButtonRight, gamepad.dpad.right.pressed);
        HandleButton(device_, state_, delta, PadButtonUp, gamepad.dpad.up.pressed);
        HandleButton(device_, state_, delta, PadButtonDown, gamepad.dpad.down.pressed);
        
        HandleButton(device_, state_, delta, PadButtonA, gamepad.buttonA.pressed);
        HandleButton(device_, state_, delta, PadButtonB, gamepad.buttonB.pressed);
        HandleButton(device_, state_, delta, PadButtonX, gamepad.buttonX.pressed);
        HandleButton(device_, state_, delta, PadButtonY, gamepad.buttonY.pressed);
        
        HandleAxis(device_, state_, delta, PadButtonLeftStickX, gamepad.leftThumbstick.xAxis.value);
        HandleAxis(device_, state_, delta, PadButtonLeftStickY, gamepad.leftThumbstick.yAxis.value);
        
        HandleAxis(device_, state_, delta, PadButtonRightStickX, gamepad.rightThumbstick.xAxis.value);
        HandleAxis(device_, state_, delta, PadButtonRightStickY, gamepad.rightThumbstick.yAxis.value);
        
        HandleButton(device_, state_, delta, PadButtonL2, gamepad.leftTrigger.pressed);
        HandleAxis(device_, state_, delta, PadButtonAxis4, gamepad.leftTrigger.value);
        HandleButton(device_, state_, delta, PadButtonR2, gamepad.rightTrigger.pressed);
        HandleAxis(device_, state_, delta, PadButtonAxis5, gamepad.rightTrigger.value);
    }
    else if (isNormal_)
    {
        GCExtendedGamepad* gamepad = [controller extendedGamepad];
        
        HandleButton(device_, state_, delta, PadButtonL1, gamepad.leftShoulder.pressed);
        HandleButton(device_, state_, delta, PadButtonR1, gamepad.rightShoulder.pressed);
        
        HandleButton(device_, state_, delta, PadButtonLeft, gamepad.dpad.left.pressed);
        HandleButton(device_, state_, delta, PadButtonRight, gamepad.dpad.right.pressed);
        HandleButton(device_, state_, delta, PadButtonUp, gamepad.dpad.up.pressed);
        HandleButton(device_, state_, delta, PadButtonDown, gamepad.dpad.down.pressed);
        
        HandleButton(device_, state_, delta, PadButtonA, gamepad.buttonA.pressed);
        HandleButton(device_, state_, delta, PadButtonB, gamepad.buttonB.pressed);
        HandleButton(device_, state_, delta, PadButtonX, gamepad.buttonX.pressed);
        HandleButton(device_, state_, delta, PadButtonY, gamepad.buttonY.pressed);
    }
#if defined(GAINPUT_PLATFORM_TVOS)
    else if (isMicro_)
    {
        GCMicroGamepad * gamepad =  [controller microGamepad];
        gamepad.reportsAbsoluteDpadValues  = YES;
        HandleButton(device_, state_, delta, PadButtonA, gamepad.buttonA.pressed); // Force push on touch area
        HandleButton(device_, state_, delta, PadButtonX, gamepad.buttonX.pressed); // Play/Pause Button
        
        HandleAxis(device_, state_, delta, PadButtonAxis30, gamepad.dpad.xAxis.value); // Touch area X
        HandleAxis(device_, state_, delta, PadButtonAxis31, gamepad.dpad.yAxis.value); // Touch area Y
    }
#endif
    
    if (GCMotion* motion = [controller motion])
    {
        HandleAxis(device_, state_, delta, PadButtonAccelerationX, motion.userAcceleration.x);
        HandleAxis(device_, state_, delta, PadButtonAccelerationY, motion.userAcceleration.y);
        HandleAxis(device_, state_, delta, PadButtonAccelerationZ, motion.userAcceleration.z);
        
        HandleAxis(device_, state_, delta, PadButtonGravityX, motion.gravity.x);
        HandleAxis(device_, state_, delta, PadButtonGravityY, motion.gravity.y);
        HandleAxis(device_, state_, delta, PadButtonGravityZ, motion.gravity.z);
        
		if (@available(iOS 11.0, *))
		{
			const float gyroX = 2.0f * (motion.attitude.x * motion.attitude.z + motion.attitude.w * motion.attitude.y);
			const float gyroY = 2.0f * (motion.attitude.y * motion.attitude.z - motion.attitude.w * motion.attitude.x);
			const float gyroZ = 1.0f - 2.0f * (motion.attitude.x * motion.attitude.x + motion.attitude.y * motion.attitude.y);
			
			HandleAxis(device_, state_, delta, PadButtonGyroscopeX, gyroX);
			HandleAxis(device_, state_, delta, PadButtonGyroscopeY, gyroY);
			HandleAxis(device_, state_, delta, PadButtonGyroscopeZ, gyroZ);
		}
    }
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
            || (deviceButton >= PadButtonLeft && deviceButton <= PadButtonR2)
            || deviceButton == PadButtonHome;
    }
    return (deviceButton >= PadButtonLeft && deviceButton <= PadButtonR1)
        || deviceButton == PadButtonHome;
}

}

#endif
