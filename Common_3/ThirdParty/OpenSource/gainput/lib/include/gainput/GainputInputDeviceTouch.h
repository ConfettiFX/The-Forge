
#ifndef GAINPUTINPUTDEVICETOUCH_H_
#define GAINPUTINPUTDEVICETOUCH_H_

namespace gainput
{

/// All valid device inputs for InputDeviceTouch.
enum TouchButton
{
	Touch0Down,
	Touch0X,
	Touch0Y,
	Touch0Pressure,
	Touch1Down,
	Touch1X,
	Touch1Y,
	Touch1Pressure,
	Touch2Down,
	Touch2X,
	Touch2Y,
	Touch2Pressure,
	Touch3Down,
	Touch3X,
	Touch3Y,
	Touch3Pressure,
	Touch4Down,
	Touch4X,
	Touch4Y,
	Touch4Pressure,
	Touch5Down,
	Touch5X,
	Touch5Y,
	Touch5Pressure,
	Touch6Down,
	Touch6X,
	Touch6Y,
	Touch6Pressure,
	Touch7Down,
	Touch7X,
	Touch7Y,
	Touch7Pressure,
	TouchCount_
};

enum GestureType
{
    GestureTap,
    GesturePan,
    GesturePinch,
    GestureRotate,
    GestureLongPress,
};
    
struct GestureConfig
{
    /// Type
    GestureType mType;
    /// Configuring Pan gesture
    uint32_t    mMinNumberOfTouches;
    uint32_t    mMaxNumberOfTouches;
    /// Configuring Tap gesture (single tap, double tap, ...)
    uint32_t    mNumberOfTapsRequired;
    /// Configuring Long press gesture
    float       mMinimumPressDuration;
};

class InputDeviceTouchImpl;

/// A touch input device.
/**
 * This input device provides support for touch screen/surface devices. The valid device buttons are defined
 * in the ::TouchButton enum. For each touch point, there is a boolean input (Touch*Down) showing if the
 * touch point is active, two float inputs (Touch*X, Touch*Y) showing the touch point's position, and a
 * float input (Touch*Pressure) showing the touch's pressure.
 *
 * Not all touch points must be numbered consecutively, i.e. point #2 may be down even though #0 and #1 are not.
 *
 * The number of simultaneously possible touch points is implementaion-dependent.
 *
 * This device is implemented on Android NDK.
 */
class GAINPUT_LIBEXPORT InputDeviceTouch : public InputDevice
{
public:
	/// Initializes the device.
	/**
	 * Instantiate the device using InputManager::CreateDevice().
	 *
	 * \param manager The input manager this device is managed by.
	 * \param device The ID of this device.
	 */
	InputDeviceTouch(InputManager& manager, DeviceId device, unsigned index, DeviceVariant variant);
	/// Shuts down the device.
	~InputDeviceTouch();

	/// Returns DT_TOUCH.
	DeviceType GetType() const { return DT_TOUCH; }
	DeviceVariant GetVariant() const;
	const char* GetTypeName() const { return "touch"; }
	bool IsValidButtonId(DeviceButtonId deviceButton) const;

	size_t GetAnyButtonDown(DeviceButtonSpec* outButtons, size_t maxButtonCount) const;

	size_t GetButtonName(DeviceButtonId deviceButton, char* buffer, size_t bufferLength) const;
	ButtonType GetButtonType(DeviceButtonId deviceButton) const;
	DeviceButtonId GetButtonByName(const char* name) const;

    InputState* GetNextInputState();
	void GetVirtualKeyboardInput(char* buffer, uint32_t inBufferLength) const;

	/// Returns the platform-specific implementation of this device.
	InputDeviceTouchImpl* GetPimpl() { return impl_; }

protected:
	void InternalUpdate(InputDeltaState* delta);

	DeviceState InternalGetState() const;

private:
	InputDeviceTouchImpl* impl_;

};

}

#endif

