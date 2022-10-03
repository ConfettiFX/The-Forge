
#ifndef GAINPUTINPUTDEVICEBUILTIN_H_
#define GAINPUTINPUTDEVICEBUILTIN_H_

namespace gainput
{

/// All valid device buttons for InputDeviceBuiltIn.
enum BuiltInButton
{
	BuiltInButtonAccelerationX,
	BuiltInButtonAccelerationY,
	BuiltInButtonAccelerationZ,
	BuiltInButtonGravityX,
	BuiltInButtonGravityY,
	BuiltInButtonGravityZ,
	BuiltInButtonGyroscopeX,
	BuiltInButtonGyroscopeY,
	BuiltInButtonGyroscopeZ,
	BuiltInButtonMagneticFieldX,
	BuiltInButtonMagneticFieldY,
	BuiltInButtonMagneticFieldZ,
	BuiltInButtonCount_
};

class InputDeviceBuiltInImpl;

/// An input device for inputs that are directly built into the executing device (for example, sensors in a phone).
class GAINPUT_LIBEXPORT InputDeviceBuiltIn : public InputDevice
{
public:
	/// Initializes the device.
	/**
	 * Instantiate the device using InputManager::CreateDevice().
	 *
	 * \param manager The input manager this device is managed by.
	 * \param device The ID of this device.
	 */
	InputDeviceBuiltIn(InputManager& manager, DeviceId device, unsigned index, DeviceVariant variant);
	/// Shuts down the device.
	~InputDeviceBuiltIn();

	/// Returns DT_BUILTIN.
	DeviceType GetType() const { return DT_BUILTIN; }
	DeviceVariant GetVariant() const;
	const char* GetTypeName() const { return "builtin"; }
	bool IsValidButtonId(DeviceButtonId deviceButton) const;

	size_t GetAnyButtonDown(DeviceButtonSpec* outButtons, size_t maxButtonCount) const;

	size_t GetButtonName(DeviceButtonId deviceButton, char* buffer, size_t bufferLength) const;
	ButtonType GetButtonType(DeviceButtonId deviceButton) const;
	DeviceButtonId GetButtonByName(const char* name) const;

protected:
	void InternalUpdate(InputDeltaState* delta);

	DeviceState InternalGetState() const;

private:
	InputDeviceBuiltInImpl* impl_;

};

}

#endif

