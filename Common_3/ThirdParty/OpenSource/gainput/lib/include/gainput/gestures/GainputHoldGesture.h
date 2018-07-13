
#ifndef GAINPUTHOLDGESTURE_H_
#define GAINPUTHOLDGESTURE_H_

#ifdef GAINPUT_ENABLE_HOLD_GESTURE

namespace gainput
{

/// Buttons provided by the HoldGesture.
enum HoldAction
{
	HoldTriggered		///< The button that triggers after holding for the given time.
};

/// A hold-to-trigger gesture.
/**
 * This gesture, mainly meant for touch devices, triggers after the specified button has been down for at least
 * the specified amount of time. Its button ::HoldTriggered is then either active for one frame or as long as
 * the source button is down.
 *
 * After instantiating the gesture like any other input device, call one of the Initialize() functions to properly
 * set it up.
 *
 * In order for this gesture to be available, Gainput must be built with \c GAINPUT_ENABLE_ALL_GESTURES or
 * \c GAINPUT_ENABLE_HOLD_GESTURE defined.
 *
 * \sa Initialize
 * \sa InputManager::CreateDevice
 */
class GAINPUT_LIBEXPORT HoldGesture : public InputGesture
{
public:
	/// Initializes the gesture.
	HoldGesture(InputManager& manager, DeviceId device, unsigned index, DeviceVariant variant);
	/// Uninitializes the gesture.
	~HoldGesture();

	/// Sets up the gesture for operation without position checking.
	/**
	 * \param actionButtonDevice ID of the input device containing the action button.
	 * \param actionButton ID of the device button to be used as the action button.
	 * \param oneShot Specifies if the gesture triggers only once after the given time or if it triggers as long as the source button is down.
	 * \param timeSpan Time in milliseconds the user needs to hold in order to trigger the gesture.
	 */
	void Initialize(DeviceId actionButtonDevice, DeviceButtonId actionButton, bool oneShot = true, uint64_t timeSpan = 800);
	/// Sets up the gesture for operation with position checking, i.e. the user may not move the mouse too far while holding.
	/**
	 * \param actionButtonDevice ID of the input device containing the action button.
	 * \param actionButton ID of the device button to be used as the action button.
	 * \param xAxisDevice ID of the input device containing the X coordinate of the pointer.
	 * \param xAxis ID of the device button/axis to be used for the X coordinate of the pointer.
	 * \param xTolerance The amount the pointer may travel in the X coordinate to still be valid.
	 * \param yAxisDevice ID of the input device containing the Y coordinate of the pointer.
	 * \param yAxis ID of the device button/axis to be used for the Y coordinate of the pointer.
	 * \param yTolerance The amount the pointer may travel in the Y coordinate to still be valid.
	 * \param oneShot Specifies if the gesture triggers only once after the given time or if it triggers as long as the source button is down.
	 * \param timeSpan Time in milliseconds the user needs to hold in order to trigger the gesture.
	 */
	void Initialize(DeviceId actionButtonDevice, DeviceButtonId actionButton, 
			DeviceId xAxisDevice, DeviceButtonId xAxis, float xTolerance, 
			DeviceId yAxisDevice, DeviceButtonId yAxis, float yTolerance, 
			bool oneShot = true, 
			uint64_t timeSpan = 800);

	bool IsValidButtonId(DeviceButtonId deviceButton) const { return deviceButton == HoldTriggered; }

	ButtonType GetButtonType(DeviceButtonId deviceButton) const { GAINPUT_UNUSED(deviceButton); GAINPUT_ASSERT(IsValidButtonId(deviceButton)); return BT_BOOL; }

protected:
	void InternalUpdate(InputDeltaState* delta);

private:
	DeviceButtonSpec actionButton_;
	DeviceButtonSpec xAxis_;
	float xTolerance_;
	DeviceButtonSpec yAxis_;
	float yTolerance_;

	bool oneShot_;
	bool oneShotReset_;
	uint64_t timeSpan_;
	uint64_t firstDownTime_;

	float firstDownX_;
	float firstDownY_;
};

}

#endif

#endif

