
#ifndef GAINPUTINPUTDEVICEPADWIN_H_
#define GAINPUTINPUTDEVICEPADWIN_H_

// Cf. http://msdn.microsoft.com/en-us/library/windows/desktop/ee417005%28v=vs.85%29.aspx

#include "GainputWindows.h"
#include "../pad/GainputInputDevicePadImpl.h"
#include "GainputInputDevicePadWinDirectInput.h"
#include <XInput.h>

namespace gainput
{


const float MaxTriggerValue = 255.0f;
const float MaxAxisValue = 32767.0f;
const float MaxNegativeAxisValue = 32768.0f;
const float MaxMotorSpeed = 65535.0f;


class InputDevicePadImplWin : public InputDevicePadImpl
{
	enum PadType
	{
		Unresolved,
		XInput,
		DInput
	};

public:
	InputDevicePadImplWin(InputManager& manager, InputDevice& device, unsigned index, InputState& state, InputState& previousState) :
		manager_(manager),
		device_(device),
		state_(state),
		previousState_(previousState),
		deviceState_(InputDevice::DS_UNAVAILABLE),
		lastPacketNumber_((uint32_t)-1),
		hasBattery_(false),
		type_(PadType::Unresolved)
	{
		padIndex_ = index % 4;

#if 0
		XINPUT_BATTERY_INFORMATION xbattery;
		DWORD result = XInputGetBatteryInformation(padIndex, BATTERY_DEVTYPE_GAMEPAD, &xbattery);
		if (result == ERROR_SUCCESS)
		{
			hasBattery = (xbattery.BatteryType == BATTERY_TYPE_ALKALINE
				|| xbattery.BatteryType == BATTERY_TYPE_NIMH);
		}
#endif
	}

	InputDevice::DeviceVariant GetVariant() const
	{
		return InputDevice::DV_STANDARD;
	}

	void CheckConnection()
	{
		if (type_ != PadType::Unresolved)
			return;

		XINPUT_STATE xstate;
		DWORD result = XInputGetState(padIndex_, &xstate);

		if (result == ERROR_SUCCESS)
		{
			type_ = PadType::XInput;
			deviceState_ = InputDevice::DS_OK;

			DeviceId newId = manager_.GetNextId();
			device_.OverrideDeviceId(newId);
			manager_.AddDevice(newId, &device_);
		}
	}

	void Update(InputDeltaState* delta)
	{
		if (type_ == PadType::DInput)
		{
			//added
			if (fb.duration_ms)
			{
				if (getSystemTime() > fb.duration_ms)
				{
					SetRumbleEffect(0.0f, 0.0f, 0);
				}
			}

			dinpt.Update(delta, state_, device_);
			deviceState_ = InputDevice::DS_OK;
			return;
		}

		XINPUT_STATE xstate;
		DWORD result = XInputGetState(padIndex_, &xstate);

		//added
		if (fb.duration_ms)
		{
			if (getSystemTime() > fb.duration_ms)
			{
				SetRumbleEffect(0.0f, 0.0f, 0);
			}
		}

		if (result != ERROR_SUCCESS)
		{
			type_ = PadType::Unresolved;
			deviceState_ = InputDevice::DS_UNAVAILABLE;

			manager_.RemoveDevice(device_.GetDeviceId());
			device_.OverrideDeviceId(InvalidDeviceId);
			return;
		}

#if 0
		if (hasBattery)
		{
			XINPUT_BATTERY_INFORMATION xbattery;
			result = XInputGetBatteryInformation(padIndex, BATTERY_DEVTYPE_GAMEPAD, &xbattery);
			if (result == ERROR_SUCCESS)
			{
				if (xbattery.BatteryType == BATTERY_TYPE_ALKALINE
					|| xbattery.BatteryType == BATTERY_TYPE_NIMH)
				{
					if (xbattery.BatteryLevel == BATTERY_LEVEL_EMPTY
						|| xbattery.BatteryLevel == BATTERY_LEVEL_LOW)
					{
						deviceState = InputDevice::DS_LOW_BATTERY;
					}
				}
			}
		}
#endif

		if (xstate.dwPacketNumber == lastPacketNumber_ || lastPacketNumber_ == ULONG_MAX)
		{
			// Not changed
			lastPacketNumber_ = xstate.dwPacketNumber;
			return;
		}

		lastPacketNumber_ = xstate.dwPacketNumber;

		HandleButton(device_, state_, delta, PadButtonUp, (xstate.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP) != 0);
		HandleButton(device_, state_, delta, PadButtonDown, (xstate.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN) != 0);
		HandleButton(device_, state_, delta, PadButtonLeft, (xstate.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT) != 0);
		HandleButton(device_, state_, delta, PadButtonRight, (xstate.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) != 0);
		HandleButton(device_, state_, delta, PadButtonA, (xstate.Gamepad.wButtons & XINPUT_GAMEPAD_A) != 0);
		HandleButton(device_, state_, delta, PadButtonB, (xstate.Gamepad.wButtons & XINPUT_GAMEPAD_B) != 0);
		HandleButton(device_, state_, delta, PadButtonX, (xstate.Gamepad.wButtons & XINPUT_GAMEPAD_X) != 0);
		HandleButton(device_, state_, delta, PadButtonY, (xstate.Gamepad.wButtons & XINPUT_GAMEPAD_Y) != 0);
		HandleButton(device_, state_, delta, PadButtonStart, (xstate.Gamepad.wButtons & XINPUT_GAMEPAD_START) != 0);
		HandleButton(device_, state_, delta, PadButtonSelect, (xstate.Gamepad.wButtons & XINPUT_GAMEPAD_BACK) != 0);
		HandleButton(device_, state_, delta, PadButtonL3, (xstate.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB) != 0);
		HandleButton(device_, state_, delta, PadButtonR3, (xstate.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB) != 0);
		HandleButton(device_, state_, delta, PadButtonL1, (xstate.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) != 0);
		HandleButton(device_, state_, delta, PadButtonR1, (xstate.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) != 0);
        HandleButton(device_, state_, delta, PadButtonL2, xstate.Gamepad.bLeftTrigger != 0);
        HandleButton(device_, state_, delta, PadButtonR2, xstate.Gamepad.bRightTrigger != 0);

		HandleAxis(device_, state_, delta, PadButtonAxis4, float(xstate.Gamepad.bLeftTrigger)/MaxTriggerValue);
		HandleAxis(device_, state_, delta, PadButtonAxis5, float(xstate.Gamepad.bRightTrigger)/MaxTriggerValue);
		HandleAxis(device_, state_, delta, PadButtonLeftStickX, GetAxisValue(xstate.Gamepad.sThumbLX));
		HandleAxis(device_, state_, delta, PadButtonLeftStickY, GetAxisValue(xstate.Gamepad.sThumbLY));
		HandleAxis(device_, state_, delta, PadButtonRightStickX, GetAxisValue(xstate.Gamepad.sThumbRX));
		HandleAxis(device_, state_, delta, PadButtonRightStickY, GetAxisValue(xstate.Gamepad.sThumbRY));
	}

	InputDevice::DeviceState GetState() const
	{
		return deviceState_;
	}

	bool IsValidButton(DeviceButtonId deviceButton) const
	{
		return deviceButton < PadButtonAxis4 || (deviceButton >= PadButtonStart && deviceButton <= PadButtonR3);
	}

	bool Vibrate(float leftMotor, float rightMotor)
	{
		if (type_ == PadType::XInput)
		{
			GAINPUT_ASSERT(leftMotor >= 0.0f && leftMotor <= 1.0f);
			GAINPUT_ASSERT(rightMotor >= 0.0f && rightMotor <= 1.0f);
			XINPUT_VIBRATION xvibration;
			xvibration.wLeftMotorSpeed = static_cast<WORD>(leftMotor*MaxMotorSpeed);
			xvibration.wRightMotorSpeed = static_cast<WORD>(rightMotor*MaxMotorSpeed);
			DWORD result = XInputSetState(padIndex_, &xvibration);
			return result == ERROR_SUCCESS;
		}

		return false;
	}

	//added
	const char* GetDeviceName()
	{
		enum
		{
			InputTypeNone,
			InputTypeXboxOne,
			InputTypeXbox360
		};

		switch (type_)
		{
		case PadType::XInput:
			switch (InputDevicePadWinDirectInput::GetXInputType(padIndex_))
			{
			case InputTypeXboxOne:
				return "Xbox One";
			case InputTypeXbox360:
				return "Xbox 360";
			default:
				return "Xbox";
			}
		case PadType::DInput:
			return dinpt.GetDeviceName();
		default:
			return "Not Set";
		}
	}

	virtual bool SetRumbleEffect(float leftMotor, float rightMotor, uint32_t duration_ms)
	{
		fb.vibration_left = uint16_t(leftMotor * MaxMotorSpeed);
		fb.vibration_right = uint16_t(rightMotor * MaxMotorSpeed);
		if(duration_ms)
			fb.duration_ms = getSystemTime() + duration_ms;
		else
			fb.duration_ms = 0;

		switch (type_)
		{
		case PadType::XInput:
		{
			GAINPUT_ASSERT(leftMotor >= 0.0f && leftMotor <= 1.0f);
			GAINPUT_ASSERT(rightMotor >= 0.0f && rightMotor <= 1.0f);
			XINPUT_VIBRATION xvibration;
			xvibration.wLeftMotorSpeed = static_cast<WORD>(fb.vibration_left);
			xvibration.wRightMotorSpeed = static_cast<WORD>(fb.vibration_right);
			DWORD result = XInputSetState(padIndex_, &xvibration);
			return result == ERROR_SUCCESS;
		}
		case PadType::DInput:
			return dinpt.SetRumbleEffectFeedback(fb);
		default:
			return false;
		}
	}

	// targetOwningDevice discarded/ignored as it is only used for mobile phones... 
	bool SetRumbleEffect(float leftMotor, float rightMotor, uint32_t duration_ms, bool targetOwningDevice)
	{
		UNREF_PARAM(targetOwningDevice); 
		return SetRumbleEffect(leftMotor, rightMotor, duration_ms);
	}

	virtual void SetLEDColor(uint8_t r, uint8_t g, uint8_t b)
	{
		if (type_ == PadType::DInput)
		{
			ControllerFeedback feedBack;
			feedBack.r = r;
			feedBack.g = g;
			feedBack.b = b;
			dinpt.SetLedColorFeedback(feedBack);
		}
	}


	// dinput utils

	void ForceDInput(unsigned index)
	{
		type_ = PadType::DInput;
		padIndex_ = index;
		dinpt.Init(padIndex_);
	}

	void ParseMessage(void* lparam)
	{
		dinpt.ParseMessage(lparam);
	}

private:
	InputManager& manager_;
	InputDevice& device_;
	InputState& state_;
	InputState& previousState_;
	InputDevice::DeviceState deviceState_;
	unsigned padIndex_;
	DWORD lastPacketNumber_;
	bool hasBattery_;
	//added
	PadType type_;
	InputDevicePadWinDirectInput dinpt;
	ControllerFeedback fb;

    static float GetAxisValue(SHORT value)
    {
        if (value < 0)
        {
            return float(value) / MaxNegativeAxisValue;
        }
        else
        {
            return float(value) / MaxAxisValue;
        }
    }

};

}

#endif

