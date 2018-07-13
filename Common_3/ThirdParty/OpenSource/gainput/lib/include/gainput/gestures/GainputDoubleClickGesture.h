
#ifndef GAINPUTDOUBLECLICKGESTURE_H_
#define GAINPUTDOUBLECLICKGESTURE_H_

#ifdef GAINPUT_ENABLE_DOUBLE_CLICK_GESTURE

namespace gainput
{

/// Buttons provided by the DoubleClickGesture.
enum DoubleClickAction
{
	DoubleClickTriggered		///< The button triggered by double-clicking.
};

/// A double-click gesture.
/**
 * This gesture implements the classic double-click functionality. Its only device button ::DoubleClickTriggered is
 * true for one frame after the specified button has been active for a specified number of times in a given
 * time frame. It's also possible to disallow the pointer from travelling too far between and during clicks.
 *
 * After instantiating the gesture like any other input device, call one of the Initialize() functions to properly
 * set it up.
 *
 * In order for this gesture to be available, Gainput must be built with \c GAINPUT_ENABLE_ALL_GESTURES or
 * \c GAINPUT_ENABLE_DOUBLE_CLICK_GESTURE defined.
 *
 * \sa Initialize
 * \sa SetClicksTargetCount
 * \sa InputManager::CreateDevice
 */
class GAINPUT_LIBEXPORT DoubleClickGesture : public InputGesture
{
public:
	/// Initializes the gesture.
	DoubleClickGesture(InputManager& manager, DeviceId device, unsigned index, DeviceVariant variant);
	/// Uninitializes the gesture.
	~DoubleClickGesture();

	/// Sets up the gesture for operation without position checking.
	/**
	 * \param actionButtonDevice ID of the input device containing the action button.
	 * \param actionButton ID of the device button to be used as the action button.
	 * \param timeSpan Allowed time between clicks in milliseconds.
	 */
	void Initialize(DeviceId actionButtonDevice, DeviceButtonId actionButton, uint64_t timeSpan = 300);
	/// Sets up the gesture for operation with position checking, i.e. the user may not move the mouse too far between clicks.
	/**
	 * \param actionButtonDevice ID of the input device containing the action button.
	 * \param actionButton ID of the device button to be used as the action button.
	 * \param xAxisDevice ID of the input device containing the X coordinate of the pointer.
	 * \param xAxis ID of the device button/axis to be used for the X coordinate of the pointer.
	 * \param xTolerance The amount the pointer may travel in the X coordinate to still be valid.
	 * \param yAxisDevice ID of the input device containing the Y coordinate of the pointer.
	 * \param yAxis ID of the device button/axis to be used for the Y coordinate of the pointer.
	 * \param yTolerance The amount the pointer may travel in the Y coordinate to still be valid.
	 * \param timeSpan Allowed time between clicks in milliseconds.
	 */
	void Initialize(DeviceId actionButtonDevice, DeviceButtonId actionButton, 
			DeviceId xAxisDevice, DeviceButtonId xAxis, float xTolerance, 
			DeviceId yAxisDevice, DeviceButtonId yAxis, float yTolerance, 
			uint64_t timeSpan = 300);

	/// Sets the number of clicks to trigger an action.
	/**
	 * \param count The number of clicks that will trigger this gesture; the default is 2, i.e. double-click.
	 */
	void SetClicksTargetCount(unsigned count) { clicksTargetCount_ = count; }

	bool IsValidButtonId(DeviceButtonId deviceButton) const { return deviceButton == DoubleClickTriggered; }

	ButtonType GetButtonType(DeviceButtonId deviceButton) const { GAINPUT_UNUSED(deviceButton); GAINPUT_ASSERT(IsValidButtonId(deviceButton)); return BT_BOOL; }

protected:
	void InternalUpdate(InputDeltaState* delta);

private:
	DeviceButtonSpec actionButton_;
	DeviceButtonSpec xAxis_;
	float xTolerance_;
	DeviceButtonSpec yAxis_;
	float yTolerance_;

	uint64_t timeSpan_;
	uint64_t firstClickTime_;

	float firstClickX_;
	float firstClickY_;

	unsigned clicksRegistered_;
	unsigned clicksTargetCount_;
};

}

#endif

#endif

