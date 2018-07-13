
#ifndef GAINPUTINPUTDEVICEPADNULL_H_
#define GAINPUTINPUTDEVICEPADNULL_H_


namespace gainput
{

class InputDevicePadImplNull : public InputDevicePadImpl
{
public:
	InputDevicePadImplNull(InputManager& manager, InputDevice& device, unsigned index, InputState& state, InputState& previousState)
	{
	}

	InputDevice::DeviceVariant GetVariant() const
	{
		return InputDevice::DV_NULL;
	}

	void Update(InputDeltaState* /*delta*/)
	{
	}

	InputDevice::DeviceState GetState() const
	{
		return InputDevice::DS_OK;
	}

	bool IsValidButton(DeviceButtonId /*deviceButton*/) const
	{
		return false;
	}

	bool Vibrate(float /*leftMotor*/, float /*rightMotor*/)
	{
		return false;
	}
};

}

#endif
