
#ifndef GAINPUTINPUTDEVICEPADNULL_H_
#define GAINPUTINPUTDEVICEPADNULL_H_


namespace gainput
{

class InputDevicePadImplNull : public InputDevicePadImpl
{
public:
	InputDevicePadImplNull(InputManager& manager, InputDevice& device, unsigned index, InputState& state, InputState& previousState) :
		manager_(manager),
		device_(device)
	{
		UNREF_PARAM(index); 
		UNREF_PARAM(state); 
		UNREF_PARAM(previousState); 
		DeviceId newId = manager_.GetNextId();
		device_.OverrideDeviceId(newId);
		manager_.AddDevice(newId, &device_);
	}

	~InputDevicePadImplNull()
	{
		manager_.RemoveDevice(device_.GetDeviceId());
		device_.OverrideDeviceId(InvalidDeviceId);
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

private:
	InputManager& manager_;
	InputDevice& device_;
};

}

#endif
