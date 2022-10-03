
#include "../../include/gainput/gainput.h"

namespace gainput
{


InputDevice::InputDevice(InputManager& manager, DeviceId device, unsigned index) :
	manager_(manager),
	deviceId_(device),
	index_(index),
	deadZones_(0),
	debugRenderingEnabled_(false)
	
#if defined(GAINPUT_DEV) || defined(GAINPUT_ENABLE_RECORDER)
	, synced_(false)
#endif
{
}

InputDevice::~InputDevice()
{
	manager_.GetAllocator().Deallocate(deadZones_);
}

void
InputDevice::Update(InputDeltaState* delta)
{
	*previousState_ = *state_;
	
#if defined(GAINPUT_DEV)
	if (synced_)
	{
		return;
	}
#endif
	InternalUpdate(delta);
}

InputDevice::DeviceState
InputDevice::GetState() const
{
#if defined(GAINPUT_DEV)
	if (synced_)
	{
		return DS_OK;
	}
#endif
	return InternalGetState();
}

	
float InputDevice::GetDeadZone(DeviceButtonId buttonId) const
{
	if (!deadZones_
		|| !IsValidButtonId(buttonId))
	{
		return 0.0f;
	}
	GAINPUT_ASSERT(buttonId < state_->GetButtonCount());
	return deadZones_[buttonId];
}

void InputDevice::SetDeadZone(DeviceButtonId buttonId, float value)
{
	if (!deadZones_)
	{
		const size_t size = sizeof(float) * state_->GetButtonCount();
		deadZones_ = reinterpret_cast<float*>(manager_.GetAllocator().Allocate(size));
		memset(deadZones_, 0, size);
	}
	GAINPUT_ASSERT(buttonId < state_->GetButtonCount());
	deadZones_[buttonId] = value;
}

void
InputDevice::SetDebugRenderingEnabled(bool enabled)
{
	debugRenderingEnabled_ = enabled;
}

size_t
InputDevice::CheckAllButtonsDown(DeviceButtonSpec* outButtons, size_t maxButtonCount, unsigned start, unsigned end) const
{
	size_t buttonsFound = 0;
	for (unsigned i = start; i < end && buttonsFound < maxButtonCount; ++i)
	{
		DeviceButtonId id(i);
		if (IsValidButtonId(id) && GetBool(id))
		{
			outButtons[buttonsFound].deviceId = deviceId_;
			outButtons[buttonsFound].buttonId = id;
			++buttonsFound;
		}
		//we want to get notified if a button was released as well.
		else if (IsValidButtonId(id) && !GetBool(id) && GetBoolPrevious(id))
		{
			outButtons[buttonsFound].deviceId = deviceId_;
			outButtons[buttonsFound].buttonId = id;
			++buttonsFound;
		}
	}
	return buttonsFound;
}

}

