
#ifndef GAINPUTINPUTDEVICETOUCHANDROID_H_
#define GAINPUTINPUTDEVICETOUCHANDROID_H_

#include "GainputInputDeviceTouchImpl.h"
#include "GainputTouchInfo.h"

#include "../../../include/gainput/GainputInputDeltaState.h"
#include "../../../include/gainput/GainputHelpers.h"
#include "../../../include/gainput/GainputInputManager.h"

namespace gainput
{

class InputDeviceTouchImplIos : public InputDeviceTouchImpl
{
public:
	InputDeviceTouchImplIos(InputManager& manager, InputDevice& device, InputState& state, InputState& previousState) :
		manager_(manager),
		device_(device),
		state_(&state),
		previousState_(&previousState),
		nextState_(manager.GetAllocator(), TouchPointCount*TouchDataElems),
		delta_(0),
		touches_(manager.GetAllocator()),
        supportsPressure_(false)
	{
	}
	
	virtual ~InputDeviceTouchImplIos() override
	{
	}

	InputDevice::DeviceVariant GetVariant() const override
	{
		return InputDevice::DV_STANDARD;
	}

	void Update(InputDeltaState* delta) override
	{
		delta_ = delta;
		*state_ = nextState_;
	}

	InputDevice::DeviceState GetState() const override { return InputDevice::DS_OK; }

    bool SupportsPressure() const override
    {
        return supportsPressure_;
    }

    void SetSupportsPressure(bool supports)
    {
        supportsPressure_ = supports;
    }

	void HandleTouch(void* id, float x, float y, float z = 0.f)
	{
		GAINPUT_ASSERT(state_);
		GAINPUT_ASSERT(previousState_);

		int touchIdx = -1;
		for (unsigned i = 0; i < touches_.size(); ++i)
		{
			if (touches_[i] == static_cast<void*>(id))
			{
				touchIdx = i;
				break;
			}
		}

		if (touchIdx == -1)
		{
			for (unsigned i = 0; i < touches_.size(); ++i)
			{
				if (touches_[i] == 0)
				{
					touches_[i] = static_cast<void*>(id);
					touchIdx = i;
					break;
				}
			}
		}

		if (touchIdx == -1)
		{
			touchIdx = static_cast<unsigned>(touches_.size());
			touches_.push_back(static_cast<void*>(id));
		}

		HandleBool(gainput::Touch0Down + touchIdx*4, true);
		HandleFloat(gainput::Touch0X + touchIdx*4, x);
		HandleFloat(gainput::Touch0Y + touchIdx*4, y);
		HandleFloat(gainput::Touch0Pressure + touchIdx*4, z);
	}

	void HandleTouchEnd(void* id, float x, float y, float z = 0.f)
	{
		GAINPUT_ASSERT(state_);
		GAINPUT_ASSERT(previousState_);

		int touchIdx = -1;
		for (unsigned i = 0; i < touches_.size(); ++i)
		{
			if (touches_[i] == static_cast<void*>(id))
			{
				touchIdx = i;
				break;
			}
		}

		// #NOTE: touchIdx is -1 whenever gestures are used
//		GAINPUT_ASSERT(touchIdx != -1);
		if (touchIdx == -1)
		{
			return;
		}

		if(touches_[touchIdx] != 0)
		{
			touches_[touchIdx] = 0;
			HandleBool(gainput::Touch0Down + touchIdx*4, false);
			HandleFloat(gainput::Touch0X + touchIdx*4, x);
			HandleFloat(gainput::Touch0Y + touchIdx*4, y);
			HandleFloat(gainput::Touch0Pressure + touchIdx*4, z);
		}
	}
	
	void HandleGesture(DeviceButtonId buttonId, gainput::GestureChange& gesture)
	{
		if (delta_)
			delta_->AddChange(device_.GetDeviceId(), buttonId, gesture);
	}

private:
	__unused InputManager& manager_;
	InputDevice& device_;
	InputState* state_;
	__unused InputState* previousState_;
	InputState nextState_;
	InputDeltaState* delta_;
	
	
	typedef gainput::Array< void* > TouchList;
	TouchList touches_;

    bool supportsPressure_;
	void HandleBool(DeviceButtonId buttonId, bool value)
	{
        if (delta_)
		{
			const bool oldValue = nextState_.GetBool(buttonId);
			delta_->AddChange(device_.GetDeviceId(), buttonId, oldValue, value);
		}
		nextState_.Set(buttonId, value);
	}

	void HandleFloat(DeviceButtonId buttonId, float value)
	{
       if (delta_)
		{
			const float oldValue = nextState_.GetFloat(buttonId);
			delta_->AddChange(device_.GetDeviceId(), buttonId, oldValue, value);
		}
		nextState_.Set(buttonId, value);
	}
};

}

#endif

