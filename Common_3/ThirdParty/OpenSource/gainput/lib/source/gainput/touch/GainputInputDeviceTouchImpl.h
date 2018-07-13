
#ifndef GAINPUTINPUTDEVICETOUCHIMPL_H_
#define GAINPUTINPUTDEVICETOUCHIMPL_H_

namespace gainput
{

class InputDeviceTouchImpl
{
public:
	virtual ~InputDeviceTouchImpl() { }
	virtual InputDevice::DeviceVariant GetVariant() const = 0;
	virtual InputDevice::DeviceState GetState() const = 0;
	virtual void Update(InputDeltaState* delta) = 0;
    virtual bool SupportsPressure() const { return false; }
	virtual InputState* GetNextInputState() { return 0; }
};

}

#endif

