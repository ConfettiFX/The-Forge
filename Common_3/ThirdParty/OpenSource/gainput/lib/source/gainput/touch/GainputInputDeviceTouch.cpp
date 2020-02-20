
#include "../../../include/gainput/gainput.h"
#include "../../../include/gainput/GainputDebugRenderer.h"

#include "GainputInputDeviceTouchImpl.h"
#include "GainputTouchInfo.h"
#include "../../../include/gainput/GainputInputDeltaState.h"
#include "../../../include/gainput/GainputHelpers.h"
#include "../../../include/gainput/GainputLog.h"

#include "GainputInputDeviceTouchNull.h"

#if defined(GAINPUT_PLATFORM_ANDROID)
	#include "GainputInputDeviceTouchAndroid.h"
#elif defined(GAINPUT_PLATFORM_IOS) || defined(GAINPUT_PLATFORM_TVOS)
	#include "GainputInputDeviceTouchIos.h"
#elif defined(GAINPUT_PLATFORM_NX64)
	#include "../../../../../../../../Switch/Common_3/OS/Input/GainputInputDeviceTouchNX.h"
#endif

namespace gainput
{

InputDeviceTouch::InputDeviceTouch(InputManager& manager, DeviceId device, unsigned index, DeviceVariant variant) :
	InputDevice(manager, device, index == InputDevice::AutoIndex ? manager.GetDeviceCountByType(DT_TOUCH) : 0),
	impl_(0)
{
	state_ = manager.GetAllocator().New<InputState>(manager.GetAllocator(), TouchPointCount*TouchDataElems);
	GAINPUT_ASSERT(state_);
	previousState_ = manager.GetAllocator().New<InputState>(manager.GetAllocator(), TouchPointCount*TouchDataElems);
	GAINPUT_ASSERT(previousState_);

#if defined(GAINPUT_PLATFORM_ANDROID)
	if (variant != DV_NULL)
	{
		impl_ = manager.GetAllocator().New<InputDeviceTouchImplAndroid>(manager, *this, *state_, *previousState_);
	}
#elif defined(GAINPUT_PLATFORM_IOS) || defined(GAINPUT_PLATFORM_TVOS)
	if (variant != DV_NULL)
	{
		impl_ = manager.GetAllocator().New<InputDeviceTouchImplIos>(manager, *this, *state_, *previousState_);
	}
#elif defined(GAINPUT_PLATFORM_NX64)
	if (variant != DV_NULL)
	{
		impl_ = manager.GetAllocator().New<InputDeviceTouchImplNx>(manager, *this, *state_, *previousState_);
	}
#endif

	if (!impl_)
	{
		impl_ = manager.GetAllocator().New<InputDeviceTouchImplNull>(manager, *this);
	}
	GAINPUT_ASSERT(impl_);
}

InputDeviceTouch::~InputDeviceTouch()
{
	manager_.GetAllocator().Delete(state_);
	manager_.GetAllocator().Delete(previousState_);
	manager_.GetAllocator().Delete(impl_);
}

bool
InputDeviceTouch::IsValidButtonId(DeviceButtonId deviceButton) const
{
    if (deviceButton == Touch0Pressure
        || deviceButton == Touch1Pressure
        || deviceButton == Touch2Pressure
        || deviceButton == Touch3Pressure
        || deviceButton == Touch4Pressure
        || deviceButton == Touch5Pressure
        || deviceButton == Touch6Pressure
        || deviceButton == Touch7Pressure
    )
    {
        return impl_->SupportsPressure();
    }
    return deviceButton < TouchCount_;
}

void
InputDeviceTouch::InternalUpdate(InputDeltaState* delta)
{
	impl_->Update(delta);

	if ((manager_.IsDebugRenderingEnabled() || IsDebugRenderingEnabled())
		&& manager_.GetDebugRenderer())
	{
		DebugRenderer* debugRenderer = manager_.GetDebugRenderer();
		InputState* state = GetInputState();

		for (unsigned i = 0; i < TouchPointCount; ++i)
		{
			if (state->GetBool(Touch0Down + i*4))
			{
				const float x = state->GetFloat(Touch0X + i*4);
				const float y = state->GetFloat(Touch0Y + i*4);
				debugRenderer->DrawCircle(x, y, 0.03f);
			}
		}
	}
}

InputDevice::DeviceState
InputDeviceTouch::InternalGetState() const
{
	return impl_->GetState();
}

InputDevice::DeviceVariant
InputDeviceTouch::GetVariant() const
{
	return impl_->GetVariant();
}

size_t
InputDeviceTouch::GetAnyButtonDown(DeviceButtonSpec* outButtons, size_t maxButtonCount) const
{
	GAINPUT_ASSERT(outButtons);
	GAINPUT_ASSERT(maxButtonCount > 0);
	return CheckAllButtonsDown(outButtons, maxButtonCount, Touch0Down, TouchCount_);
}

size_t
InputDeviceTouch::GetButtonName(DeviceButtonId deviceButton, char* buffer, size_t bufferLength) const
{
	GAINPUT_ASSERT(IsValidButtonId(deviceButton));
	GAINPUT_ASSERT(buffer);
	GAINPUT_ASSERT(bufferLength > 0);
	strncpy(buffer, deviceButtonInfos[deviceButton].name, bufferLength);
	buffer[bufferLength-1] = 0;
	const size_t nameLen = strlen(deviceButtonInfos[deviceButton].name);
	return nameLen >= bufferLength ? bufferLength : nameLen+1;
}

ButtonType
InputDeviceTouch::GetButtonType(DeviceButtonId deviceButton) const
{
	GAINPUT_ASSERT(IsValidButtonId(deviceButton));
	return deviceButtonInfos[deviceButton].type;
}

void InputDeviceTouch::GetVirtualKeyboardInput(char* buffer, uint32_t inBufferLength) const {
	impl_->GetVirtualKeyboardInput(buffer, inBufferLength);
}

DeviceButtonId
InputDeviceTouch::GetButtonByName(const char* name) const
{
	GAINPUT_ASSERT(name);
	for (unsigned i = 0; i < TouchPointCount*TouchDataElems; ++i)
	{
		if (strcmp(name, deviceButtonInfos[i].name) == 0)
		{
			return DeviceButtonId(i);
		}
	}
	return InvalidDeviceButtonId;
}

InputState*
InputDeviceTouch::GetNextInputState()
{
	return impl_->GetNextInputState();
}

}
