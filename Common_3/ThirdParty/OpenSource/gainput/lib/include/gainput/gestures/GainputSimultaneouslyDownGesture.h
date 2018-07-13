
#ifndef GAINPUTSIMULTANEOUSLYDOWNGESTURE_H_
#define GAINPUTSIMULTANEOUSLYDOWNGESTURE_H_

#ifdef GAINPUT_ENABLE_SIMULTANEOUSLY_DOWN_GESTURE

namespace gainput
{

/// Buttons provided by the SimultaneouslyDownGesture.
enum SimultaneouslyDownAction
{
	SimultaneouslyDownTriggered		///< The button triggered by double-clicking.
};

/// A gesture that tracks if a number of buttons is down simultaneously.
/**
 * This gesture can be used to detect if multiple buttons are down at the same time. Its only 
 * device button ::SimultaneouslyDownTriggered is true while all buttons provided through AddButton() 
 * are down.
 *
 * After instantiating the gesture like any other input device, call AddButton() as often as necessary
 * to properly set it up.
 *
 * In order for this gesture to be available, Gainput must be built with \c GAINPUT_ENABLE_ALL_GESTURES or
 * \c GAINPUT_ENABLE_SIMULTANEOUSLY_DOWN_GESTURE defined.
 *
 * \sa AddButton
 * \sa InputManager::CreateDevice
 */
class GAINPUT_LIBEXPORT SimultaneouslyDownGesture : public InputGesture
{
public:
	/// Initializes the gesture.
	SimultaneouslyDownGesture(InputManager& manager, DeviceId device, unsigned index, DeviceVariant variant);
	/// Uninitializes the gesture.
	~SimultaneouslyDownGesture();

	/// Adds the given button as a button to check.
	/**
	 * \param device ID of the input device containing the button to be checked.
	 * \param button ID of the device button to be checked.
	 */
	void AddButton(DeviceId device, DeviceButtonId button);

	/// Removes all buttons previously registered through AddButton().
	void ClearButtons();

	bool IsValidButtonId(DeviceButtonId deviceButton) const { return deviceButton == SimultaneouslyDownTriggered; }

	ButtonType GetButtonType(DeviceButtonId deviceButton) const { GAINPUT_UNUSED(deviceButton); GAINPUT_ASSERT(IsValidButtonId(deviceButton)); return BT_BOOL; }

protected:
	void InternalUpdate(InputDeltaState* delta);

private:
	Array<DeviceButtonSpec> buttons_;

};

}

#endif

#endif

