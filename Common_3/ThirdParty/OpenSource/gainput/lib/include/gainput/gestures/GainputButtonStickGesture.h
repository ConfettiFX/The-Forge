
#ifndef GAINPUTBUTTONSTICKGESTURE_H_
#define GAINPUTBUTTONSTICKGESTURE_H_

#ifdef GAINPUT_ENABLE_BUTTON_STICK_GESTURE

namespace gainput
{

/// Buttons provided by the ButtonStickGesture.
enum ButtonStickAction
{
    ButtonStickAxis
};


class GAINPUT_LIBEXPORT ButtonStickGesture : public InputGesture
{
public:
	/// Initializes the gesture.
	ButtonStickGesture(InputManager& manager, DeviceId device, unsigned index, DeviceVariant variant);
	/// Uninitializes the gesture.
	~ButtonStickGesture();

	/// Sets up the gesture for operation with the given axes and buttons.
	void Initialize(DeviceId negativeAxisDevice, DeviceButtonId negativeAxis,
			DeviceId positiveAxisDevice, DeviceButtonId positiveAxis);

	bool IsValidButtonId(DeviceButtonId deviceButton) const { return deviceButton == ButtonStickAxis; }

	ButtonType GetButtonType(DeviceButtonId deviceButton) const { GAINPUT_UNUSED(deviceButton); GAINPUT_ASSERT(IsValidButtonId(deviceButton)); return BT_FLOAT; }

protected:
	void InternalUpdate(InputDeltaState* delta);

private:
	DeviceButtonSpec negativeAxis_;
	DeviceButtonSpec positiveAxis_;

};

}

#endif

#endif

