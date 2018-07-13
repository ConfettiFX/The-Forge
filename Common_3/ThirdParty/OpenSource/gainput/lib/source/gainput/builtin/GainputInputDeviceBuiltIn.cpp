
#include "../../../include/gainput/gainput.h"
#include "../../../include/gainput/GainputDebugRenderer.h"

#include "GainputInputDeviceBuiltInImpl.h"
#include "../../../include/gainput/GainputInputDeltaState.h"
#include "../../../include/gainput/GainputHelpers.h"
#include "../../../include/gainput/GainputLog.h"

#if defined(GAINPUT_PLATFORM_ANDROID)
	#include "GainputInputDeviceBuiltInAndroid.h"
#elif defined(GAINPUT_PLATFORM_IOS)
	#include "GainputInputDeviceBuiltInIos.h"
#endif

#include "GainputInputDeviceBuiltInNull.h"

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
		{ BT_FLOAT, "builtin_acceleration_x" },
		{ BT_FLOAT, "builtin_acceleration_y" },
		{ BT_FLOAT, "builtin_acceleration_z" },
		{ BT_FLOAT, "builtin_gravity_x" },
		{ BT_FLOAT, "builtin_gravity_y" },
		{ BT_FLOAT, "builtin_gravity_z" },
		{ BT_FLOAT, "builtin_gyroscope_x" },
		{ BT_FLOAT, "builtin_gyroscope_y" },
		{ BT_FLOAT, "builtin_gyroscope_z" },
		{ BT_FLOAT, "builtin_magneticfield_x" },
		{ BT_FLOAT, "builtin_magneticfield_y" },
		{ BT_FLOAT, "builtin_magneticfield_z" },
};

const unsigned BuiltInButtonCount = BuiltInButtonCount_;

}


InputDeviceBuiltIn::InputDeviceBuiltIn(InputManager& manager, DeviceId device, unsigned index, DeviceVariant /*variant*/) :
	InputDevice(manager, device, index == InputDevice::AutoIndex ? manager.GetDeviceCountByType(DT_BUILTIN) : 0),
	impl_(0)
{
	state_ = manager.GetAllocator().New<InputState>(manager.GetAllocator(), BuiltInButtonCount);
	GAINPUT_ASSERT(state_);
	previousState_ = manager.GetAllocator().New<InputState>(manager.GetAllocator(), BuiltInButtonCount);
	GAINPUT_ASSERT(previousState_);

#if defined(GAINPUT_PLATFORM_ANDROID)
	impl_ = manager.GetAllocator().New<InputDeviceBuiltInImplAndroid>(manager, *this, index_, *state_, *previousState_);
#elif defined(GAINPUT_PLATFORM_IOS)
	impl_ = manager.GetAllocator().New<InputDeviceBuiltInImplIos>(manager, *this, index_, *state_, *previousState_);
#endif

	if (!impl_)
	{
		impl_ = manager.GetAllocator().New<InputDeviceBuiltInImplNull>(manager, *this, index_, *state_, *previousState_);
	}

	GAINPUT_ASSERT(impl_);
}

InputDeviceBuiltIn::~InputDeviceBuiltIn()
{
	manager_.GetAllocator().Delete(state_);
	manager_.GetAllocator().Delete(previousState_);
	manager_.GetAllocator().Delete(impl_);
}

void
InputDeviceBuiltIn::InternalUpdate(InputDeltaState* delta)
{
	impl_->Update(delta);
}

InputDevice::DeviceState
InputDeviceBuiltIn::InternalGetState() const
{
	return impl_->GetState();
}

InputDevice::DeviceVariant
InputDeviceBuiltIn::GetVariant() const
{
	return impl_->GetVariant();
}

bool
InputDeviceBuiltIn::IsValidButtonId(DeviceButtonId deviceButton) const
{
	return impl_->IsValidButton(deviceButton);
}

size_t
InputDeviceBuiltIn::GetAnyButtonDown(DeviceButtonSpec* outButtons, size_t maxButtonCount) const
{
	GAINPUT_ASSERT(outButtons);
	GAINPUT_ASSERT(maxButtonCount > 0);
	return CheckAllButtonsDown(outButtons, maxButtonCount, BuiltInButtonAccelerationX, BuiltInButtonCount_);
}

size_t
InputDeviceBuiltIn::GetButtonName(DeviceButtonId deviceButton, char* buffer, size_t bufferLength) const
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
InputDeviceBuiltIn::GetButtonType(DeviceButtonId deviceButton) const
{
	return deviceButtonInfos[deviceButton].type;
}

DeviceButtonId
InputDeviceBuiltIn::GetButtonByName(const char* name) const
{
	GAINPUT_ASSERT(name);
	for (unsigned i = 0; i < BuiltInButtonCount; ++i)
	{
		if (strcmp(name, deviceButtonInfos[i].name) == 0)
		{
			return DeviceButtonId(i);
		}
	}
	return InvalidDeviceButtonId;
}

}

