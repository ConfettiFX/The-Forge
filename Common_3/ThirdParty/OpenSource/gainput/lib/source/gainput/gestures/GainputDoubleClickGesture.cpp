
#include "../../../include/gainput/gainput.h"
#include "../../../include/gainput/gestures/GainputDoubleClickGesture.h"

#ifdef GAINPUT_ENABLE_DOUBLE_CLICK_GESTURE
#include "../../../include/gainput/GainputInputDeltaState.h"
#include "../../../include/gainput/GainputHelpers.h"

namespace gainput
{

DoubleClickGesture::DoubleClickGesture(InputManager& manager, DeviceId device, unsigned index, DeviceVariant /*variant*/) :
	InputGesture(manager, device, index),
	timeSpan_(1000),
	firstClickTime_(0),
	clicksRegistered_(0),
	clicksTargetCount_(2)
{
	actionButton_.buttonId = InvalidDeviceButtonId;
	xAxis_.buttonId = InvalidDeviceButtonId;
	yAxis_.buttonId = InvalidDeviceButtonId;

	state_ = manager_.GetAllocator().New<InputState>(manager.GetAllocator(), 1);
	GAINPUT_ASSERT(state_);
	previousState_ = manager_.GetAllocator().New<InputState>(manager.GetAllocator(), 1);
	GAINPUT_ASSERT(previousState_);
}

DoubleClickGesture::~DoubleClickGesture()
{
	manager_.GetAllocator().Delete(state_);
	manager_.GetAllocator().Delete(previousState_);
}

void
DoubleClickGesture::Initialize(DeviceId actionButtonDevice, DeviceButtonId actionButton, uint64_t timeSpan)
{
	actionButton_.deviceId = actionButtonDevice;
	actionButton_.buttonId = actionButton;
	xAxis_.buttonId = InvalidDeviceButtonId;
	yAxis_.buttonId = InvalidDeviceButtonId;
	timeSpan_ = timeSpan;
}

void
DoubleClickGesture::Initialize(DeviceId actionButtonDevice, DeviceButtonId actionButton, 
		DeviceId xAxisDevice, DeviceButtonId xAxis, float xTolerance, 
		DeviceId yAxisDevice, DeviceButtonId yAxis, float yTolerance, 
		uint64_t timeSpan)
{
	actionButton_.deviceId = actionButtonDevice;
	actionButton_.buttonId = actionButton;
	xAxis_.deviceId = xAxisDevice;
	xAxis_.buttonId = xAxis;
	xTolerance_ = xTolerance;
	yAxis_.deviceId = yAxisDevice;
	yAxis_.buttonId = yAxis;
	yTolerance_ = yTolerance;
	timeSpan_ = timeSpan;
}

void
DoubleClickGesture::InternalUpdate(InputDeltaState* delta)
{
	if (actionButton_.buttonId == InvalidDeviceButtonId)
	{
		return;
	}

	const InputDevice* actionDevice = manager_.GetDevice(actionButton_.deviceId);
	GAINPUT_ASSERT(actionDevice);

	bool positionValid = false;
	float clickX = 0.0f;
	float clickY = 0.0f;
	if (xAxis_.buttonId != InvalidDeviceButtonId && yAxis_.buttonId != InvalidDeviceButtonId)
	{
		const InputDevice* xAxisDevice = manager_.GetDevice(xAxis_.deviceId);
		GAINPUT_ASSERT(xAxisDevice);
		clickX = xAxisDevice->GetFloat(xAxis_.buttonId);
		const InputDevice* yAxisDevice = manager_.GetDevice(yAxis_.deviceId);
		GAINPUT_ASSERT(yAxisDevice);
		clickY = yAxisDevice->GetFloat(yAxis_.buttonId);
		positionValid = true;
	}

	if (actionDevice->GetBoolPrevious(actionButton_.buttonId) 
			&& !actionDevice->GetBool(actionButton_.buttonId))
	{
		if (clicksRegistered_ == 0)
		{
			firstClickTime_ = manager_.GetTime();
			firstClickX_ = clickX;
			firstClickY_ = clickY;
		}
		++clicksRegistered_;
	}

	if (firstClickTime_ + timeSpan_ < manager_.GetTime())
	{
		clicksRegistered_ = 0;
	}

	if (positionValid && clicksRegistered_ > 1 
		&& (Abs(clickX - firstClickX_) > xTolerance_ || Abs(clickY - firstClickY_) > yTolerance_))
	{
		clicksRegistered_ = 0;
	}

	if (clicksRegistered_ >= clicksTargetCount_)
	{
		clicksRegistered_ = 0;
		HandleButton(*this, *state_, delta, DoubleClickTriggered, true);
	}
	else
	{
		HandleButton(*this, *state_, delta, DoubleClickTriggered, false);
	}
}

}

#endif

