
#include "../../../include/gainput/gainput.h"
#include "../../../include/gainput/GainputDebugRenderer.h"

#include "GainputInputDeviceKeyboardImpl.h"
#include "GainputKeyboardKeyNames.h"
#include "../../../include/gainput/GainputInputDeltaState.h"
#include "../../../include/gainput/GainputHelpers.h"
#include "../../../include/gainput/GainputLog.h"

#if defined(GAINPUT_PLATFORM_LINUX)
	#include "../linux/GainputInputDeviceKeyboardLinux.h"
	#include "../linux/GainputInputDeviceKeyboardEvdev.h"
#elif defined(GAINPUT_PLATFORM_WIN)
	#include "../windows/GainputInputDeviceKeyboardWin.h"
	#include "../windows/GainputInputDeviceKeyboardWinRaw.h"
#elif defined(GAINPUT_PLATFORM_ANDROID)
	#include "../android/GainputInputDeviceKeyboardAndroid.h"
#elif defined(GAINPUT_PLATFORM_MAC)
	#include "../apple/GainputInputDeviceKeyboardMac.h"
#elif defined(GAINPUT_PLATFORM_IOS)
	#include "../apple/GainputInputDeviceKeyboardIOS.h"
#elif defined(GAINPUT_PLATFORM_GGP)
	#include "../../../../../../../../Stadia/Common_3/OS/Input/GainputInputDeviceKeyboardGGP.h"
#endif

#include "GainputInputDeviceKeyboardNull.h"


namespace gainput
{

InputDeviceKeyboard::InputDeviceKeyboard(InputManager& manager, DeviceId device, unsigned index, DeviceVariant variant) :
	InputDevice(manager, device, index == InputDevice::AutoIndex ? manager.GetDeviceCountByType(DT_KEYBOARD): index),
	impl_(0),
	keyNames_(manager_.GetAllocator())
{
    UNREF_PARAM(variant);
	state_ = manager.GetAllocator().New<InputState>(manager.GetAllocator(), KeyCount_);
	GAINPUT_ASSERT(state_);
	previousState_ = manager.GetAllocator().New<InputState>(manager.GetAllocator(), KeyCount_);
	GAINPUT_ASSERT(previousState_);

#if defined(GAINPUT_PLATFORM_LINUX)
	if (variant == DV_STANDARD)
	{
		impl_ = manager.GetAllocator().New<InputDeviceKeyboardImplLinux>(manager, *this, *state_, *previousState_);
	}
	else if (variant == DV_RAW)
	{
		impl_ = manager.GetAllocator().New<InputDeviceKeyboardImplEvdev>(manager, *this, *state_, *previousState_);
	}
#elif defined(GAINPUT_PLATFORM_WIN)
	if (variant == DV_STANDARD)
	{
		impl_ = manager.GetAllocator().New<InputDeviceKeyboardImplWin>(manager, *this, *state_, *previousState_);
	}
	else if (variant == DV_RAW)
	{
		impl_ = manager.GetAllocator().New<InputDeviceKeyboardImplWinRaw>(manager, *this, *state_, *previousState_);
	}
#elif defined(GAINPUT_PLATFORM_ANDROID)
	impl_ = manager.GetAllocator().New<InputDeviceKeyboardImplAndroid>(manager, *this, *state_, *previousState_);
#elif defined(GAINPUT_PLATFORM_MAC)
	impl_ = manager.GetAllocator().New<InputDeviceKeyboardImplMac>(manager, *this, *state_, *previousState_);
#elif defined(GAINPUT_PLATFORM_IOS)
	impl_ = manager.GetAllocator().New<InputDeviceKeyboardImplIOS>(manager, *this, *state_, *previousState_);
#endif

	if (!impl_)
	{
		impl_ = manager.GetAllocator().New<InputDeviceKeyboardImplNull>(manager, device);
	}

	GAINPUT_ASSERT(impl_);

	GetKeyboardKeyNames(keyNames_);
}

InputDeviceKeyboard::~InputDeviceKeyboard()
{
	manager_.GetAllocator().Delete(state_);
	manager_.GetAllocator().Delete(previousState_);
	manager_.GetAllocator().Delete(impl_);
}

void
InputDeviceKeyboard::InternalUpdate(InputDeltaState* delta)
{
	impl_->Update(delta);

	if ((manager_.IsDebugRenderingEnabled() || IsDebugRenderingEnabled())
		&& manager_.GetDebugRenderer())
	{
		DebugRenderer* debugRenderer = manager_.GetDebugRenderer();
		InputState* state = GetInputState();
		char buf[64];
		const float x = 0.2f;
		float y = 0.2f;
		for (int i = 0; i < KeyCount_; ++i)
		{
			if (state->GetBool(i))
			{
				GetButtonName(i, buf, 64);
				debugRenderer->DrawText(x, y, buf);
				y += 0.025f;
			}
		}
	}
}

InputDevice::DeviceState
InputDeviceKeyboard::InternalGetState() const
{
	return impl_->GetState();

}

InputDevice::DeviceVariant
InputDeviceKeyboard::GetVariant() const
{
	return impl_->GetVariant();
}

size_t
InputDeviceKeyboard::GetAnyButtonDown(DeviceButtonSpec* outButtons, size_t maxButtonCount) const
{
	GAINPUT_ASSERT(outButtons);
	GAINPUT_ASSERT(maxButtonCount > 0);
	return CheckAllButtonsDown(outButtons, maxButtonCount, 0, KeyCount_);
}

size_t
InputDeviceKeyboard::GetButtonName(DeviceButtonId deviceButton, char* buffer, size_t bufferLength) const
{
	GAINPUT_ASSERT(IsValidButtonId(deviceButton));
	GAINPUT_ASSERT(buffer);
	GAINPUT_ASSERT(bufferLength > 0);
	HashMap<Key, const char*>::const_iterator it = keyNames_.find(Key(deviceButton));
	if (it == keyNames_.end())
	{
		return 0;
	}
	strncpy(buffer, it->second, bufferLength);
	buffer[bufferLength-1] = 0;
	const size_t nameLen = strlen(it->second);
	return nameLen >= bufferLength ? bufferLength : nameLen+1;
}

ButtonType
InputDeviceKeyboard::GetButtonType(DeviceButtonId deviceButton) const
{
	GAINPUT_ASSERT(IsValidButtonId(deviceButton));
	return BT_BOOL;
}

DeviceButtonId
InputDeviceKeyboard::GetButtonByName(const char* name) const
{
	GAINPUT_ASSERT(name);
	for (HashMap<Key, const char*>::const_iterator it = keyNames_.begin();
			it != keyNames_.end();
			++it)
	{
		if (strcmp(name, it->second) == 0)
		{
			return it->first;
		}
	}
	return InvalidDeviceButtonId;
}

InputState*
InputDeviceKeyboard::GetNextInputState()
{
	return impl_->GetNextInputState();
}

bool
InputDeviceKeyboard::IsTextInputEnabled() const
{
	return impl_->IsTextInputEnabled();
}

void
InputDeviceKeyboard::SetTextInputEnabled(bool enabled)
{
	impl_->SetTextInputEnabled(enabled);
}

wchar_t*
InputDeviceKeyboard::GetTextInput(uint32_t* count)
{
	return impl_->GetTextInput(count);
}

void
InputDeviceKeyboard::ClearButtons()
{
	impl_->ClearButtons();
}
}

