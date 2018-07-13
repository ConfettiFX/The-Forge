
#ifndef GAINPUTTAPGESTURE_H_
#define GAINPUTTAPGESTURE_H_

#ifdef GAINPUT_ENABLE_TAP_GESTURE

namespace gainput
{

/// Buttons provided by the TapGesture.
enum TapAction
{
	TapTriggered		///< The button that is triggered by tapping.
};

/// A tap-to-trigger gesture.
/**
 * This gesture, mainly meant for touch devices, triggers after the specified button has been down and released
 * during the specified time frame. If the button is down for a longer time, no action is triggered.
 *
 * After instantiating the gesture like any other input device, call Initialize() to properly
 * set it up.
 *
 * In order for this gesture to be available, Gainput must be built with \c GAINPUT_ENABLE_ALL_GESTURES or
 * \c GAINPUT_ENABLE_TAP_GESTURE defined.
 *
 * \sa Initialize
 * \sa InputManager::CreateDevice
 */
class GAINPUT_LIBEXPORT TapGesture : public InputGesture
{
public:
	/// Initializes the gesture.
	TapGesture(InputManager& manager, DeviceId device, unsigned index, DeviceVariant variant);
	/// Uninitializes the gesture.
	~TapGesture();

	/// Sets up the gesture.
	/**
	 * \param actionButtonDevice ID of the input device containing the action button.
	 * \param actionButton ID of the device button to be used as the action button.
	 * \param timeSpan Time in milliseconds the user may hold at most.
	 */
	void Initialize(DeviceId actionButtonDevice, DeviceButtonId actionButton, uint64_t timeSpan = 500);

	bool IsValidButtonId(DeviceButtonId deviceButton) const { return deviceButton == TapTriggered; }

	ButtonType GetButtonType(DeviceButtonId deviceButton) const { GAINPUT_UNUSED(deviceButton); GAINPUT_ASSERT(IsValidButtonId(deviceButton)); return BT_BOOL; }

protected:
	void InternalUpdate(InputDeltaState* delta);

private:
	DeviceButtonSpec actionButton_;

	uint64_t timeSpan_;
	uint64_t firstDownTime_;

};

}

#endif

#endif

