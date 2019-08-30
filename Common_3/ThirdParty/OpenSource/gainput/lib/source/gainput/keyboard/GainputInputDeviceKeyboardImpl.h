
#ifndef GAINPUTINPUTDEVICEKEYBOARDIMPL_H_
#define GAINPUTINPUTDEVICEKEYBOARDIMPL_H_

namespace gainput
{

class InputDeviceKeyboardImpl
{
public:
	virtual ~InputDeviceKeyboardImpl() { }
	virtual InputDevice::DeviceVariant GetVariant() const = 0;
	virtual InputDevice::DeviceState GetState() const { return InputDevice::DS_OK; }
	virtual void Update(InputDeltaState* delta) = 0;
	virtual InputState* GetNextInputState() { return 0; }

	virtual bool IsTextInputEnabled() const = 0;
	virtual void SetTextInputEnabled(bool enabled) = 0;
	virtual wchar_t* GetTextInput(uint32_t* count) = 0;
	virtual void ClearButtons() { return; }
};

}

#endif

