
#ifndef GAINPUTINPUTDEVICEPAD_H_
#define GAINPUTINPUTDEVICEPAD_H_

namespace gainput
{

/// The maximum number of pads supported.
enum { MaxPadCount = 10 };

/// All valid device buttons for InputDevicePad.
enum PadButton
{
	PadButtonLeftStickX,
	PadButtonLeftStickY,
	PadButtonRightStickX,
	PadButtonRightStickY,
	PadButtonAxis4, // L2/Left trigger
	PadButtonAxis5, // R2/Right trigger
	PadButtonAxis6,
	PadButtonAxis7,
	PadButtonAxis8,
	PadButtonAxis9,
	PadButtonAxis10,
	PadButtonAxis11,
	PadButtonAxis12,
	PadButtonAxis13,
	PadButtonAxis14,
	PadButtonAxis15,
	PadButtonAxis16,
	PadButtonAxis17,
	PadButtonAxis18,
	PadButtonAxis19,
	PadButtonAxis20,
	PadButtonAxis21,
	PadButtonAxis22,
	PadButtonAxis23,
	PadButtonAxis24,
	PadButtonAxis25,
	PadButtonAxis26,
	PadButtonAxis27,
	PadButtonAxis28,
	PadButtonAxis29,
	PadButtonAxis30,
	PadButtonAxis31,
	PadButtonAccelerationX,
	PadButtonAccelerationY,
	PadButtonAccelerationZ,
	PadButtonGravityX,
	PadButtonGravityY,
	PadButtonGravityZ,
	PadButtonGyroscopeX,
	PadButtonGyroscopeY,
	PadButtonGyroscopeZ,
	PadButtonMagneticFieldX,
	PadButtonMagneticFieldY,
	PadButtonMagneticFieldZ,
	PadButtonStart,
	PadButtonAxisCount_ = PadButtonStart,
	PadButtonSelect,
	PadButtonLeft,
	PadButtonRight,
	PadButtonUp,
	PadButtonDown,
	PadButtonA, // Cross
	PadButtonB, // Circle
	PadButtonX, // Square
	PadButtonY, // Triangle
	PadButtonL1,
	PadButtonR1,
	PadButtonL2,
	PadButtonR2,
	PadButtonL3, // Left thumb
	PadButtonR3, // Right thumb
	PadButtonHome, // PS button
	PadButton17,
	PadButton18,
	PadButton19,
	PadButton20,
	PadButton21,
	PadButton22,
	PadButton23,
	PadButton24,
	PadButton25,
	PadButton26,
	PadButton27,
	PadButton28,
	PadButton29,
	PadButton30,
	PadButton31,
	PadButtonMax_,
	PadButtonCount_ = PadButtonMax_ - PadButtonAxisCount_
};

class InputDevicePadImpl;

/// A pad input device.
/**
 * This input device provides support for gamepad devices. The valid device buttons are defined
 * in the ::PadButton enum.
 *
 * This device is implemented on Android NDK, Linux and Windows.
 *
 * Note that the Android implementation does not support any external pads, but only internal
 * sensors (acceleration, gyroscope, magnetic field).
 */
class GAINPUT_LIBEXPORT InputDevicePad : public InputDevice
{
public:
	/// The operating system device IDs for all possible pads.
	static const char* PadDeviceIds[MaxPadCount];
	// TODO SetPadDeviceId(padIndex, const char* id);

	/// Initializes the device.
	/**
	 * Instantiate the device using InputManager::CreateDevice().
	 *
	 * \param manager The input manager this device is managed by.
	 * \param device The ID of this device.
	 */
	InputDevicePad(InputManager& manager, DeviceId device, unsigned index, DeviceVariant variant);
	/// Shuts down the device.
	~InputDevicePad();

	/// Returns DT_PAD.
	DeviceType GetType() const { return DT_PAD; }
	DeviceVariant GetVariant() const;
	const char* GetTypeName() const { return "pad"; }
    bool IsValidButtonId(DeviceButtonId deviceButton) const;

	size_t GetAnyButtonDown(DeviceButtonSpec* outButtons, size_t maxButtonCount) const;

	size_t GetButtonName(DeviceButtonId deviceButton, char* buffer, size_t bufferLength) const;
	ButtonType GetButtonType(DeviceButtonId deviceButton) const;
	DeviceButtonId GetButtonByName(const char* name) const;

	InputState* GetNextInputState();

	/// Enables the rumble feature of the pad.
	/**
	 * \param leftMotor Speed of the left motor, between 0.0 and 1.0.
	 * \param rightMotor Speed of the right motor, between 0.0 and 1.0.
	 * \return true if rumble was enabled successfully, false otherwise.
	 */
	bool Vibrate(float leftMotor, float rightMotor);

	/// Returns the platform-specific implementation of this device.
	InputDevicePadImpl* GetPimpl() { return impl_; }

	const char* GetDeviceName();
	bool SetRumbleEffect(float left_motor, float right_motor, uint32_t duration_ms);
	void SetLEDColor(uint8_t r, uint8_t g, uint8_t b);

protected:
	void InternalUpdate(InputDeltaState* delta);

	DeviceState InternalGetState() const;

private:
	InputDevicePadImpl* impl_;

};

}

#endif

