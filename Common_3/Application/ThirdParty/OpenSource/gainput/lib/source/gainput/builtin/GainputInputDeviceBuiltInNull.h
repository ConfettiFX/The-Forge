
#ifndef GAINPUTINPUTDEVICEBUILTINNULL_H_
#define GAINPUTINPUTDEVICEBUILTINNULL_H_


namespace gainput
{

class InputDeviceBuiltInImplNull : public InputDeviceBuiltInImpl
{
public:
	InputDeviceBuiltInImplNull(InputManager& /*manager*/, InputDevice& /*device*/, unsigned /*index*/, InputState& /*state*/, InputState& /*previousState*/)
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
};

}

#endif
