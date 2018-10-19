
#ifndef GAINPUTINPUTDEVICEMOUSEIMPL_H_
#define GAINPUTINPUTDEVICEMOUSEIMPL_H_

namespace gainput
{

class InputDeviceMouseImpl
{
public:
	virtual ~InputDeviceMouseImpl() { }
	virtual InputDevice::DeviceVariant GetVariant() const = 0;
	virtual InputDevice::DeviceState GetState() const { return InputDevice::DS_OK; }
	virtual void Update(InputDeltaState* delta) = 0;
	virtual void WarpMouse(const float&x, const float& y) {}
	virtual InputState * GetNextInputState() { return NULL; }
};

}

#endif

