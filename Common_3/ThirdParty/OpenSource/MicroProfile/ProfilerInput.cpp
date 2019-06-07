#include "../../../OS/Interfaces/IProfiler.h"

#if (PROFILE_ENABLED)

#include "ProfilerBase.h"
#include "ProfilerUI.h"

// Input
#include "../../../OS/Input/InputSystem.h"
#include "../../../OS/Input/InputMappings.h"

namespace PlatformEvents {
	extern bool skipMouseCapture;
}

extern void DrawMouse(bool bDraw);
extern void DrawMousePosition(int32_t x, int32_t y);
extern void ProfileSetPreviousMousePosition(uint32_t x, uint32_t y);

bool microprofiler_input(const ButtonData * button)
{
	static bool clicked = false;

#if defined(_DURANGO)
    static float mouse_x = 0;
    static float mouse_y = 0;
    
    Profile * pProfile = ProfileGet();
    
    // Trigger left stick to toggle display
    if(button->mUserId == KEY_LEFT_STICK && button->mIsTriggered)
    {
        int enabled = pProfile->nDisplay ? 0 : 1;
        ProfileSetDisplayMode(enabled);
        DrawMouse(enabled);
        
        int32_t x = static_cast<int32_t>(mouse_x);
        int32_t y = static_cast<int32_t>(mouse_y);
        ProfileSetPreviousMousePosition(x, y);
        ProfileMousePosition(x, y, 0);
        DrawMousePosition(x, y);
    }
    
    // Only update if we are displaying the profiler
    if(pProfile->nDisplay)
    {
        // Mouse movement
        if(button->mUserId == KEY_LEFT_STICK)
        {
            mouse_x += button->mValue[0];
            mouse_y += button->mValue[1];
            
            int32_t x = static_cast<int32_t>(mouse_x);
            int32_t y = static_cast<int32_t>(mouse_y);
            ProfileMousePosition(x, y, 0);
            DrawMousePosition(x, y);
        }
        // Click
        else if(button->mUserId == KEY_BUTTON_X)
        {
            if(button->mIsTriggered)
                clicked = true;
            else if(button->mIsReleased)
                clicked = false;
            ProfileMouseButton(clicked, false);
        }
        
        PlatformEvents::skipMouseCapture = true;
    }
    
#else
#if !defined(TARGET_IOS) && !defined(__ANDROID__)
    if (InputSystem::IsMouseCaptured())
    {
        clicked = false;
        ProfileMouseButton(clicked, false);
        return true;
    }
#endif

	if (button->mUserId == KEY_UI_MOVE)
		ProfileMousePosition(static_cast<uint32_t>(button->mValue[0]), static_cast<uint32_t>(button->mValue[1]), 0);
#if defined(TARGET_IOS) || defined(__ANDROID__)
    else if(button->mUserId == KEY_CONFIRM)
#else
	else if (button->mUserId == KEY_RIGHT_BUMPER)
#endif
	{
		if (button->mIsTriggered)
        {
            // Start displaying profiler if we previously were not, and we clicked on the top left corner
            const uint32_t pos_x = static_cast<uint32_t>(InputSystem::GetFloatInput(KEY_UI_MOVE, 0));
            const uint32_t pos_y = static_cast<uint32_t>(InputSystem::GetFloatInput(KEY_UI_MOVE, 1));
            const uint32_t reopen_size_x = static_cast<uint32_t>(20 * getDpiScale().x);
            const uint32_t reopen_size_y = static_cast<uint32_t>(20 * getDpiScale().y)	;
            if(ProfileGet()->nDisplay == 0 && pos_x < reopen_size_x && pos_y < reopen_size_y)
                ProfileSetDisplayMode(1);
            
            clicked = true;
#if defined(TARGET_IOS) || defined(__ANDROID__)
            // Reset mouse position, so the display does not disappear due to how MicroProfile handles changes in mouse position
            ProfileSetPreviousMousePosition(pos_x, pos_y);
#endif
        }
		else if (button->mIsReleased)
			clicked = false;

        // Update MicroProfile's input
		ProfileMouseButton(clicked, false);
	}

	PlatformEvents::skipMouseCapture = (ProfileIsDetailed() ? true : PlatformEvents::skipMouseCapture);
#endif
	return true;
}

void profileRegisterInput()
{
	InputSystem::RegisterInputEvent(microprofiler_input);
}

#else

void profileRegisterInput()
{ }

#endif
