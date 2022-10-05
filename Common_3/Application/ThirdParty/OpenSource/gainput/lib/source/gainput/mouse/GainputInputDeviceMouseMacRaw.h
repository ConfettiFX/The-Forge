
#ifndef GAINPUTINPUTDEVICEMOUSEMACRAW_H_
#define GAINPUTINPUTDEVICEMOUSEMACRAW_H_

#include "GainputInputDeviceMouseImpl.h"

namespace gainput
{

class InputDeviceMouseImplMacRaw : public InputDeviceMouseImpl
{
public:
	InputDeviceMouseImplMacRaw(InputManager& manager, InputDevice& device, InputState& state, InputState& previousState);
	~InputDeviceMouseImplMacRaw();

	InputDevice::DeviceVariant GetVariant() const override
	{
		return InputDevice::DV_RAW;
	}
	
	virtual InputState * GetNextInputState() override {
		return &nextState_;
	}

	InputDevice::DeviceState GetState() const override { return deviceState_; }

	void Update(InputDeltaState* delta) override;
	void HandleMouseMove(float x, float y);

	InputManager& manager_;
	InputDevice::DeviceState deviceState_;
	InputDevice& device_;
	InputState* state_;
	InputState* previousState_;
	InputState nextState_;
	InputDeltaState* delta_;

	float mousePosAccumulationX_;
	float mousePosAccumulationY_;
};

}

#endif

