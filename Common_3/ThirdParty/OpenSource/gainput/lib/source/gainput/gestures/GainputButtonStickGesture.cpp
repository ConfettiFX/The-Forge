
#include "../../../include/gainput/gainput.h"
#include "../../../include/gainput/gestures/GainputButtonStickGesture.h"

#ifdef GAINPUT_ENABLE_BUTTON_STICK_GESTURE
#include "../../../include/gainput/GainputInputDeltaState.h"
#include "../../../include/gainput/GainputHelpers.h"

namespace gainput
{

ButtonStickGesture::ButtonStickGesture(InputManager& manager, DeviceId device, unsigned index, DeviceVariant /*variant*/) :
	InputGesture(manager, device, index)
{
	negativeAxis_.buttonId = InvalidDeviceButtonId;
	positiveAxis_.buttonId = InvalidDeviceButtonId;

	state_ = manager_.GetAllocator().New<InputState>(manager.GetAllocator(), 1);
	GAINPUT_ASSERT(state_);
	previousState_ = manager_.GetAllocator().New<InputState>(manager.GetAllocator(), 1);
	GAINPUT_ASSERT(previousState_);
}

ButtonStickGesture::~ButtonStickGesture()
{
	manager_.GetAllocator().Delete(state_);
	manager_.GetAllocator().Delete(previousState_);
}

void
ButtonStickGesture::Initialize(DeviceId negativeAxisDevice, DeviceButtonId negativeAxis,
			DeviceId positiveAxisDevice, DeviceButtonId positiveAxis)
{
	negativeAxis_.deviceId = negativeAxisDevice;
	negativeAxis_.buttonId = negativeAxis;
	positiveAxis_.deviceId = positiveAxisDevice;
	positiveAxis_.buttonId = positiveAxis;
}

void
ButtonStickGesture::InternalUpdate(InputDeltaState* delta)
{
	if (negativeAxis_.buttonId == InvalidDeviceButtonId
		|| positiveAxis_.buttonId == InvalidDeviceButtonId)
	{
		return;
	}

	const InputDevice* negativeDevice = manager_.GetDevice(negativeAxis_.deviceId);
	GAINPUT_ASSERT(negativeDevice);
	const bool isDown = negativeDevice->GetBool(negativeAxis_.buttonId);

	const InputDevice* positiveDevice = manager_.GetDevice(positiveAxis_.deviceId);
	GAINPUT_ASSERT(positiveDevice);
	const bool isDown2 = positiveDevice->GetBool(positiveAxis_.buttonId);

	if (isDown && !isDown2)
	{
		HandleAxis(*this, *state_, delta, ButtonStickAxis, -1.0f);
	}
	else if (!isDown && isDown2)
	{
		HandleAxis(*this, *state_, delta, ButtonStickAxis, 1.0f);
	}
	else
	{
		HandleAxis(*this, *state_, delta, ButtonStickAxis, 0.0f);
	}
}

}

#endif

