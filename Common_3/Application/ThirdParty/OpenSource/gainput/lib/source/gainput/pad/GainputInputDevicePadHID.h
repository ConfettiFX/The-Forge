
#ifndef GAINPUTINPUTDEVICEPADHID_H_
#define GAINPUTINPUTDEVICEPADHID_H_

#include "GainputInputDevicePadImpl.h"
#include "../hid/GainputHID.h"

namespace gainput
{

class InputDevicePadImplHID : public InputDevicePadImpl
{
public:
	InputDevicePadImplHID(InputManager& manager, InputDevice& device, unsigned devId) :
		devId_((uint8_t)devId)
	{
	UNREF_PARAM(manager); 
	UNREF_PARAM(device); 
	}

	InputDevice::DeviceVariant GetVariant() const
	{
		return InputDevice::DV_STANDARD;
	}

	void Update(InputDeltaState* delta)
	{
		UNREF_PARAM(delta); 
		// NOTE: Currently handled via HIDUpdateDevices
		//HIDUpdate(devId_, delta);
	}

	bool IsValidButton(DeviceButtonId deviceButton) const
	{
		UNREF_PARAM(deviceButton); 
		// Needed? Buttons should be handled via the change events
		return false;
	}

	bool Vibrate(float leftMotor, float rightMotor)
	{
		HIDDoRumble(devId_, leftMotor, rightMotor, 0);
		return true;
	}

	const char* GetDeviceName()
	{
		return HIDControllerName(devId_);
	}

	bool SetRumbleEffect(float leftMotor, float rightMotor, uint32_t duration_ms, bool)
	{
		HIDDoRumble(devId_, leftMotor, rightMotor, duration_ms);
		return true;
	}

	void SetLEDColor(uint8_t r, uint8_t g, uint8_t b)
	{
		HIDSetLights(devId_, r, g, b);
	}

	void SetOnDeviceChangeCallBack(void(*onDeviceChange)(const char*, bool added, int controllerIndex))
	{
		UNREF_PARAM(onDeviceChange); 
		// handled by InputSystem
	}

private:
	uint8_t devId_;
};

}

#endif
