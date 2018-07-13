
#ifndef GAINPUTINPUTDEVICETOUCHNULL_H_
#define GAINPUTINPUTDEVICETOUCHNULL_H_

namespace gainput
{

class InputDeviceTouchImplNull : public InputDeviceTouchImpl
{
public:
	InputDeviceTouchImplNull(InputManager& manager, InputDevice& device)
	{
	}

	InputDevice::DeviceVariant GetVariant() const
	{
		return InputDevice::DV_NULL;
	}

	void Update(InputDeltaState* /*delta*/)
	{
	}

	InputDevice::DeviceState GetState() const { return InputDevice::DS_OK; }
};

}

#endif

