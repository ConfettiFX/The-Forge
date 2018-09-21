
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
	void HandleKey(int keycode, bool isPressed, char * inputChar = NULL);
	void HandleFlagsChanged(int keycode);

	InputDevice::DeviceState GetState() const { return deviceState_; }

	void Update(InputDeltaState* delta)
	{
		delta_ = delta;
		*state_ = nextState_;
	}

	bool IsTextInputEnabled() const { return textInputEnabled_; }
	void SetTextInputEnabled(bool enabled) { textInputEnabled_ = enabled; }

	char GetNextCharacter(gainput::DeviceButtonId buttonId)
	{
		if (!textBuffer_.CanGet())
		{
			return 0;
		}
		InputCharDesc currentDesc = textBuffer_.Get();
		
		//Removed buffered inputs for which we didn't call GetNextCharacter
		if(buttonId != gainput::InvalidDeviceButtonId && buttonId < gainput::KeyCount_)
		{
			while(currentDesc.buttonId != buttonId)
			{
				if (!textBuffer_.CanGet())
				{
					return 0;
				}
				currentDesc = textBuffer_.Get();
			}
		}
		
		//if button id was provided then we return the appropriate character
		//else we return the first buffered character
		return currentDesc.inputChar;
	}

	InputManager& manager_;
	InputDevice& device_;
	InputDevice::DeviceState deviceState_;
	bool textInputEnabled_;
	struct InputCharDesc
	{
		char inputChar;
		gainput::DeviceButtonId buttonId;
	};
	RingBuffer<GAINPUT_TEXT_INPUT_QUEUE_LENGTH, InputCharDesc> textBuffer_;
	HashMap<unsigned, DeviceButtonId> dialect_;
	InputState* state_;
	InputState* previousState_;
	InputState nextState_;
	InputDeltaState* delta_;
};

}

#endif
#endif
