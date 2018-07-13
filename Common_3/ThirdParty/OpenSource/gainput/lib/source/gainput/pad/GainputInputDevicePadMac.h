
#ifndef GAINPUTINPUTDEVICEPADMAC_H_
#define GAINPUTINPUTDEVICEPADMAC_H_


namespace gainput
{

class InputDevicePadImplMac : public InputDevicePadImpl
{
public:
	InputDevicePadImplMac(InputManager& manager, InputDevice& device, unsigned index, InputState& state, InputState& previousState);
	~InputDevicePadImplMac();

	InputDevice::DeviceVariant GetVariant() const
	{
		return InputDevice::DV_STANDARD;
	}

	void Update(InputDeltaState* delta);

	InputDevice::DeviceState GetState() const
	{
		return deviceState_;
	}

	bool IsValidButton(DeviceButtonId deviceButton) const;

	bool Vibrate(float leftMotor, float rightMotor)
	{
		return false;
	}

	HashMap<unsigned, DeviceButtonId> buttonDialect_;
	HashMap<unsigned, DeviceButtonId> axisDialect_;
	float minAxis_;
	float maxAxis_;
	float minTriggerAxis_;
	float maxTriggerAxis_;
	InputManager& manager_;
	InputDevice& device_;
	unsigned index_;
	InputState& state_;
	InputState& previousState_;
	InputState nextState_;
	InputDeltaState* delta_;
	InputDevice::DeviceState deviceState_;

	void* ioManager_;

private:
};

}

#endif

