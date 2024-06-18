
#include "../../../include/gainput/gainput.h"
#include "../../../include/gainput/GainputDebugRenderer.h"

#include "GainputInputDevicePadImpl.h"
#include "../../../include/gainput/GainputInputDeltaState.h"
#include "../../../include/gainput/GainputHelpers.h"
#include "../../../include/gainput/GainputLog.h"

#if defined(GAINPUT_PLATFORM_LINUX)
	#include "../linux/GainputInputDevicePadLinux.h"
	#include "GainputInputDevicePadHID.h"
#elif defined(GAINPUT_PLATFORM_WIN)
	#include "../windows/GainputInputDevicePadWin.h"
	#include "GainputInputDevicePadHID.h"
#elif defined(GAINPUT_PLATFORM_IOS) || defined(GAINPUT_PLATFORM_TVOS)
#include "../apple/GainputInputDevicePadAppleGCKit.h"
#elif defined(GAINPUT_PLATFORM_MAC)
	#include "../apple/GainputInputDevicePadAppleGCKit.h"
	#include "../apple/GainputInputDevicePadMac.h"
	#include "GainputInputDevicePadHID.h"
#elif defined(GAINPUT_PLATFORM_ANDROID)
	#include "../android/GainputInputDevicePadAndroid.h" 
#elif defined(GAINPUT_PLATFORM_QUEST)
	#include "../quest/GainputInputDevicePadQuest.h" 
#elif defined (GAINPUT_PLATFORM_XBOX_ONE)
	#include "../../../../../../../../../Xbox/Common_3/Application/Input/GainputInputDevicePadXboxOne.h"
#elif defined(GAINPUT_PLATFORM_NX64)
#include "../../../../../../../../../Switch/Common_3/Application/Input/GainputInputDevicePadNX.h"
#elif defined(GAINPUT_PLATFORM_GGP)
	#include "../../../../../../../../Stadia/Common_3/OS/Input/GainputInputDevicePadGGP.h"
#elif defined(GAINPUT_PLATFORM_ORBIS)
#include "../../../../../../../../../PS4/Common_3/Application/Input/GainputInputDevicePadOrbis.h"
#elif defined(GAINPUT_PLATFORM_PROSPERO)
#include "../../../../../../../../../Prospero/Common_3/Application/Input/GainputInputDevicePadProspero.h"
#endif

#include "GainputInputDevicePadNull.h"

namespace gainput
{

namespace
{
struct DeviceButtonInfo
{
	ButtonType type;
	const char* name;
};

DeviceButtonInfo deviceButtonInfos[] =
{
		{ BT_FLOAT, "pad_left_stick_x" },
		{ BT_FLOAT, "pad_left_stick_y" },
		{ BT_FLOAT, "pad_right_stick_x" },
		{ BT_FLOAT, "pad_right_stick_y" },
		{ BT_FLOAT, "pad_axis_4" },
		{ BT_FLOAT, "pad_axis_5" },
		{ BT_FLOAT, "pad_axis_6" },
		{ BT_FLOAT, "pad_axis_7" },
		{ BT_FLOAT, "pad_axis_8" },
		{ BT_FLOAT, "pad_axis_9" },
		{ BT_FLOAT, "pad_axis_10" },
		{ BT_FLOAT, "pad_axis_11" },
		{ BT_FLOAT, "pad_axis_12" },
		{ BT_FLOAT, "pad_axis_13" },
		{ BT_FLOAT, "pad_axis_14" },
		{ BT_FLOAT, "pad_axis_15" },
		{ BT_FLOAT, "pad_axis_16" },
		{ BT_FLOAT, "pad_axis_17" },
		{ BT_FLOAT, "pad_axis_18" },
		{ BT_FLOAT, "pad_axis_19" },
		{ BT_FLOAT, "pad_axis_20" },
		{ BT_FLOAT, "pad_axis_21" },
		{ BT_FLOAT, "pad_axis_22" },
		{ BT_FLOAT, "pad_axis_23" },
		{ BT_FLOAT, "pad_axis_24" },
		{ BT_FLOAT, "pad_axis_25" },
		{ BT_FLOAT, "pad_axis_26" },
		{ BT_FLOAT, "pad_axis_27" },
		{ BT_FLOAT, "pad_axis_28" },
		{ BT_FLOAT, "pad_axis_29" },
		{ BT_FLOAT, "pad_axis_30" },
		{ BT_FLOAT, "pad_axis_31" },
		{ BT_FLOAT, "pad_acceleration_x" },
		{ BT_FLOAT, "pad_acceleration_y" },
		{ BT_FLOAT, "pad_acceleration_z" },
		{ BT_FLOAT, "pad_gravity_x" },
		{ BT_FLOAT, "pad_gravity_y" },
		{ BT_FLOAT, "pad_gravity_z" },
		{ BT_FLOAT, "pad_gyroscope_x" },
		{ BT_FLOAT, "pad_gyroscope_y" },
		{ BT_FLOAT, "pad_gyroscope_z" },
		{ BT_FLOAT, "pad_magneticfield_x" },
		{ BT_FLOAT, "pad_magneticfield_y" },
		{ BT_FLOAT, "pad_magneticfield_z" },
		{ BT_BOOL, "pad_button_start"},
		{ BT_BOOL, "pad_button_select"},
		{ BT_BOOL, "pad_button_left"},
		{ BT_BOOL, "pad_button_right"},
		{ BT_BOOL, "pad_button_up"},
		{ BT_BOOL, "pad_button_down"},
		{ BT_BOOL, "pad_button_a"},
		{ BT_BOOL, "pad_button_b"},
		{ BT_BOOL, "pad_button_x"},
		{ BT_BOOL, "pad_button_y"},
		{ BT_BOOL, "pad_button_l1"},
		{ BT_BOOL, "pad_button_r1"},
		{ BT_BOOL, "pad_button_l2"},
		{ BT_BOOL, "pad_button_r2"},
		{ BT_BOOL, "pad_button_l3"},
		{ BT_BOOL, "pad_button_r3"},
		{ BT_BOOL, "pad_button_home"},
		{ BT_BOOL, "pad_button_17"},
		{ BT_BOOL, "pad_button_18"},
		{ BT_BOOL, "pad_button_19"},
		{ BT_BOOL, "pad_button_20"},
		{ BT_BOOL, "pad_button_21"},
		{ BT_BOOL, "pad_button_22"},
		{ BT_BOOL, "pad_button_23"},
		{ BT_BOOL, "pad_button_24"},
		{ BT_BOOL, "pad_button_25"},
		{ BT_BOOL, "pad_button_26"},
		{ BT_BOOL, "pad_button_27"},
		{ BT_BOOL, "pad_button_28"},
		{ BT_BOOL, "pad_button_29"},
		{ BT_BOOL, "pad_button_30"},
		{ BT_BOOL, "pad_button_31"}
};

const unsigned PadButtonCount = PadButtonCount_;
const unsigned PadAxisCount = PadButtonAxisCount_;

}


InputDevicePad::InputDevicePad(InputManager& manager, DeviceId device, unsigned index, DeviceVariant /*variant*/) :
	InputDevice(manager, device, index == InputDevice::AutoIndex ? manager.GetDeviceCountByType(DT_PAD) : index),
	impl_(NULL)
{
	state_ = manager.GetAllocator().New<InputState>(manager.GetAllocator(), PadButtonCount + PadAxisCount);
	GAINPUT_ASSERT(state_);
	previousState_ = manager.GetAllocator().New<InputState>(manager.GetAllocator(), PadButtonCount + PadAxisCount);
	GAINPUT_ASSERT(previousState_);

#if defined(GAINPUT_PLATFORM_LINUX)
	impl_ = manager.GetAllocator().New<InputDevicePadImplLinux>(manager, *this, index_, *state_, *previousState_);
#elif defined(GAINPUT_PLATFORM_WIN)
	impl_ = manager.GetAllocator().New<InputDevicePadImplWin>(manager, *this, index_, *state_, *previousState_);
#elif defined(GAINPUT_PLATFORM_IOS) || defined(GAINPUT_PLATFORM_TVOS)
	impl_ = manager.GetAllocator().New<InputDevicePadImplGCKit>(manager, *this, index_, *state_, *previousState_);
#elif defined(GAINPUT_PLATFORM_MAC)
	if(IOS14_RUNTIME)
	{
		impl_ = manager.GetAllocator().New<InputDevicePadImplGCKit>(manager, *this, index_, *state_, *previousState_);
	}
	else
	{
		//fallback to previous implementation
		impl_ = manager.GetAllocator().New<InputDevicePadImplMac>(manager, *this, index_, *state_, *previousState_);
	}
#elif defined(GAINPUT_PLATFORM_ANDROID)
	impl_ = manager.GetAllocator().New<InputDevicePadImplAndroid>(manager, *this, index_, *state_, *previousState_);
#elif defined(GAINPUT_PLATFORM_QUEST)
	impl_ = manager.GetAllocator().New<InputDevicePadImplQuest>(manager, *this, index_, *state_, *previousState_);
#elif defined(GAINPUT_PLATFORM_XBOX_ONE)
	impl_ = manager.GetAllocator().New<InputDevicePadImplXboxOne>(manager, *this, index_, *state_, *previousState_);
#elif defined(GAINPUT_PLATFORM_NX64)
	impl_ = manager.GetAllocator().New<InputDevicePadImplNx>(manager, *this, index_, *state_, *previousState_);
#elif defined(GAINPUT_PLATFORM_GGP)
	impl_ = manager.GetAllocator().New<InputDevicePadImplGGP>(manager, *this, index_, *state_, *previousState_);
#elif defined(GAINPUT_PLATFORM_ORBIS)
	impl_ = manager.GetAllocator().New<InputDevicePadImplOrbis>(manager, *this, index_, *state_, *previousState_);
#elif defined(GAINPUT_PLATFORM_PROSPERO)
	impl_ = manager.GetAllocator().New<InputDevicePadImplProspero>(manager, *this, index_, *state_, *previousState_);
#endif

	if (!impl_)
	{
		impl_ = manager.GetAllocator().New<InputDevicePadImplNull>(manager, *this, index_, *state_, *previousState_);
	}

	GAINPUT_ASSERT(impl_);

#if defined (GAINPUT_PLATFORM_XBOX_ONE)
	SetDeadZone(PadButtonLeftStickX, 0.0f);
	SetDeadZone(PadButtonLeftStickY, 0.0f);
	SetDeadZone(PadButtonRightStickX, 0.0f);
	SetDeadZone(PadButtonRightStickY, 0.0f);

#else
	SetDeadZone(PadButtonLeftStickX, 0.15f);
	SetDeadZone(PadButtonLeftStickY, 0.15f);
	SetDeadZone(PadButtonRightStickX, 0.15f);
	SetDeadZone(PadButtonRightStickY, 0.15f);
#endif
}

InputDevicePad::InputDevicePad(InputManager& manager, DeviceId device, unsigned hidDevId) :
	InputDevice(manager, device, (uint32_t)-1),
	impl_(NULL)
{
    UNREF_PARAM(hidDevId);
	state_ = manager.GetAllocator().New<InputState>(manager.GetAllocator(), PadButtonCount + PadAxisCount);
	GAINPUT_ASSERT(state_);
	previousState_ = manager.GetAllocator().New<InputState>(manager.GetAllocator(), PadButtonCount + PadAxisCount);
	GAINPUT_ASSERT(previousState_);

#if defined(GAINPUT_PLATFORM_LINUX) ||  defined(GAINPUT_PLATFORM_WIN) ||  defined(GAINPUT_PLATFORM_MAC)
	impl_ = manager.GetAllocator().New<InputDevicePadImplHID>(manager, *this, hidDevId);
#endif

	if (!impl_)
	{
		impl_ = manager.GetAllocator().New<InputDevicePadImplNull>(manager, *this, index_, *state_, *previousState_);
	}

	GAINPUT_ASSERT(impl_);

#if defined (GAINPUT_PLATFORM_XBOX_ONE)
	SetDeadZone(PadButtonLeftStickX, 0.0f);
	SetDeadZone(PadButtonLeftStickY, 0.0f);
	SetDeadZone(PadButtonRightStickX, 0.0f);
	SetDeadZone(PadButtonRightStickY, 0.0f);

#else
	SetDeadZone(PadButtonLeftStickX, 0.15f);
	SetDeadZone(PadButtonLeftStickY, 0.15f);
	SetDeadZone(PadButtonRightStickX, 0.15f);
	SetDeadZone(PadButtonRightStickY, 0.15f);
#endif
}

InputDevicePad::~InputDevicePad()
{
	manager_.GetAllocator().Delete(state_);
	manager_.GetAllocator().Delete(previousState_);
	manager_.GetAllocator().Delete(impl_);
}

void
InputDevicePad::CheckConnection()
{
	impl_->CheckConnection();
}

void
InputDevicePad::InternalUpdate(InputDeltaState* delta)
{
	impl_->Update(delta);

	if ((manager_.IsDebugRenderingEnabled() || IsDebugRenderingEnabled())
		&& manager_.GetDebugRenderer())
	{
		DebugRenderer* debugRenderer = manager_.GetDebugRenderer();
		InputState* state = GetInputState();
		char buf[64];
		float x = 0.4f;
		float y = 0.2f;
		for (int i = PadButtonStart; i < PadButtonMax_; ++i)
		{
			if (state->GetBool(i))
			{
				GetButtonName(i, buf, 64);
				debugRenderer->DrawText(x, y, buf);
				y += 0.025f;
			}
		}

		x = 0.8f;
		y = 0.2f;
		const float circleRadius = 0.1f;
		debugRenderer->DrawCircle(x, y, 0.01f);
		debugRenderer->DrawCircle(x, y, circleRadius);
		float dirX = state->GetFloat(PadButtonLeftStickX) * circleRadius;
		float dirY = state->GetFloat(PadButtonLeftStickY) * circleRadius;
		debugRenderer->DrawLine(x, y, x + dirX, y + dirY);

		y = 0.6f;
		debugRenderer->DrawCircle(x, y, 0.01f);
		debugRenderer->DrawCircle(x, y, circleRadius);
		dirX = state->GetFloat(PadButtonRightStickX) * circleRadius;
		dirY = state->GetFloat(PadButtonRightStickY) * circleRadius;
		debugRenderer->DrawLine(x, y, x + dirX, y + dirY);
	}
}

InputDevice::DeviceState
InputDevicePad::InternalGetState() const
{
	return impl_->GetState();
}

InputDevice::DeviceVariant
InputDevicePad::GetVariant() const
{
	return impl_->GetVariant();
}

bool
InputDevicePad::IsValidButtonId(DeviceButtonId deviceButton) const
{
	return impl_->IsValidButton(deviceButton);
}

size_t
InputDevicePad::GetAnyButtonDown(DeviceButtonSpec* outButtons, size_t maxButtonCount) const
{
	GAINPUT_ASSERT(outButtons);
	GAINPUT_ASSERT(maxButtonCount > 0);
	return CheckAllButtonsDown(outButtons, maxButtonCount, PadButtonLeftStickX, PadButtonMax_);
}

size_t
InputDevicePad::GetButtonName(DeviceButtonId deviceButton, char* buffer, size_t bufferLength) const
{
	GAINPUT_ASSERT(IsValidButtonId(deviceButton));
	GAINPUT_ASSERT(buffer);
	GAINPUT_ASSERT(bufferLength > 0);
    if (bufferLength > 0)
    {
        strncpy(buffer, deviceButtonInfos[deviceButton].name, bufferLength-1);
        buffer[bufferLength-1] = 0;        
    }
    else
    {        
        GAINPUT_ASSERT(!"bufferLength <= 0");
    }
	const size_t nameLen = strlen(deviceButtonInfos[deviceButton].name);
	return nameLen >= bufferLength ? bufferLength : nameLen+1;
}

ButtonType
InputDevicePad::GetButtonType(DeviceButtonId deviceButton) const
{
	return deviceButtonInfos[deviceButton].type;
}

DeviceButtonId
InputDevicePad::GetButtonByName(const char* name) const
{
	GAINPUT_ASSERT(name);
	for (unsigned i = 0; i < PadButtonCount + PadAxisCount; ++i)
	{
		if (strcmp(name, deviceButtonInfos[i].name) == 0)
		{
			return DeviceButtonId(i);
		}
	}
	return InvalidDeviceButtonId;
}

InputState*
InputDevicePad::GetNextInputState()
{
	return impl_->GetNextInputState();
}

bool
InputDevicePad::Vibrate(float leftMotor, float rightMotor)
{
	return impl_->Vibrate(leftMotor, rightMotor);
}

const char*
InputDevicePad::GetDeviceName()
{
	return impl_->GetDeviceName();
}

void InputDevicePad::SetOnDeviceChangeCallBack(void(*onDeviceChange)(const char*, bool added, int controllerIndex))
{
	return impl_->SetOnDeviceChangeCallBack(onDeviceChange);
}

bool
InputDevicePad::SetRumbleEffect(float left_motor, float right_motor, uint32_t duration_ms, bool targetOwningDevice)
{
	return impl_->SetRumbleEffect(left_motor, right_motor, duration_ms, targetOwningDevice);
}


void InputDevicePad::SetLEDColor(uint8_t r, uint8_t g, uint8_t b)
{
	impl_->SetLEDColor(r, g, b);
}

}

