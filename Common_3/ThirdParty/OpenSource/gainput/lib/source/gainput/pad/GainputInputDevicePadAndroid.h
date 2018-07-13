
#ifndef GAINPUTINPUTDEVICEPADANDROID_H_
#define GAINPUTINPUTDEVICEPADANDROID_H_

#include "GainputInputDevicePadImpl.h"
#include <gainput/GainputHelpers.h>

namespace gainput
{

class InputDevicePadImplAndroid : public InputDevicePadImpl
{
public:
	InputDevicePadImplAndroid(InputManager& manager, InputDevice& device, unsigned index, InputState& state, InputState& previousState) :
		manager_(manager),
		device_(device),
		state_(&state),
		previousState_(&previousState),
		nextState_(manager.GetAllocator(), PadButtonMax_),
		delta_(0),
		index_(index),
		deviceState_(InputDevice::DS_UNAVAILABLE)
	{
		(void)previousState;
		GAINPUT_ASSERT(index_ < MaxPadCount);
	}

	~InputDevicePadImplAndroid()
	{
	}

	InputDevice::DeviceVariant GetVariant() const
	{
		return InputDevice::DV_STANDARD;
	}

	void Update(InputDeltaState* delta)
	{
		delta_ = delta;
		*state_ = nextState_;
	}

	InputDevice::DeviceState GetState() const
	{
		return deviceState_;
	}

	void SetState(InputDevice::DeviceState state)
	{
		deviceState_ = state;
	}

	bool IsValidButton(DeviceButtonId deviceButton) const
	{
		return deviceButton < PadButtonMax_;
	}

	bool Vibrate(float /*leftMotor*/, float /*rightMotor*/)
	{
		return false; // TODO
	}

	InputState* GetNextInputState() { return &nextState_; }

private:
	InputManager& manager_;
	InputDevice& device_;
	InputState* state_;
	InputState* previousState_;
	InputState nextState_;
	InputDeltaState* delta_;
	unsigned index_;
	InputDevice::DeviceState deviceState_;

};

}

#endif
