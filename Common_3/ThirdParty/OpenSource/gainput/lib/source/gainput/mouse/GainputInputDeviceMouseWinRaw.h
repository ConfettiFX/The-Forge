
#ifndef GAINPUTINPUTDEVICEMOUSEWINRAW_H_
#define GAINPUTINPUTDEVICEMOUSEWINRAW_H_

#include "GainputInputDeviceMouseImpl.h"
#include "../../../include/gainput/GainputHelpers.h"
#include "../../../include/gainput/GainputLog.h"

#include "../GainputWindows.h"

#ifndef HID_USAGE_PAGE_GENERIC
#define HID_USAGE_PAGE_GENERIC         ((USHORT) 0x01)
#endif
#ifndef HID_USAGE_GENERIC_MOUSE
#define HID_USAGE_GENERIC_MOUSE        ((USHORT) 0x02)
#endif

namespace gainput
{

class InputDeviceMouseImplWinRaw : public InputDeviceMouseImpl
{
public:
	InputDeviceMouseImplWinRaw(InputManager& manager, InputDevice& device, InputState& state, InputState& previousState) :
		manager_(manager),
		device_(device),
		deviceState_(InputDevice::DS_UNAVAILABLE),
		state_(&state),
		previousState_(&previousState),
		nextState_(manager.GetAllocator(), MouseButtonCount + MouseAxisCount),
		delta_(0),
		buttonsToReset_(manager.GetAllocator())
	{
		RAWINPUTDEVICE Rid[1];
		Rid[0].usUsagePage = HID_USAGE_PAGE_GENERIC;
		Rid[0].usUsage = HID_USAGE_GENERIC_MOUSE;
		Rid[0].dwFlags = 0;//RIDEV_NOLEGACY;
		Rid[0].hwndTarget = 0;
		if (RegisterRawInputDevices(Rid, 1, sizeof(Rid[0])))
		{
			deviceState_ = InputDevice::DS_OK;
			//keep track of all relative changes
			absMousePosX = 0;
			absMousePosY = 0;
			absPrevMousePosX = 0;
			absPrevMousePosY = 0;
		}
	}

	InputDevice::DeviceVariant GetVariant() const
	{
		return InputDevice::DV_RAW;
	}

	InputDevice::DeviceState GetState() const
	{
		return deviceState_;
	}
	
	virtual InputState * GetNextInputState() override {
		return &nextState_;
	}

	void Update(InputDeltaState* delta)
	{
		delta_ = delta;

		for (Array<DeviceButtonId>::const_iterator it = buttonsToReset_.begin();
				it != buttonsToReset_.end();
				++it)
		{

			HandleButton(device_, nextState_, delta, *it, false);
		}
		buttonsToReset_.clear();

		*state_ = nextState_;
	}

	void HandleMessage(const MSG& msg)
	{
		GAINPUT_ASSERT(state_);
		GAINPUT_ASSERT(previousState_);


		if (msg.message != WM_INPUT)
		{
			return;
		}

		UINT dwSize = sizeof(RAWINPUT);
		static BYTE lpb[sizeof(RAWINPUT)];
	    
		GetRawInputData((HRAWINPUT)msg.lParam, RID_INPUT, &lpb, &dwSize, sizeof(RAWINPUTHEADER));
	    
		RAWINPUT* raw = (RAWINPUT*)&lpb;
	    
		if (raw->header.dwType == RIM_TYPEMOUSE) 
		{
			if (raw->data.mouse.usFlags == MOUSE_MOVE_RELATIVE)
			{
				absMousePosX += raw->data.mouse.lLastX;
				absMousePosY += raw->data.mouse.lLastY;

				HandleAxis(device_, nextState_, delta_, MouseAxisX, (float)absMousePosX);
				HandleAxis(device_, nextState_, delta_, MouseAxisY, (float)absMousePosY);

			}
			else if (raw->data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE)
			{
				HandleAxis(device_, nextState_, delta_, MouseAxisX, float( raw->data.mouse.lLastX));
				HandleAxis(device_, nextState_, delta_, MouseAxisY, float( raw->data.mouse.lLastY));
			}
			
			if (raw->data.mouse.usButtonFlags == RI_MOUSE_WHEEL)
			{
				if (SHORT(raw->data.mouse.usButtonData) < 0)
				{
					HandleButton(device_, nextState_, delta_, MouseButtonWheelDown, true);
					buttonsToReset_.push_back(MouseButtonWheelDown);
				}
				else if (SHORT(raw->data.mouse.usButtonData) > 0)
				{
					HandleButton(device_, nextState_, delta_, MouseButtonWheelUp, true);
					buttonsToReset_.push_back(MouseButtonWheelUp);
				}
			}
			else
			{
				if (raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_1_DOWN)
				{
					HandleButton(device_, nextState_,delta_, MouseButton0, true);
				}
				if (raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_1_UP)
				{
					HandleButton(device_, nextState_, delta_, MouseButton0, false);
				}

				if (raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_2_DOWN)
				{
					HandleButton(device_, nextState_, delta_, MouseButton1, true);
				}
				if (raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_2_UP)
				{
					HandleButton(device_, nextState_, delta_, MouseButton1, false);
				}

				if (raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_3_DOWN)
				{
					HandleButton(device_, nextState_, delta_, MouseButton2, true);
				}
				if (raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_3_UP)
				{
					HandleButton(device_, nextState_, delta_, MouseButton2, false);
				}

				if (raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_4_DOWN)
				{
					HandleButton(device_, nextState_, delta_, MouseButton5, true);
				}
				if (raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_4_UP)
				{
					HandleButton(device_, nextState_, delta_, MouseButton5, false);
				}

				if (raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_5_DOWN)
				{
					HandleButton(device_, nextState_, delta_, MouseButton6, true);
				}
				if (raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_5_UP)
				{
					HandleButton(device_, nextState_, delta_, MouseButton6, false);
				}
			}
		}
	}

private:
	InputManager& manager_;
	InputDevice& device_;
	InputDevice::DeviceState deviceState_;
	InputState* state_;
	InputState* previousState_;
	InputState nextState_;
	InputDeltaState* delta_;
	Array<DeviceButtonId> buttonsToReset_;

	int absMousePosX;
	int absMousePosY;
	int absPrevMousePosX;
	int absPrevMousePosY;
};

}

#endif

