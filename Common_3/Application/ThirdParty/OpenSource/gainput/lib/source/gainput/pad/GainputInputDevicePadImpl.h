
#ifndef GAINPUTINPUTDEVICEPADIMPL_H_
#define GAINPUTINPUTDEVICEPADIMPL_H_

namespace gainput
{

typedef struct ControllerFeedback
{
	uint16_t  vibration_left;
	uint16_t  vibration_right;
	uint8_t  r;    // led color
	uint8_t  g;    // led color
	uint8_t  b;    // led color
	uint32_t duration_ms;
	ControllerFeedback()
	{
		vibration_left = 0;
		vibration_right = 0;
		r = 0;
		g = 0;
		b = 0;
		duration_ms = 0;
	}
} ControllerFeedback;

class InputDevicePadImpl
{
public:
	virtual ~InputDevicePadImpl() { }
	virtual InputDevice::DeviceVariant GetVariant() const = 0;
	virtual InputDevice::DeviceState GetState() const { return InputDevice::DS_OK; }
	virtual void CheckConnection() {}
	virtual void Update(InputDeltaState* delta) = 0;
	virtual bool IsValidButton(DeviceButtonId deviceButton) const = 0;
	virtual bool Vibrate(float leftMotor, float rightMotor) = 0;
	virtual InputState* GetNextInputState() { return 0; }
	virtual const char* GetDeviceName() { return "Not Set"; }
	virtual bool SetRumbleEffect(float leftMotor, float rightMotor, uint32_t duration_ms, bool targetOwningDevice) { UNREF_PARAM(leftMotor); UNREF_PARAM(rightMotor); UNREF_PARAM(duration_ms); UNREF_PARAM(targetOwningDevice); return false; }
	virtual void SetLEDColor(uint8_t r, uint8_t g, uint8_t b) { UNREF_PARAM(r); UNREF_PARAM(g); UNREF_PARAM(b); }
	virtual void SetOnDeviceChangeCallBack(void(*onDeviceChange)(const char*, bool added, int controllerID)) { UNREF_PARAM(onDeviceChange); }
};

}

#endif

