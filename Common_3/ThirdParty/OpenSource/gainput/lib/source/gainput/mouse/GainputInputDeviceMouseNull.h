
#ifndef GAINPUTINPUTDEVICEMOUSENULL_H_
#define GAINPUTINPUTDEVICEMOUSENULL_H_

namespace gainput
{

class InputDeviceMouseImplNull : public InputDeviceMouseImpl
{
public:
	InputDeviceMouseImplNull(InputManager& /*manager*/, DeviceId /*device*/)
	{
	}

	InputDevice::DeviceVariant GetVariant() const
	{
		return InputDevice::DV_NULL;
	}

	void Update(InputDeltaState* /*delta*/)
	{
	}
};

}

#endif

