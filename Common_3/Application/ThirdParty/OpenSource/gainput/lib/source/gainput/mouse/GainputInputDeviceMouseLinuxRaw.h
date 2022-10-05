
#ifndef GAINPUTINPUTDEVICEMOUSELINUXRAW_H_
#define GAINPUTINPUTDEVICEMOUSELINUXRAW_H_

#include <X11/Xlib.h>

#include "GainputInputDeviceMouseImpl.h"
#include "../../../include/gainput/GainputHelpers.h"

namespace gainput
{

class InputDeviceMouseImplLinuxRaw : public InputDeviceMouseImpl
{
public:
	InputDeviceMouseImplLinuxRaw(InputManager& manager, InputDevice& device, InputState& state, InputState& previousState) :
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
		prevAbsoluteX = 0;
		prevAbsoluteY = 0;
		deltaX = 0;
		deltaY = 0;
		lastTime = 0;
	}

	~InputDeviceMouseImplLinuxRaw()
	{
		manager_.GetAllocator().Deallocate(isWheel_);
		manager_.GetAllocator().Deallocate(pressedThisFrame_);
	}

	InputDevice::DeviceVariant GetVariant() const
	{
		return InputDevice::DV_RAW;
	}
	
	void WarpMouse(const float&x, const float&y)
	{
		prevAbsoluteX = x;
		prevAbsoluteY = y;				  
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

				if(motionEvent.time < lastTime)
					return;
					
				lastTime = motionEvent.time;	
	
				float currDeltaX = motionEvent.x - prevAbsoluteX;
				float currDeltaY = motionEvent.y - prevAbsoluteY;
				prevAbsoluteX = motionEvent.x;
				prevAbsoluteY = motionEvent.y;
				deltaX += currDeltaX;
				deltaY += currDeltaY;
				HandleAxis(device_, nextState_, delta_, MouseAxisX, deltaX);
				HandleAxis(device_, nextState_, delta_, MouseAxisY, deltaY);
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
	float deltaX, deltaY;
	float prevAbsoluteX, prevAbsoluteY;
	InputManager& manager_;
	InputDevice& device_;
	bool* isWheel_;
	bool* pressedThisFrame_;
	InputState* state_;
	InputState* previousState_;
	InputState nextState_;
	InputDeltaState* delta_;

	Time lastTime;
};

}

#endif

