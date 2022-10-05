
#include "../../../include/gainput/gainput.h"

#ifdef GAINPUT_PLATFORM_MAC
#include "GainputKeyboardKeyNames.h"
#include "../../../include/gainput/GainputInputDeltaState.h"
#include "../../../include/gainput/GainputHelpers.h"
#include "../../../include/gainput/GainputLog.h"

#include "GainputInputDeviceKeyboardMac.h"

#import <Carbon/Carbon.h>
#import <CoreFoundation/CoreFoundation.h>
#import <IOKit/hid/IOHIDManager.h>
#import <IOKit/hid/IOHIDUsageTables.h>


namespace gainput
{

extern bool MacIsApplicationKey();


InputDeviceKeyboardImplMac::InputDeviceKeyboardImplMac(InputManager& manager, InputDevice& device, InputState& state, InputState& previousState) :
	manager_(manager),
	device_(device),
	deviceState_(InputDevice::DS_OK),
	textInputEnabled_(true),
    textCount_(0),
	dialect_(manager_.GetAllocator()),
	state_(&state),
	previousState_(&previousState),
	nextState_(manager.GetAllocator(), KeyCount_),
	delta_(0)
{
	dialect_[kVK_Escape] = KeyEscape;
	dialect_[kVK_F1] = KeyF1;
	dialect_[kVK_F2] = KeyF2;
	dialect_[kVK_F3] = KeyF3;
	dialect_[kVK_F4] = KeyF4;
	dialect_[kVK_F5] = KeyF5;
	dialect_[kVK_F6] = KeyF6;
	dialect_[kVK_F7] = KeyF7;
	dialect_[kVK_F8] = KeyF8;
	dialect_[kVK_F9] = KeyF9;
	dialect_[kVK_F10] = KeyF10;
	dialect_[kVK_F11] = KeyF11;
	dialect_[kVK_F12] = KeyF12;
	dialect_[kVK_F13] = KeyF13;
	dialect_[kVK_F14] = KeyF14;
	dialect_[kVK_F15] = KeyF15;
	dialect_[kVK_F16] = KeyF16;
	dialect_[kVK_F17] = KeyF17;
	dialect_[kVK_F18] = KeyF18;
	dialect_[kVK_F19] = KeyF19;
	//dialect_[kVK_PrintScreen] = KeyPrint;
	//dialect_[kVK_ScrollLock] = KeyScrollLock;
	//dialect_[kVK_Pause] = KeyBreak;

	dialect_[kVK_ANSI_LeftBracket] = KeyBracketLeft;
	dialect_[kVK_ANSI_RightBracket] = KeyBracketRight;
	dialect_[kVK_Space] = KeySpace;

	dialect_[kVK_ANSI_Comma] = KeyComma;
	dialect_[kVK_ANSI_Period] = KeyPeriod;
	dialect_[kVK_ANSI_Slash] = KeySlash;
	dialect_[kVK_ANSI_Quote] = KeyApostrophe;

	dialect_[kVK_ANSI_0] = Key0;
	dialect_[kVK_ANSI_1] = Key1;
	dialect_[kVK_ANSI_2] = Key2;
	dialect_[kVK_ANSI_3] = Key3;
	dialect_[kVK_ANSI_4] = Key4;
	dialect_[kVK_ANSI_5] = Key5;
	dialect_[kVK_ANSI_6] = Key6;
	dialect_[kVK_ANSI_7] = Key7;
	dialect_[kVK_ANSI_8] = Key8;
	dialect_[kVK_ANSI_9] = Key9;

	dialect_[kVK_ANSI_Semicolon] = KeySemicolon;
	dialect_[kVK_ANSI_Equal] = KeyEqual;

	dialect_[kVK_ANSI_A] = KeyA;
	dialect_[kVK_ANSI_B] = KeyB;
	dialect_[kVK_ANSI_C] = KeyC;
	dialect_[kVK_ANSI_D] = KeyD;
	dialect_[kVK_ANSI_E] = KeyE;
	dialect_[kVK_ANSI_F] = KeyF;
	dialect_[kVK_ANSI_G] = KeyG;
	dialect_[kVK_ANSI_H] = KeyH;
	dialect_[kVK_ANSI_I] = KeyI;
	dialect_[kVK_ANSI_J] = KeyJ;
	dialect_[kVK_ANSI_K] = KeyK;
	dialect_[kVK_ANSI_L] = KeyL;
	dialect_[kVK_ANSI_M] = KeyM;
	dialect_[kVK_ANSI_N] = KeyN;
	dialect_[kVK_ANSI_O] = KeyO;
	dialect_[kVK_ANSI_P] = KeyP;
	dialect_[kVK_ANSI_Q] = KeyQ;
	dialect_[kVK_ANSI_R] = KeyR;
	dialect_[kVK_ANSI_S] = KeyS;
	dialect_[kVK_ANSI_T] = KeyT;
	dialect_[kVK_ANSI_U] = KeyU;
	dialect_[kVK_ANSI_V] = KeyV;
	dialect_[kVK_ANSI_W] = KeyW;
	dialect_[kVK_ANSI_X] = KeyX;
	dialect_[kVK_ANSI_Y] = KeyY;
	dialect_[kVK_ANSI_Z] = KeyZ;

	dialect_[kVK_ANSI_LeftBracket] = KeyBracketLeft;
	dialect_[kVK_ANSI_Backslash] = KeyBackslash;
	dialect_[kVK_ANSI_RightBracket] = KeyBracketRight;
	dialect_[kVK_ANSI_Grave] = KeyGrave;
	dialect_[kVK_ANSI_Minus] = KeyMinus;

	dialect_[kVK_LeftArrow] = KeyLeft;
	dialect_[kVK_RightArrow] = KeyRight;
	dialect_[kVK_UpArrow] = KeyUp;
	dialect_[kVK_DownArrow] = KeyDown;
	//dialect_[kVK_Insert] = KeyInsert;
	dialect_[kVK_Home] = KeyHome;
	dialect_[kVK_ForwardDelete] = KeyDelete;
	dialect_[kVK_End] = KeyEnd;
	dialect_[kVK_PageUp] = KeyPageUp;
	dialect_[kVK_PageDown] = KeyPageDown;

	dialect_[kVK_ANSI_KeypadEquals] = KeyKpEqual;
	dialect_[kVK_ANSI_KeypadDivide] = KeyKpDivide;
	dialect_[kVK_ANSI_KeypadMultiply] = KeyKpMultiply;
	dialect_[kVK_ANSI_KeypadMinus] = KeyKpSubtract;
	dialect_[kVK_ANSI_KeypadPlus] = KeyKpAdd;
	dialect_[kVK_ANSI_KeypadEnter] = KeyKpEnter;
	dialect_[kVK_ANSI_Keypad0] = KeyKpInsert;
	dialect_[kVK_ANSI_Keypad1] = KeyKpEnd;
	dialect_[kVK_ANSI_Keypad2] = KeyKpDown;
	dialect_[kVK_ANSI_Keypad3] = KeyKpPageDown;
	dialect_[kVK_ANSI_Keypad4] = KeyKpLeft;
	dialect_[kVK_ANSI_Keypad5] = KeyKpBegin;
	dialect_[kVK_ANSI_Keypad6] = KeyKpRight;
	dialect_[kVK_ANSI_Keypad7] = KeyKpHome;
	dialect_[kVK_ANSI_Keypad8] = KeyKpUp;
	dialect_[kVK_ANSI_Keypad9] = KeyKpPageUp;
	dialect_[kVK_ANSI_KeypadDecimal] = KeyKpDelete;

	dialect_[kVK_Delete] = KeyBackSpace;
	dialect_[kVK_Tab] = KeyTab;
	dialect_[kVK_Return] = KeyReturn;
	dialect_[kVK_CapsLock] = KeyCapsLock;
	dialect_[kVK_Shift] = KeyShiftL;
	dialect_[kVK_Control] = KeyCtrlL;
	dialect_[kVK_Command] = KeySuperL;
	dialect_[kVK_Option] = KeyAltL;
	dialect_[kVK_RightOption] = KeyAltR;
	dialect_[kVK_RightCommand] = KeySuperR;
	dialect_[kVK_RightControl] = KeyCtrlR;
	dialect_[kVK_RightShift] = KeyShiftR;
}
	
void InputDeviceKeyboardImplMac::HandleKey(int keycode, bool isPressed, char * inputChar)
{
	if(dialect_.count(keycode))
	{
		const DeviceButtonId buttonId = dialect_[keycode];
		HandleButton(device_, nextState_, delta_, buttonId, isPressed);
		
        // Process characters in dialect above.
        if (dialect_.count(keycode))
        {
            if(inputChar && inputChar[0] != '\0' && isPressed)
            {
                textBuffer_[textCount_++] = (wchar_t)inputChar[0];
            }
        }
	}
#ifdef GAINPUT_DEBUG
	else
	{
		GAINPUT_LOG("Couldn't find the key translation for regular key code %d\n", keycode);
	}
#endif
}
	
void InputDeviceKeyboardImplMac::HandleFlagsChanged(int keycode)
{
	if(dialect_.count(keycode))
	{
		const DeviceButtonId buttonId = dialect_[keycode];
		
		if(nextState_.GetBool(buttonId))
			HandleButton(device_, nextState_, delta_, buttonId, false);
		else
		{
			HandleButton(device_, nextState_, delta_, buttonId, true);
		}
	}
#ifdef GAINPUT_DEBUG
	else
	{
		GAINPUT_LOG("Couldn't find the key translation for modifier key code %d\n", keycode);
	}
#endif
}
	
	
	
InputDeviceKeyboardImplMac::~InputDeviceKeyboardImplMac()
{
	
}

}

#endif
