#include "ProfilerInput.h"

#if (PROFILE_ENABLED)

#include "ProfilerBase.h"
#include "ProfilerUI.h"

// Input
#include "../../OS/Input/InputSystem.h"
#include "../../OS/Input/InputMappings.h"

namespace PlatformEvents {
	extern bool skipMouseCapture;
}

extern void ProfileSetPreviousMousePosition(uint32_t x, uint32_t y);

bool microprofiler_input(const ButtonData * button)
{
	static bool clicked = false;

#ifndef TARGET_IOS
	if (InputSystem::IsMouseCaptured())
	{
		clicked = false;
		ProfileMouseButton(clicked, false);
		return true;
	}
#else
    static bool reset_mouse_pos = true;
#endif

	if (button->mUserId == KEY_UI_MOVE)
	{
#if defined(TARGET_IOS)
        if(reset_mouse_pos)
        {
            reset_mouse_pos = false;
            ProfileSetPreviousMousePosition(static_cast<uint32_t>(button->mValue[0]), static_cast<uint32_t>(button->mValue[1]));
        }
#endif
        
		ProfileMousePosition(static_cast<uint32_t>(button->mValue[0]), static_cast<uint32_t>(button->mValue[1]), 0);
	}
#if defined(TARGET_IOS)
    else if(button->mUserId == KEY_CONFIRM)
#else
	else if (button->mUserId == KEY_RIGHT_BUMPER)
#endif
	{
		if (button->mIsTriggered)
        {
            clicked = true;
#if defined(TARGET_IOS)
            reset_mouse_pos = true;
#endif
        }
		else if (button->mIsReleased)
			clicked = false;

		ProfileMouseButton(clicked, false);
	}

	PlatformEvents::skipMouseCapture = (ProfileIsDetailed() ? true : PlatformEvents::skipMouseCapture);
	return true;
}

void ProfileRegisterInput()
{
	InputSystem::RegisterInputEvent(microprofiler_input);
}

#endif
