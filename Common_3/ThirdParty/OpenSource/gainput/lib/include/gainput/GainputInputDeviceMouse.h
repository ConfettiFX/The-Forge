
#ifndef GAINPUTINPUTDEVICEMOUSE_H_
#define GAINPUTINPUTDEVICEMOUSE_H_

namespace gainput
{

/// All valid device buttons for InputDeviceMouse.
enum MouseButton
{
	MouseButton0 = 0,
	MouseButtonLeft = MouseButton0,
	MouseButton1,
	MouseButtonMiddle = MouseButton1,
	MouseButton2,
	MouseButtonRight = MouseButton2,
	MouseButton3,
	MouseButtonWheelUp = MouseButton3,
	MouseButton4,
	MouseButtonWheelDown = MouseButton4,
	MouseButton5,
	MouseButton6,
	MouseButton7,
	MouseButton8,
	MouseButton9,
	MouseButton10,
	MouseButton11,
	MouseButton12,
	MouseButton13,
	MouseButton14,
	MouseButton15,
	MouseButton16,
	MouseButton17,
	MouseButton18,
	MouseButton19,
	MouseButton20,
	MouseButtonMax = MouseButton20,
	MouseButtonCount,
	MouseAxisX = MouseButtonCount,
	MouseAxisY,
	MouseButtonCount_,
	MouseAxisCount = MouseButtonCount_ - MouseAxisX
};



class InputDeviceMouseImpl;

/// A mouse input device.
/**
 * This input device provides support for standard mouse devices. The valid device buttons are defined
 * in the ::MouseButton enum.
 *
 * This device is implemented on Linux and Windows.
 *
 * The raw variants (InputDevice::DV_RAW) of this device do not offer normalized absolute axis values. 
 * That means that the values of MouseAxisX and MouseAxisY don't have defined mininum or maximum 
 * values. Therefore only the delta (InputMap::GetFloatDelta()) should be used with raw mouse devices.
 */
class GAINPUT_LIBEXPORT InputDeviceMouse : public InputDevice
{
public:
	/// Initializes the device.
	/**
	 * Instantiate the device using InputManager::CreateDevice().
	 *
	 * \param manager The input manager this device is managed by.
	 * \param device The ID of this device.
	 */
	InputDeviceMouse(InputManager& manager, DeviceId device, unsigned index, DeviceVariant variant);
	/// Shuts down the device.
	~InputDeviceMouse();

	/// Returns DT_MOUSE.
	DeviceType GetType() const override{ return DT_MOUSE; }
	DeviceVariant GetVariant() const override;
	const char* GetTypeName() const override { return "mouse"; }
	bool IsValidButtonId(DeviceButtonId deviceButton) const override { return deviceButton < MouseButtonCount_; }

	size_t GetAnyButtonDown(DeviceButtonSpec* outButtons, size_t maxButtonCount) const override;

	size_t GetButtonName(DeviceButtonId deviceButton, char* buffer, size_t bufferLength) const override;
	ButtonType GetButtonType(DeviceButtonId deviceButton) const override;
	DeviceButtonId GetButtonByName(const char* name) const override;

	/// Returns the platform-specific implementation of this device (internal use only).
	InputDeviceMouseImpl* GetPimpl() { return impl_; } 
	InputState* GetNextInputState() override;
	
	void WarpMouse(const float&x, const float&y) override;

protected:
	void InternalUpdate(InputDeltaState* delta) override;
	DeviceState InternalGetState() const override;

private:
	InputDeviceMouseImpl* impl_;

};

}

#endif

