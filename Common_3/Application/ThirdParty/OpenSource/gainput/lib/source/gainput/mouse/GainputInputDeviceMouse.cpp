
#include "../../../include/gainput/gainput.h"
#include "../../../include/gainput/GainputDebugRenderer.h"

#include "GainputInputDeviceMouseImpl.h"
#include "GainputInputDeviceMouseNull.h"
#include "GainputMouseInfo.h"
#include "../../../include/gainput/GainputInputDeltaState.h"
#include "../../../include/gainput/GainputHelpers.h"
#include "../../../include/gainput/GainputLog.h"

#if defined(GAINPUT_PLATFORM_LINUX)
	#include "../linux/GainputInputDeviceMouseLinux.h"
	#include "../linux/GainputInputDeviceMouseEvdev.h"
	#include "../linux/GainputInputDeviceMouseLinuxRaw.h"
#elif defined(GAINPUT_PLATFORM_WIN)
	#include "../windows/GainputInputDeviceMouseWin.h"
	#include "../windows/GainputInputDeviceMouseWinRaw.h"
#elif defined(GAINPUT_PLATFORM_MAC)
	#include "../apple/GainputInputDeviceMouseMac.h"
    #include "../apple/GainputInputDeviceMouseMacRaw.h"
#elif defined(GAINPUT_PLATFORM_GGP)
	#include "../../../../../../../../Stadia/Common_3/OS/Input/GainputInputDeviceMouseGGP.h"
	#include "../../../../../../../../Stadia/Common_3/OS/Input/GainputInputDeviceMouseGGPRaw.h"
#endif

namespace gainput
{


InputDeviceMouse::InputDeviceMouse(InputManager& manager, DeviceId device, unsigned index, DeviceVariant variant) :
	InputDevice(manager, device, index == InputDevice::AutoIndex ? manager.GetDeviceCountByType(DT_MOUSE) : index),
	impl_(0)
{
    UNREF_PARAM(variant);
	state_ = manager.GetAllocator().New<InputState>(manager.GetAllocator(), MouseButtonCount + MouseAxisCount);
	GAINPUT_ASSERT(state_);
	previousState_ = manager.GetAllocator().New<InputState>(manager.GetAllocator(), MouseButtonCount + MouseAxisCount);
	GAINPUT_ASSERT(previousState_);

#if defined(GAINPUT_PLATFORM_LINUX)
	if (variant == DV_STANDARD)
	{
		impl_ = manager.GetAllocator().New<InputDeviceMouseImplLinux>(manager, *this, *state_, *previousState_);
	}
	else if (variant == DV_RAW)
	{
		impl_ = manager.GetAllocator().New<InputDeviceMouseImplLinuxRaw>(manager, *this, *state_, *previousState_);
	}
#elif defined(GAINPUT_PLATFORM_WIN)
	if (variant == DV_STANDARD)
	{
		impl_ = manager.GetAllocator().New<InputDeviceMouseImplWin>(manager, *this, *state_, *previousState_);
	}
	else if (variant == DV_RAW)
	{
		impl_ = manager.GetAllocator().New<InputDeviceMouseImplWinRaw>(manager, *this, *state_, *previousState_);
	}
#elif defined(GAINPUT_PLATFORM_MAC)
    if (variant == DV_STANDARD)
    {
        impl_ = manager.GetAllocator().New<InputDeviceMouseImplMac>(manager, *this, *state_, *previousState_);
    }
    else if (variant == DV_RAW)
    {
        impl_ = manager.GetAllocator().New<InputDeviceMouseImplMacRaw>(manager, *this, *state_, *previousState_);
    }
#elif defined(GAINPUT_PLATFORM_GGP)
	if (variant == DV_STANDARD)
	{
		impl_ = manager.GetAllocator().New<InputDeviceMouseImplGGP>(manager, *this, index_, *state_, *previousState_);
	}
	if (variant == DV_RAW)
	{
		impl_ = manager.GetAllocator().New<InputDeviceMouseImplGGPRaw>(manager, *this, index_, *state_, *previousState_);
	}
#endif

	if (!impl_)
	{
		impl_ = manager.GetAllocator().New<InputDeviceMouseImplNull>(manager, device);
	}
	GAINPUT_ASSERT(impl_);
}

InputDeviceMouse::~InputDeviceMouse()
{
	manager_.GetAllocator().Delete(state_);
	manager_.GetAllocator().Delete(previousState_);
	manager_.GetAllocator().Delete(impl_);
}

void
InputDeviceMouse::InternalUpdate(InputDeltaState* delta)
{
	impl_->Update(delta);

	if ((manager_.IsDebugRenderingEnabled() || IsDebugRenderingEnabled())
		&& manager_.GetDebugRenderer())
	{
		DebugRenderer* debugRenderer = manager_.GetDebugRenderer();
		InputState* state = GetInputState();
		const float x = state->GetFloat(MouseAxisX);
		const float y = state->GetFloat(MouseAxisY);
		debugRenderer->DrawCircle(x, y, 0.01f);

		for (int i = 0; i < MouseButtonCount; ++i)
		{
			if (state->GetBool(MouseButton0 + i))
			{
				debugRenderer->DrawCircle(x, y, 0.03f + (0.005f * float(i+1)));
			}
		}
	}
}

InputDevice::DeviceState
InputDeviceMouse::InternalGetState() const
{
	return impl_->GetState();
}

InputDevice::DeviceVariant
InputDeviceMouse::GetVariant() const
{
	return impl_->GetVariant();
}

void InputDeviceMouse::WarpMouse(const float&x, const float& y) 
{
	impl_->WarpMouse(x,y);
}

size_t
InputDeviceMouse::GetAnyButtonDown(DeviceButtonSpec* outButtons, size_t maxButtonCount) const
{
	GAINPUT_ASSERT(outButtons);
	GAINPUT_ASSERT(maxButtonCount > 0);
	return CheckAllButtonsDown(outButtons, maxButtonCount, MouseButton0, MouseButtonCount_);
}

size_t
InputDeviceMouse::GetButtonName(DeviceButtonId deviceButton, char* buffer, size_t bufferLength) const
{
	GAINPUT_ASSERT(IsValidButtonId(deviceButton));
	GAINPUT_ASSERT(buffer);
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
InputDeviceMouse::GetButtonType(DeviceButtonId deviceButton) const
{
	GAINPUT_ASSERT(IsValidButtonId(deviceButton));
	return deviceButtonInfos[deviceButton].type;
}

DeviceButtonId
InputDeviceMouse::GetButtonByName(const char* name) const
{
	GAINPUT_ASSERT(name);
	for (unsigned i = 0; i < MouseButtonCount + MouseAxisCount; ++i)
	{
		if (strcmp(name, deviceButtonInfos[i].name) == 0)
		{
			return DeviceButtonId(i);
		}
	}
	return InvalidDeviceButtonId;
}

InputState*
InputDeviceMouse::GetNextInputState()
{
	return impl_->GetNextInputState();
}
}

