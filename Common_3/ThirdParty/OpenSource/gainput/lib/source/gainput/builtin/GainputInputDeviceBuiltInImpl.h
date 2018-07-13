
#ifndef GAINPUTINPUTDEVICEBUILTINIMPL_H_
#define GAINPUTINPUTDEVICEBUILTINIMPL_H_

namespace gainput
{

class InputDeviceBuiltInImpl
{
public:
	virtual ~InputDeviceBuiltInImpl() { }
	virtual InputDevice::DeviceVariant GetVariant() const = 0;
	virtual InputDevice::DeviceState GetState() const { return InputDevice::DS_OK; }
	virtual void Update(InputDeltaState* delta) = 0;
	virtual bool IsValidButton(DeviceButtonId deviceButton) const = 0;
};

}

#endif

