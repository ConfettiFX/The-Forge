
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

	InputDevice::DeviceVariant GetVariant() const override
	{
		return InputDevice::DV_RAW;
	}
	void HandleKey(int keycode, bool isPressed, char * inputChar = NULL);
	void HandleFlagsChanged(int keycode);

	InputDevice::DeviceState GetState() const override { return deviceState_; }

	void Update(InputDeltaState* delta) override
	{
		delta_ = delta;
		*state_ = nextState_;
		textCount_ = 0;
        memset(textBuffer_, 0, sizeof(textBuffer_));
	}

	bool IsTextInputEnabled() const override { return textInputEnabled_; }
	void SetTextInputEnabled(bool enabled) override { textInputEnabled_ = enabled; }
	wchar_t* GetTextInput(uint32_t* count) override
	{
		*count = textCount_;
		return textBuffer_;
	}

	virtual InputState * GetNextInputState() override {
		return &nextState_;
	}

	InputManager& manager_;
	InputDevice& device_;
	InputDevice::DeviceState deviceState_;
	bool textInputEnabled_;
	wchar_t textBuffer_[GAINPUT_TEXT_INPUT_QUEUE_LENGTH];
	uint32_t textCount_;
	HashMap<unsigned, DeviceButtonId> dialect_;
	InputState* state_;
	InputState* previousState_;
	InputState nextState_;
	InputDeltaState* delta_;
};

}

#endif
#endif
