
#ifndef GAINPUTINPUTSTATE_H_
#define GAINPUTINPUTSTATE_H_

namespace gainput
{

/// State of an input device.
class GAINPUT_LIBEXPORT InputState
{
public:
	/// Initializes the state.
	/**
	 * \param allocator The allocator to be used for all memory allocations.
	 * \param buttonCount The maximum number of device buttons.
	 */
	InputState(Allocator& allocator, unsigned int buttonCount);
	/// Unitializes the state.
	~InputState();

	/// Returns the number of buttons in this state.
	/**
	 * Note that not all buttons may be valid.
	 *
	 * \sa InputDevice::IsValidButtonId()
	 */
	unsigned GetButtonCount() const { return buttonCount_; }

	/// Returns the bool state of the given device button.
	bool GetBool(DeviceButtonId buttonId) const { return buttons_[buttonId].b; }
	/// Sets the bool state of the given device button.
	void Set(DeviceButtonId buttonId, bool value) { buttons_[buttonId].b = value; }
	/// Returns the float state of the given device button.
	float GetFloat(DeviceButtonId buttonId) const { return buttons_[buttonId].f; }
	/// Sets the float state of the given device button.
	void Set(DeviceButtonId buttonId, float value) { buttons_[buttonId].f = value; }

	/// Sets the states of all buttons in this input state to the states of all buttons in the given input state.
	InputState& operator=(const InputState& other);

private:
	Allocator& allocator_;
	unsigned int buttonCount_;

	struct Button
	{
		union
		{
			bool b;
			float f;
		};
	};

	Button* buttons_;
};

}

#endif

