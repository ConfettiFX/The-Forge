
#ifndef GAINPUTPINCHGESTURE_H_
#define GAINPUTPINCHGESTURE_H_

#ifdef GAINPUT_ENABLE_PINCH_GESTURE

namespace gainput
{

/// Buttons provided by the PinchGesture.
enum PinchAction
{
	PinchTriggered,		///< The button that triggers when both pinch buttons are down.
	PinchScale		///< The current pinch scale value if pinching is active.
};

/// A multi-touch pinch-to-scale gesture.
/**
 * This gesture, mainly meant for multi-touch devices, triggers (::PinchTriggered is down) when both specified 
 * source buttons are down. It will then determine the distance between the two 2D touch points and report 
 * any change in distance as a factor of the initial distance using ::PinchScale.
 *
 * After instantiating the gesture like any other input device, call Initialize() to properly
 * set it up.
 *
 * In order for this gesture to be available, Gainput must be built with \c GAINPUT_ENABLE_ALL_GESTURES or
 * \c GAINPUT_ENABLE_PINCH_GESTURE defined.
 *
 * \sa Initialize
 * \sa InputManager::CreateDevice
 */
class GAINPUT_LIBEXPORT PinchGesture : public InputGesture
{
public:
	/// Initializes the gesture.
	PinchGesture(InputManager& manager, DeviceId device, unsigned index, DeviceVariant variant);
	/// Uninitializes the gesture.
	~PinchGesture();

	/// Sets up the gesture for operation with the given axes and buttons.
	/**
	 * \param downDevice ID of the input device containing the first touch button.
	 * \param downButton ID of the device button to be used as the first touch button.
	 * \param xAxisDevice ID of the input device containing the X coordinate of the first touch point.
	 * \param xAxis ID of the device button/axis to be used for the X coordinate of the first touch point.
	 * \param yAxisDevice ID of the input device containing the Y coordinate of the first touch point.
	 * \param yAxis ID of the device button/axis to be used for the Y coordinate of the first touch point.
	 * \param down2Device ID of the input device containing the second touch button.
	 * \param downButton2 ID of the device button to be used as the second touch button.
	 * \param xAxis2Device ID of the input device containing the X coordinate of the second touch point.
	 * \param xAxis2 ID of the device button/axis to be used for the X coordinate of the second touch point.
	 * \param yAxis2Device ID of the input device containing the Y coordinate of the second touch point.
	 * \param yAxis2 ID of the device button/axis to be used for the Y coordinate of the second touch point.
	 */
	void Initialize(DeviceId downDevice, DeviceButtonId downButton, 
			DeviceId xAxisDevice, DeviceButtonId xAxis, 
			DeviceId yAxisDevice, DeviceButtonId yAxis, 
			DeviceId down2Device, DeviceButtonId downButton2, 
			DeviceId xAxis2Device, DeviceButtonId xAxis2, 
			DeviceId yAxis2Device, DeviceButtonId yAxis2);

	bool IsValidButtonId(DeviceButtonId deviceButton) const { return deviceButton == PinchTriggered || deviceButton == PinchScale; }

	ButtonType GetButtonType(DeviceButtonId deviceButton) const { GAINPUT_ASSERT(IsValidButtonId(deviceButton)); return deviceButton == PinchTriggered ? BT_BOOL : BT_FLOAT; }

protected:
	void InternalUpdate(InputDeltaState* delta);

private:
	DeviceButtonSpec downButton_;
	DeviceButtonSpec xAxis_;
	DeviceButtonSpec yAxis_;
	DeviceButtonSpec downButton2_;
	DeviceButtonSpec xAxis2_;
	DeviceButtonSpec yAxis2_;

	bool pinching_;
	float initialDistance_;
};

}

#endif

#endif

