
#include "../../../include/gainput/gainput.h"
#include "../../../include/gainput/gestures/GainputSimultaneouslyDownGesture.h"

#ifdef GAINPUT_ENABLE_SIMULTANEOUSLY_DOWN_GESTURE
#include "../../../include/gainput/GainputInputDeltaState.h"
#include "../../../include/gainput/GainputHelpers.h"

namespace gainput
{

SimultaneouslyDownGesture::SimultaneouslyDownGesture(InputManager& manager, DeviceId device, unsigned index, DeviceVariant /*variant*/) :
	InputGesture(manager, device, index),
	buttons_(manager.GetAllocator())
{
	state_ = manager_.GetAllocator().New<InputState>(manager.GetAllocator(), 1);
	GAINPUT_ASSERT(state_);
	previousState_ = manager_.GetAllocator().New<InputState>(manager.GetAllocator(), 1);
	GAINPUT_ASSERT(previousState_);
}

SimultaneouslyDownGesture::~SimultaneouslyDownGesture()
{
	manager_.GetAllocator().Delete(state_);
	manager_.GetAllocator().Delete(previousState_);
}

void
SimultaneouslyDownGesture::AddButton(DeviceId device, DeviceButtonId button)
{
	DeviceButtonSpec spec;
	spec.deviceId = device;
	spec.buttonId = button;
	buttons_.push_back(spec);
}

void
SimultaneouslyDownGesture::ClearButtons()
{
	buttons_.clear();
}

void
SimultaneouslyDownGesture::InternalUpdate(InputDeltaState* delta)
{
	bool allDown = !buttons_.empty();

	for (Array<DeviceButtonSpec>::const_iterator it = buttons_.begin();
			it != buttons_.end() && allDown;
			++it)
	{
		const InputDevice* device = manager_.GetDevice(it->deviceId);
		GAINPUT_ASSERT(device);
		allDown = allDown && device->GetBool(it->buttonId);
	}

	HandleButton(*this, *state_, delta, SimultaneouslyDownTriggered, allDown);
}

}

#endif

