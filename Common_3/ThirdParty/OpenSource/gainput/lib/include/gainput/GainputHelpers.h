
#ifndef GAINPUTHELPERS_H_
#define GAINPUTHELPERS_H_

#include "GainputLog.h"

namespace gainput
{

	inline void HandleButton(InputDevice& device, InputState& state, InputDeltaState* delta, DeviceButtonId buttonId, bool value)
	{
#ifdef GAINPUT_DEBUG
		if (value != state.GetBool(buttonId))
		{
			GAINPUT_LOG("Button changed: %d, %i\n", buttonId, value);
		}
#endif

		if (delta)
		{
			const bool oldValue = state.GetBool(buttonId);
			delta->AddChange(device.GetDeviceId(), buttonId, oldValue, value);			
		}
		state.Set(buttonId, value);
	}

	inline void HandleAxis(InputDevice& device, InputState& state, InputDeltaState* delta, DeviceButtonId buttonId, float value)
	{
		const float deadZone = device.GetDeadZone(buttonId);
		if (deadZone > 0.0f)
		{
			const float absValue = Abs(value);
			const float sign = value < 0.0f ? -1.0f : 1.0f;
			if (absValue < deadZone)
			{
				value = 0.0f;
			}
			else
			{
				value -= sign*deadZone;
				value *= 1.0f / (1.0f - deadZone);
			}
		}
#ifdef GAINPUT_DEBUG
		if (value != state.GetFloat(buttonId))
		{
			GAINPUT_LOG("Axis changed: %d, %f\n", buttonId, value);
		}
#endif

		if (delta)
		{
			const float oldValue = state.GetFloat(buttonId);
			delta->AddChange(device.GetDeviceId(), buttonId, oldValue, value);			
		}
        state.Set(buttonId, value);
		
	}

}

#endif


