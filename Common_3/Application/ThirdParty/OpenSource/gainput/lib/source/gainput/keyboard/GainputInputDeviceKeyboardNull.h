
#ifndef GAINPUTINPUTDEVICEKEYBOARDNULL_H_
#define GAINPUTINPUTDEVICEKEYBOARDNULL_H_

#include "GainputInputDeviceKeyboardImpl.h"

namespace gainput
{

class InputDeviceKeyboardImplNull : public InputDeviceKeyboardImpl
{
public:
	InputDeviceKeyboardImplNull(InputManager& /*manager*/, DeviceId /*device*/)
	{
	}

	InputDevice::DeviceVariant GetVariant() const
	{
		return InputDevice::DV_NULL;
	}

	void Update(InputDeltaState* /*delta*/)
	{
	}

	bool IsTextInputEnabled() const { return false; }
	void SetTextInputEnabled(bool /*enabled*/) { }
	wchar_t* GetTextInput(uint32_t* count) { return NULL; }
};


}

#endif
