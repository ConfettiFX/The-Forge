
#ifndef GAINPUTINPUTDEVICEKEYBOARDLINUX_H_
#define GAINPUTINPUTDEVICEKEYBOARDLINUX_H_

#ifdef __linux__

#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <X11/Xutil.h>

#include "GainputInputDeviceKeyboardImpl.h"
#include "../../../include/gainput/GainputHelpers.h"

namespace gainput
{

class InputDeviceKeyboardImplLinux : public InputDeviceKeyboardImpl
{
public:
	InputDeviceKeyboardImplLinux(InputManager& manager, InputDevice& device, InputState& state, InputState& previousState) :
		manager_(manager),
		device_(device),
		textInputEnabled_(true),
		dialect_(manager_.GetAllocator()),
		state_(&state),
		previousState_(&previousState),
		nextState_(manager.GetAllocator(), KeyCount_),
		delta_(0),
		textCount_(0)
	{
		// Cf. <X11/keysymdef.h>
		dialect_[XK_Escape] = KeyEscape;
		dialect_[XK_F1] = KeyF1;
		dialect_[XK_F2] = KeyF2;
		dialect_[XK_F3] = KeyF3;
		dialect_[XK_F4] = KeyF4;
		dialect_[XK_F5] = KeyF5;
		dialect_[XK_F6] = KeyF6;
		dialect_[XK_F7] = KeyF7;
		dialect_[XK_F8] = KeyF8;
		dialect_[XK_F9] = KeyF9;
		dialect_[XK_F10] = KeyF10;
		dialect_[XK_F11] = KeyF11;
		dialect_[XK_F12] = KeyF12;
		dialect_[XK_Print] = KeyPrint;
		dialect_[XK_Scroll_Lock] = KeyScrollLock;
		dialect_[XK_Pause] = KeyBreak;

		dialect_[XK_space] = KeySpace;

		dialect_[XK_apostrophe] = KeyApostrophe;
		dialect_[XK_comma] = KeyComma;
		dialect_[XK_minus] = KeyMinus;
		dialect_[XK_period] = KeyPeriod;
		dialect_[XK_slash] = KeySlash;

		dialect_[XK_0] = Key0;
		dialect_[XK_1] = Key1;
		dialect_[XK_2] = Key2;
		dialect_[XK_3] = Key3;
		dialect_[XK_4] = Key4;
		dialect_[XK_5] = Key5;
		dialect_[XK_6] = Key6;
		dialect_[XK_7] = Key7;
		dialect_[XK_8] = Key8;
		dialect_[XK_9] = Key9;

		dialect_[XK_semicolon] = KeySemicolon;
		dialect_[XK_less] = KeyLess;
		dialect_[XK_equal] = KeyEqual;

		dialect_[XK_a] = KeyA;
		dialect_[XK_b] = KeyB;
		dialect_[XK_c] = KeyC;
		dialect_[XK_d] = KeyD;
		dialect_[XK_e] = KeyE;
		dialect_[XK_f] = KeyF;
		dialect_[XK_g] = KeyG;
		dialect_[XK_h] = KeyH;
		dialect_[XK_i] = KeyI;
		dialect_[XK_j] = KeyJ;
		dialect_[XK_k] = KeyK;
		dialect_[XK_l] = KeyL;
		dialect_[XK_m] = KeyM;
		dialect_[XK_n] = KeyN;
		dialect_[XK_o] = KeyO;
		dialect_[XK_p] = KeyP;
		dialect_[XK_q] = KeyQ;
		dialect_[XK_r] = KeyR;
		dialect_[XK_s] = KeyS;
		dialect_[XK_t] = KeyT;
		dialect_[XK_u] = KeyU;
		dialect_[XK_v] = KeyV;
		dialect_[XK_w] = KeyW;
		dialect_[XK_x] = KeyX;
		dialect_[XK_y] = KeyY;
		dialect_[XK_z] = KeyZ;

		dialect_[XK_bracketleft] = KeyBracketLeft;
		dialect_[XK_backslash] = KeyBackslash;
		dialect_[XK_bracketright] = KeyBracketRight;

		dialect_[XK_grave] = KeyGrave;

		dialect_[XK_Left] = KeyLeft;
		dialect_[XK_Right] = KeyRight;
		dialect_[XK_Up] = KeyUp;
		dialect_[XK_Down] = KeyDown;
		dialect_[XK_Insert] = KeyInsert;
		dialect_[XK_Home] = KeyHome;
		dialect_[XK_Delete] = KeyDelete;
		dialect_[XK_End] = KeyEnd;
		dialect_[XK_Page_Up] = KeyPageUp;
		dialect_[XK_Page_Down] = KeyPageDown;

		dialect_[XK_Num_Lock] = KeyNumLock;
		dialect_[XK_KP_Divide] = KeyKpDivide;
		dialect_[XK_KP_Multiply] = KeyKpMultiply;
		dialect_[XK_KP_Subtract] = KeyKpSubtract;
		dialect_[XK_KP_Add] = KeyKpAdd;
		dialect_[XK_KP_Enter] = KeyKpEnter;
		dialect_[XK_KP_Insert] = KeyKpInsert;
		dialect_[XK_KP_End] = KeyKpEnd;
		dialect_[XK_KP_Down] = KeyKpDown;
		dialect_[XK_KP_Page_Down] = KeyKpPageDown;
		dialect_[XK_KP_Left] = KeyKpLeft;
		dialect_[XK_KP_Begin] = KeyKpBegin;
		dialect_[XK_KP_Right] = KeyKpRight;
		dialect_[XK_KP_Home] = KeyKpHome;
		dialect_[XK_KP_Up] = KeyKpUp;
		dialect_[XK_KP_Page_Up] = KeyKpPageUp;
		dialect_[XK_KP_Delete] = KeyKpDelete;

		dialect_[XK_BackSpace] = KeyBackSpace;
		dialect_[XK_Tab] = KeyTab;
		dialect_[XK_Return] = KeyReturn;
		dialect_[XK_Caps_Lock] = KeyCapsLock;
		dialect_[XK_Shift_L] = KeyShiftL;
		dialect_[XK_Control_L] = KeyCtrlL;
		dialect_[XK_Super_L] = KeySuperL;
		dialect_[XK_Alt_L] = KeyAltL;
		dialect_[XK_Alt_R] = KeyAltR;
		dialect_[XK_Super_R] = KeySuperR;
		dialect_[XK_Menu] = KeyMenu;
		dialect_[XK_Control_R] = KeyCtrlR;
		dialect_[XK_Shift_R] = KeyShiftR;

		dialect_[XK_dead_circumflex] = KeyCircumflex;
		dialect_[XK_ssharp] = KeySsharp;
		dialect_[XK_dead_acute] = KeyAcute;
		dialect_[XK_ISO_Level3_Shift] = KeyAltGr;
		dialect_[XK_plus] = KeyPlus;
		dialect_[XK_numbersign] = KeyNumbersign;
		dialect_[XK_udiaeresis] = KeyUdiaeresis;
		dialect_[XK_adiaeresis] = KeyAdiaeresis;
		dialect_[XK_odiaeresis] = KeyOdiaeresis;
		dialect_[XK_section] = KeySection;
		dialect_[XK_aring] = KeyAring;
		dialect_[XK_dead_diaeresis] = KeyDiaeresis;
		dialect_[XK_twosuperior] = KeyTwosuperior;
		dialect_[XK_parenright] = KeyRightParenthesis;
		dialect_[XK_dollar] = KeyDollar;
		dialect_[XK_ugrave] = KeyUgrave;
		dialect_[XK_asterisk] = KeyAsterisk;
		dialect_[XK_colon] = KeyColon;
		dialect_[XK_exclam] = KeyExclam;
	}

	InputDevice::DeviceVariant GetVariant() const
	{
		return InputDevice::DV_STANDARD;
	}

	void Update(InputDeltaState* delta)
	{
		delta_ = delta;
		*state_ = nextState_;
		textCount_ = 0;
        memset(textBuffer_, 0, sizeof(textBuffer_));
	}

	virtual InputState * GetNextInputState() override {
		return &nextState_;
	}

	bool IsTextInputEnabled() const { return textInputEnabled_; }
	void SetTextInputEnabled(bool enabled) { textInputEnabled_ = enabled; }
	wchar_t* GetTextInput(uint32_t* count)
	{
		*count = textCount_;
		return textBuffer_;
	}

	void HandleEvent(XEvent& event)
	{
		GAINPUT_ASSERT(state_);
		GAINPUT_ASSERT(previousState_);

		if (event.type == KeyPress || event.type == KeyRelease)
		{
			XKeyEvent& keyEvent = event.xkey;
			KeySym keySym = XkbKeycodeToKeysym(keyEvent.display, keyEvent.keycode, 0, 0);
			const bool pressed = event.type == KeyPress;

			if (dialect_.count(keySym))
			{
				if (textInputEnabled_ && pressed)
				{
					char buf[32];
					int len = XLookupString(&keyEvent, buf, 32, 0, 0);
					if (len == 1)
					{
						textBuffer_[textCount_++] = (wchar_t)buf[0];
					}
				}

				const DeviceButtonId buttonId = dialect_[keySym];
				HandleButton(device_, nextState_, delta_, buttonId, pressed);
			}
#ifdef GAINPUT_DEBUG
			else
			{
				char* str = XKeysymToString(keySym);
				GAINPUT_LOG("Unmapped key >> keycode: %d, keysym: %d, str: %s\n", keyEvent.keycode, int(keySym), str);
			}
#endif
		}
	}

private:
	InputManager& manager_;
	InputDevice& device_;
	bool textInputEnabled_;
	wchar_t textBuffer_[GAINPUT_TEXT_INPUT_QUEUE_LENGTH];
	uint32_t textCount_;
	HashMap<unsigned, DeviceButtonId> dialect_;
	InputState* state_;
	InputState* previousState_;
	InputState nextState_;
	InputDeltaState* delta_;
};


}

#endif
#endif

