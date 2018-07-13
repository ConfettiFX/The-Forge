
#include "../../../include/gainput/gainput.h"
#include "../../../include/gainput/gestures/GainputPinchGesture.h"

#ifdef GAINPUT_ENABLE_PINCH_GESTURE
#include "../../../include/gainput/GainputInputDeltaState.h"
#include "../../../include/gainput/GainputHelpers.h"
#include <math.h>

namespace gainput
{

PinchGesture::PinchGesture(InputManager& manager, DeviceId device, unsigned index, DeviceVariant /*variant*/) :
	InputGesture(manager, device, index),
	pinching_(false)
{
	downButton_.buttonId = InvalidDeviceButtonId;
	xAxis_.buttonId = InvalidDeviceButtonId;
	yAxis_.buttonId = InvalidDeviceButtonId;
	downButton2_.buttonId = InvalidDeviceButtonId;
	xAxis2_.buttonId = InvalidDeviceButtonId;
	yAxis2_.buttonId = InvalidDeviceButtonId;

	state_ = manager_.GetAllocator().New<InputState>(manager.GetAllocator(), 2);
	GAINPUT_ASSERT(state_);
	previousState_ = manager_.GetAllocator().New<InputState>(manager.GetAllocator(), 2);
	GAINPUT_ASSERT(previousState_);
}

PinchGesture::~PinchGesture()
{
	manager_.GetAllocator().Delete(state_);
	manager_.GetAllocator().Delete(previousState_);
}

void
PinchGesture::Initialize(DeviceId downDevice, DeviceButtonId downButton, 
			DeviceId xAxisDevice, DeviceButtonId xAxis, 
			DeviceId yAxisDevice, DeviceButtonId yAxis, 
			DeviceId down2Device, DeviceButtonId downButton2, 
			DeviceId xAxis2Device, DeviceButtonId xAxis2, 
			DeviceId yAxis2Device, DeviceButtonId yAxis2)
{
	downButton_.deviceId = downDevice;
	downButton_.buttonId = downButton;
	xAxis_.deviceId = xAxisDevice;
	xAxis_.buttonId = xAxis;
	yAxis_.deviceId = yAxisDevice;
	yAxis_.buttonId = yAxis;
	downButton2_.deviceId = down2Device;
	downButton2_.buttonId = downButton2;
	xAxis2_.deviceId = xAxis2Device;
	xAxis2_.buttonId = xAxis2;
	yAxis2_.deviceId = yAxis2Device;
	yAxis2_.buttonId = yAxis2;
}

void
PinchGesture::InternalUpdate(InputDeltaState* delta)
{
	if (downButton_.buttonId == InvalidDeviceButtonId
	 || xAxis_.buttonId == InvalidDeviceButtonId
	 || yAxis_.buttonId == InvalidDeviceButtonId
	 || downButton2_.buttonId == InvalidDeviceButtonId
	 || xAxis2_.buttonId == InvalidDeviceButtonId
	 || yAxis2_.buttonId == InvalidDeviceButtonId)
	{
		return;
	}

	const InputDevice* downDevice = manager_.GetDevice(downButton_.deviceId);
	GAINPUT_ASSERT(downDevice);
	const bool isDown = downDevice->GetBool(downButton_.buttonId);

	const InputDevice* downDevice2 = manager_.GetDevice(downButton2_.deviceId);
	GAINPUT_ASSERT(downDevice2);
	const bool isDown2 = downDevice2->GetBool(downButton2_.buttonId);

	if (!isDown || !isDown2)
	{
		HandleButton(*this, *state_, delta, PinchTriggered, false);
		pinching_ = false;
		return;
	}

	HandleButton(*this, *state_, delta, PinchTriggered, true);

	const InputDevice* xAxisDevice = manager_.GetDevice(xAxis_.deviceId);
	GAINPUT_ASSERT(xAxisDevice);
	const float posX = xAxisDevice->GetFloat(xAxis_.buttonId);
	const InputDevice* yAxisDevice = manager_.GetDevice(yAxis_.deviceId);
	GAINPUT_ASSERT(yAxisDevice);
	const float posY = yAxisDevice->GetFloat(yAxis_.buttonId);

	const InputDevice* xAxis2Device = manager_.GetDevice(xAxis2_.deviceId);
	GAINPUT_ASSERT(xAxis2Device);
	const float posX2 = xAxis2Device->GetFloat(xAxis2_.buttonId);
	const InputDevice* yAxis2Device = manager_.GetDevice(yAxis2_.deviceId);
	GAINPUT_ASSERT(yAxis2Device);
	const float posY2 = yAxis2Device->GetFloat(yAxis2_.buttonId);

	const float xd = posX - posX2;
	const float yd = posY - posY2;
	const float dist = sqrtf(xd*xd + yd*yd);

	if (!pinching_ && dist > 0.0f)
	{
		pinching_ = true;
		initialDistance_ = dist;
		HandleAxis(*this, *state_, delta, PinchScale, 1.0f);
		return;
	}

	HandleAxis(*this, *state_, delta, PinchScale, dist / initialDistance_);
}

}

#endif

