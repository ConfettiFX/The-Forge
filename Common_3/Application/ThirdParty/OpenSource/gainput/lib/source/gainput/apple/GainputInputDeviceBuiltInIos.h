
#ifndef GAINPUTINPUTDEVICEBUILTINIOS_H_
#define GAINPUTINPUTDEVICEBUILTINIOS_H_


namespace gainput
{

class InputDeviceBuiltInImplIos : public InputDeviceBuiltInImpl
{
public:
	InputDeviceBuiltInImplIos(InputManager& manager, InputDevice& device, unsigned index, InputState& state, InputState& previousState);
	~InputDeviceBuiltInImplIos();

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

	bool pausePressed_;

private:
	InputManager& manager_ __unused;
	InputDevice& device_;
	unsigned index_ __unused;
	bool padFound_ __unused;
	InputState& state_;
	InputState& previousState_ __unused;
	InputDevice::DeviceState deviceState_;

	void* motionManager_;

};

}

#endif

