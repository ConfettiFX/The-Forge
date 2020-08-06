
#ifndef GAINPUTINPUTDEVICEKEYBOARDWIN_H_
#define GAINPUTINPUTDEVICEKEYBOARDWIN_H_

#include "../GainputWindows.h"

#include "GainputInputDeviceKeyboardImpl.h"
#include "../../../include/gainput/GainputHelpers.h"

namespace gainput
{

class InputDeviceKeyboardImplWin : public InputDeviceKeyboardImpl
{
public:
	InputDeviceKeyboardImplWin(InputManager& manager, InputDevice& device, InputState& state, InputState& previousState) :
		manager_(manager),
		device_(device),
		textInputEnabled_(true),
		textCount_(0),
		dialect_(manager_.GetAllocator()),
		state_(&state),
		previousState_(&previousState),
		nextState_(manager.GetAllocator(), KeyCount_),
		delta_(0)
	{
		dialect_[VK_ESCAPE] = KeyEscape;
		dialect_[VK_F1] = KeyF1;
		dialect_[VK_F2] = KeyF2;
		dialect_[VK_F3] = KeyF3;
		dialect_[VK_F4] = KeyF4;
		dialect_[VK_F5] = KeyF5;
		dialect_[VK_F6] = KeyF6;
		dialect_[VK_F7] = KeyF7;
		dialect_[VK_F8] = KeyF8;
		dialect_[VK_F9] = KeyF9;
		dialect_[VK_F10] = KeyF10;
		dialect_[VK_F11] = KeyF11;
		dialect_[VK_F12] = KeyF12;
		dialect_[VK_PRINT] = KeyPrint;
		dialect_[VK_SCROLL] = KeyScrollLock;
		dialect_[VK_PAUSE] = KeyBreak;

		dialect_[VK_SPACE] = KeySpace;

		dialect_[VK_OEM_5] = KeyApostrophe;
		dialect_[VK_OEM_COMMA] = KeyComma;

		dialect_['0'] = Key0;
		dialect_['1'] = Key1;
		dialect_['2'] = Key2;
		dialect_['3'] = Key3;
		dialect_['4'] = Key4;
		dialect_['5'] = Key5;
		dialect_['6'] = Key6;
		dialect_['7'] = Key7;
		dialect_['8'] = Key8;
		dialect_['9'] = Key9;

		dialect_[VK_DECIMAL] = KeyPeriod;
		dialect_[VK_SUBTRACT] = KeyKpSubtract;
		dialect_[VK_ADD] = KeyKpAdd;
		dialect_[VK_NUMPAD0] = KeyKpInsert;
		dialect_[VK_NUMPAD1] = KeyKpEnd;
		dialect_[VK_NUMPAD2] = KeyKpDown;
		dialect_[VK_NUMPAD3] = KeyKpPageDown;
		dialect_[VK_NUMPAD4] = KeyKpLeft;
		dialect_[VK_NUMPAD5] = KeyKpBegin;
		dialect_[VK_NUMPAD6] = KeyKpRight;
		dialect_[VK_NUMPAD7] = KeyKpHome;
		dialect_[VK_NUMPAD8] = KeyKpUp;
		dialect_[VK_NUMPAD9] = KeyKpPageUp;

		dialect_['A'] = KeyA;
		dialect_['B'] = KeyB;
		dialect_['C'] = KeyC;
		dialect_['D'] = KeyD;
		dialect_['E'] = KeyE;
		dialect_['F'] = KeyF;
		dialect_['G'] = KeyG;
		dialect_['H'] = KeyH;
		dialect_['I'] = KeyI;
		dialect_['J'] = KeyJ;
		dialect_['K'] = KeyK;
		dialect_['L'] = KeyL;
		dialect_['M'] = KeyM;
		dialect_['N'] = KeyN;
		dialect_['O'] = KeyO;
		dialect_['P'] = KeyP;
		dialect_['Q'] = KeyQ;
		dialect_['R'] = KeyR;
		dialect_['S'] = KeyS;
		dialect_['T'] = KeyT;
		dialect_['U'] = KeyU;
		dialect_['V'] = KeyV;
		dialect_['W'] = KeyW;
		dialect_['X'] = KeyX;
		dialect_['Y'] = KeyY;
		dialect_['Z'] = KeyZ;

		dialect_[VK_LEFT] = KeyLeft;
		dialect_[VK_RIGHT] = KeyRight;
		dialect_[VK_UP] = KeyUp;
		dialect_[VK_DOWN] = KeyDown;
		dialect_[VK_INSERT] = KeyInsert;
		dialect_[VK_HOME] = KeyHome;
		dialect_[VK_DELETE] = KeyDelete;
		dialect_[VK_END] = KeyEnd;
		dialect_[VK_PRIOR] = KeyPageUp;
		dialect_[VK_NEXT] = KeyPageDown;

		dialect_[VK_BACK] = KeyBackSpace;
		dialect_[VK_TAB] = KeyTab;
		dialect_[VK_RETURN] = KeyReturn;
		dialect_[VK_CAPITAL] = KeyCapsLock;
		dialect_[VK_LSHIFT] = KeyShiftL;
		dialect_[VK_LCONTROL] = KeyCtrlL;
		dialect_[VK_LWIN] = KeySuperL;
		dialect_[VK_LMENU] = KeyAltL;
		dialect_[VK_RMENU] = KeyAltR;
		dialect_[VK_RWIN] = KeySuperR;
		dialect_[VK_APPS] = KeyMenu;
		dialect_[VK_RCONTROL] = KeyCtrlR;
		dialect_[VK_RSHIFT] = KeyShiftR;

		dialect_[VK_VOLUME_MUTE] = KeyMute;
		dialect_[VK_VOLUME_DOWN] = KeyVolumeDown;
		dialect_[VK_VOLUME_UP] = KeyVolumeUp;
		dialect_[VK_SNAPSHOT] = KeyPrint;
		dialect_[VK_OEM_4] = KeyExtra1;
		dialect_[VK_OEM_6] = KeyExtra2;
		dialect_[VK_BROWSER_BACK] = KeyBack;
		dialect_[VK_BROWSER_FORWARD] = KeyForward;
		dialect_[VK_OEM_MINUS] = KeyMinus;
		dialect_[VK_OEM_PERIOD] = KeyPeriod;
		dialect_[VK_OEM_2] = KeyExtra3;
		dialect_[VK_OEM_PLUS] = KeyPlus;
		dialect_[VK_OEM_7] = KeyExtra4;
		dialect_[VK_OEM_3] = KeyExtra5;
		dialect_[VK_OEM_1] = KeyExtra6;

		//added
		dialect_[VK_MULTIPLY] = KeyKpMultiply;
		dialect_[VK_NUMLOCK] = KeyNumLock;
		dialect_[VK_OEM_CLEAR] = KeyClear;

		dialect_[0xff] = KeyFn; // Marked as "reserved".
		
		memset(textBuffer_, 0, sizeof(textBuffer_));
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

	bool IsTextInputEnabled() const { return textInputEnabled_; }
	void SetTextInputEnabled(bool enabled) { textInputEnabled_ = enabled; }
	wchar_t* GetTextInput(uint32_t* count)
	{
		*count = textCount_;
		return textBuffer_;
	}

	void HandleMessage(const MSG& msg)
	{
		GAINPUT_ASSERT(state_);
		GAINPUT_ASSERT(previousState_);

		const unsigned LParamExtendedKeymask = 1 << 24;

		if (msg.message == WM_CHAR)
		{
			if (!textInputEnabled_)
			{
				return;
			}

			const int key =(int) msg.wParam;
			if (key == 0x08 // backspace 
				|| key == 0x0A // linefeed 
				|| key == 0x1B // escape 
				|| key == 0x09 // tab 
				|| key == 0x0D // carriage return 
				|| key > 255)
			{
				return;
			}
			const wchar_t charKey = (wchar_t)key;
			unsigned char scancode = ((unsigned char*)&msg.lParam)[2];
			unsigned int virtualKey = MapVirtualKey(scancode, MAPVK_VSC_TO_VK);

			if (dialect_.count(virtualKey) || dialect_.count(charKey))
			{
				textBuffer_[textCount_++] = charKey;
			}
			
#ifdef GAINPUT_DEBUG
			GAINPUT_LOG("Text: %c\n", charKey);
#endif
			return;
		}

		bool pressed = false;
		unsigned winKey;
		switch (msg.message)
		{
		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
			pressed = true;
			winKey = (uint32_t)msg.wParam;
			break;
		case WM_KEYUP:
		case WM_SYSKEYUP:
			pressed = false;
			winKey = (uint32_t)msg.wParam;
			break;
		default: // Non-keyboard message
			return;
		}

		if (winKey == VK_CONTROL)
		{
			winKey = (msg.lParam & LParamExtendedKeymask) ? VK_RCONTROL : VK_LCONTROL;
		}
		else if (winKey == VK_MENU)
		{
			winKey = (msg.lParam & LParamExtendedKeymask) ? VK_RMENU : VK_LMENU;
		}
		else if (winKey == VK_SHIFT)
		{
			if (pressed)
			{
				if (GetKeyState(VK_LSHIFT) & 0x8000)
				{
					winKey = VK_LSHIFT;
				}
				else if (GetKeyState(VK_RSHIFT) & 0x8000)
				{
					winKey = VK_RSHIFT;
				}
#ifdef GAINPUT_DEBUG
				else
				{
					GAINPUT_LOG("Not sure which shift this is.\n");
				}
#endif
			}
			else
			{
				if (previousState_->GetBool(KeyShiftL) && !(GetKeyState(VK_LSHIFT) & 0x8000))
				{
					winKey = VK_LSHIFT;
				} 
				else if (previousState_->GetBool(KeyShiftR) && !(GetKeyState(VK_RSHIFT) & 0x8000))
				{
					winKey = VK_RSHIFT;
				}
#ifdef GAINPUT_DEBUG
				else
				{
					GAINPUT_LOG("Not sure which shift this is.\n");
				}
#endif
			}
		}
		// TODO handle l/r alt properly

#ifdef GAINPUT_DEBUG
		GAINPUT_LOG("Keyboard: %d, %i\n", winKey, pressed);
#endif

		if (dialect_.count(winKey))
		{
			const DeviceButtonId buttonId = dialect_[winKey];
			HandleButton(device_, nextState_, delta_, buttonId, pressed);
		}
	}


	virtual InputState * GetNextInputState() override {
		return &nextState_;
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

