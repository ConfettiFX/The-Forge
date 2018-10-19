
#ifndef GAINPUTINPUTDEVICEMOUSEMAC_H_
#define GAINPUTINPUTDEVICEMOUSEMAC_H_

#include "GainputInputDeviceMouseImpl.h"
#import <Carbon/Carbon.h>

@class GainputMacInputView;

namespace gainput
{

class InputDeviceMouseImplMac : public InputDeviceMouseImpl
{
public:
	InputDeviceMouseImplMac(InputManager& manager, InputDevice& device, InputState& state, InputState& previousState);
	~InputDeviceMouseImplMac();

	InputDevice::DeviceVariant GetVariant() const override
	{
		return InputDevice::DV_STANDARD;
	}
	
	virtual InputState * GetNextInputState() override {
		return &nextState_;
	}
	
	InputDevice::DeviceState GetState() const override { return deviceState_; }

	void Update(InputDeltaState* delta) override;
	
	void HandleMouseMove(float x, float y);
	void HandleMouseWheel(int deltaY);
	void HandleMouseButton(bool isPressed, gainput::DeviceButtonId mouseButtonId );

	InputManager& manager_;
	InputDevice::DeviceState deviceState_;
	InputDevice& device_;
	InputState* state_;
	InputState* previousState_;
	InputState nextState_;
	InputDeltaState* delta_;
};

}

#endif

