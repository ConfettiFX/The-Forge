
#ifndef GAINPUTINPUTDEVICEKEYBOARDMAC_H_
#define GAINPUTINPUTDEVICEKEYBOARDMAC_H_

#ifdef __APPLE__

#include "GainputInputDeviceKeyboardImpl.h"

namespace gainput
{

class InputDeviceKeyboardImplMac : public InputDeviceKeyboardImpl
{
public:
	InputDeviceKeyboardImplMac(InputManager& manager, InputDevice& device, InputState& state, InputState& previousState);
	~InputDeviceKeyboardImplMac();

	InputDevice::DeviceVariant GetVariant() const
	{
		return InputDevice::DV_RAW;
	}

	InputDevice::DeviceState GetState() const { return deviceState_; }

	void Update(InputDeltaState* delta)
	{
		delta_ = delta;
		*state_ = nextState_;
	}

	bool IsTextInputEnabled() const { return textInputEnabled_; }
	void SetTextInputEnabled(bool enabled) { textInputEnabled_ = enabled; }

	char GetNextCharacter()
	{
		if (!textBuffer_.CanGet())
		{
			return 0;
		}
		return textBuffer_.Get();
	}

	InputManager& manager_;
	InputDevice& device_;
	InputDevice::DeviceState deviceState_;
	bool textInputEnabled_;
	RingBuffer<GAINPUT_TEXT_INPUT_QUEUE_LENGTH, char> textBuffer_;
	HashMap<unsigned, DeviceButtonId> dialect_;
	InputState* state_;
	InputState* previousState_;
	InputState nextState_;
	InputDeltaState* delta_;

private:
	void* ioManager_;
};

}

#endif
#endif
