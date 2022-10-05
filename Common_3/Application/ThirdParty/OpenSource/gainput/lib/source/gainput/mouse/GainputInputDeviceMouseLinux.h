
#ifndef GAINPUTINPUTDEVICEMOUSELINUX_H_
#define GAINPUTINPUTDEVICEMOUSELINUX_H_

#include <X11/Xlib.h>

#include "GainputInputDeviceMouseImpl.h"
#include "../../../include/gainput/GainputHelpers.h"

namespace gainput
{

class InputDeviceMouseImplLinux : public InputDeviceMouseImpl
{
public:
	InputDeviceMouseImplLinux(InputManager& manager, InputDevice& device, InputState& state, InputState& previousState) :
		manager_(manager),
		device_(device),
		state_(&state),
		previousState_(&previousState),
		nextState_(manager.GetAllocator(), MouseButtonCount + MouseAxisCount),
		delta_(0)
	{
		const size_t size = sizeof(bool)*MouseButtonCount;
		isWheel_ = static_cast<bool*>(manager_.GetAllocator().Allocate(size));
		GAINPUT_ASSERT(isWheel_);
		memset(isWheel_, 0, size);
		pressedThisFrame_ = static_cast<bool*>(manager_.GetAllocator().Allocate(size));
		GAINPUT_ASSERT(pressedThisFrame_);
	}

	~InputDeviceMouseImplLinux()
	{
		manager_.GetAllocator().Deallocate(isWheel_);
		manager_.GetAllocator().Deallocate(pressedThisFrame_);
	}

	InputDevice::DeviceVariant GetVariant() const
	{
		return InputDevice::DV_STANDARD;
	}

	virtual InputState * GetNextInputState() override {
		return &nextState_;
	}
	
	void Update(InputDeltaState* delta)
	{
		delta_ = delta;

		// Reset mouse wheel buttons
		for (unsigned i = 0; i < MouseButtonCount; ++i)
		{
			const DeviceButtonId buttonId = i;
			const bool oldValue = previousState_->GetBool(buttonId);
			if (isWheel_[i] && oldValue)
			{
				const bool pressed = false;
				HandleButton(device_, nextState_, delta_, buttonId, pressed);
			}
		}

		*state_ = nextState_;

		memset(pressedThisFrame_, 0, sizeof(bool) * MouseButtonCount);
	}

	void HandleEvent(XEvent& event)
	{
		GAINPUT_ASSERT(state_);
		GAINPUT_ASSERT(previousState_);

		switch (event.type)
		{
		case MotionNotify:
			{
				const XMotionEvent& motionEvent = event.xmotion;
				const float x = float(motionEvent.x);
				const float y = float(motionEvent.y);
				HandleAxis(device_, nextState_, delta_, MouseAxisX, x);
				HandleAxis(device_, nextState_, delta_, MouseAxisY, y);
				break;
			}
		case ButtonPress:
		case ButtonRelease:
			{
				const XButtonEvent& buttonEvent = event.xbutton;
				GAINPUT_ASSERT(buttonEvent.button > 0);
				const DeviceButtonId buttonId = buttonEvent.button-1;
				GAINPUT_ASSERT(buttonId <= MouseButtonMax);
				const bool pressed = event.type == ButtonPress;

				if (!pressed && pressedThisFrame_[buttonId])
				{
					// This is a mouse wheel button. Ignore release now, reset next frame.
					isWheel_[buttonId] = true;
				}
				else if (buttonEvent.button < MouseButtonCount)
				{
					HandleButton(device_, nextState_, delta_, buttonId, pressed);
				}

				if (pressed)
				{
					pressedThisFrame_[buttonId] = true;
				}
				break;
			}
		}
	}

private:
	InputManager& manager_;
	InputDevice& device_;
	bool* isWheel_;
	bool* pressedThisFrame_;
	InputState* state_;
	InputState* previousState_;
	InputState nextState_;
	InputDeltaState* delta_;
};

}

#endif

