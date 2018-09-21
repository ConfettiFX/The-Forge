#include "../../../include/gainput/gainput.h"


#ifdef GAINPUT_PLATFORM_MAC

#include "GainputInputDeviceMouseMacRaw.h"

#include "../../../include/gainput/GainputInputDeltaState.h"
#include "../../../include/gainput/GainputHelpers.h"
#include "../../../include/gainput/GainputLog.h"

#import <AppKit/AppKit.h>
//#import <Carbon/Carbon.h>

#import <CoreFoundation/CoreFoundation.h>
#import <IOKit/hid/IOHIDManager.h>
#import <IOKit/hid/IOHIDUsageTables.h>


namespace gainput
{

InputDeviceMouseImplMacRaw::InputDeviceMouseImplMacRaw(InputManager& manager, InputDevice& device, InputState& state, InputState& previousState) :
	manager_(manager),
	device_(device),
	state_(&state),
	previousState_(&previousState),
	nextState_(manager.GetAllocator(), MouseButtonCount + MouseAxisCount),
	delta_(0)
{
	mousePosAccumulationX_ = 0;
	mousePosAccumulationY_ = 0;
	deviceState_ = InputDevice::DeviceState::DS_OK;
}

InputDeviceMouseImplMacRaw::~InputDeviceMouseImplMacRaw()
{
}

void InputDeviceMouseImplMacRaw::HandleMouseMove(float x, float y)
{
	mousePosAccumulationX_ += x;
	mousePosAccumulationY_ += y;
	HandleAxis(device_, nextState_, delta_, gainput::MouseAxisX, mousePosAccumulationX_);
	HandleAxis(device_, nextState_, delta_, gainput::MouseAxisY, mousePosAccumulationY_);
}

void InputDeviceMouseImplMacRaw::Update(InputDeltaState* delta)
{
    delta_ = delta;
    
    // Reset mouse wheel buttons
    HandleButton(device_, nextState_, delta_, MouseButtonWheelDown, false);
    HandleButton(device_, nextState_, delta_, MouseButtonWheelUp, false);
    HandleButton(device_, nextState_, delta_, MouseButtonWheelDown, false);

    *state_ = nextState_;
}

}

#endif

