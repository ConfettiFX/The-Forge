
#ifndef GAINPUTINPUTDEVICEPADIMPL_H_
#define GAINPUTINPUTDEVICEPADIMPL_H_

namespace gainput
{

class InputDevicePadImpl
{
public:
	virtual ~InputDevicePadImpl() { }
	virtual InputDevice::DeviceVariant GetVariant() const = 0;
	virtual InputDevice::DeviceState GetState() const { return InputDevice::DS_OK; }
	virtual void Update(InputDeltaState* delta) = 0;
	virtual bool IsValidButton(DeviceButtonId deviceButton) const = 0;
	virtual bool Vibrate(float leftMotor, float rightMotor) = 0;
	virtual InputState* GetNextInputState() { return 0; }
	virtual const char* GetDeviceName() { return "Not Set"; }
	virtual bool SetRumbleEffect(float leftMotor, float rightMotor, uint32_t duration_ms) { return false; }
	virtual void SetLEDColor(uint8_t r, uint8_t g, uint8_t b) { }
};

}

#endif

