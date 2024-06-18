
#ifndef GAINPUTINPUTDEVICEKEYBOARDWINRAW_H_
#define GAINPUTINPUTDEVICEKEYBOARDWINRAW_H_

#include "GainputWindows.h"

#include "../keyboard/GainputInputDeviceKeyboardImpl.h"
#include "../../../include/gainput/GainputHelpers.h"

#ifndef HID_USAGE_PAGE_GENERIC
#define HID_USAGE_PAGE_GENERIC         ((USHORT) 0x01)
#endif
#ifndef HID_USAGE_GENERIC_KEYBOARD
#define HID_USAGE_GENERIC_KEYBOARD     ((USHORT) 0x06)
#endif

namespace gainput
{

class InputDeviceKeyboardImplWinRaw : public InputDeviceKeyboardImpl
{
public:
	InputDeviceKeyboardImplWinRaw(InputManager& manager, InputDevice& device, InputState& state, InputState& previousState) :
		manager_(manager),
		device_(device),
		deviceState_(InputDevice::DS_UNAVAILABLE),
		dialect_(manager_.GetAllocator()),
		state_(&state),
		previousState_(&previousState),
		nextState_(manager.GetAllocator(), KeyCount_),
		delta_(0)
	{
		RAWINPUTDEVICE Rid[1];
		Rid[0].usUsagePage = HID_USAGE_PAGE_GENERIC;
		Rid[0].usUsage = HID_USAGE_GENERIC_KEYBOARD;
		Rid[0].dwFlags = 0;//RIDEV_NOLEGACY;
		Rid[0].hwndTarget = 0;
		if (RegisterRawInputDevices(Rid, 1, sizeof(Rid[0])))
		{
			deviceState_ = InputDevice::DS_OK;
		}

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

		dialect_[VK_NUMLOCK] = KeyNumLock;
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
		dialect_[VK_DECIMAL] = KeyKpDelete;
		dialect_[VK_DIVIDE] = KeyKpDivide;
		dialect_[VK_MULTIPLY] = KeyKpMultiply;
		dialect_[VK_SUBTRACT] = KeyKpSubtract;
		dialect_[VK_ADD] = KeyKpAdd;

		dialect_[0xff] = KeyFn; // Marked as "reserved".
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
		*state_ = nextState_;
	}

	bool IsTextInputEnabled() const { return false; }
	void SetTextInputEnabled(bool enabled) { UNREF_PARAM(enabled); }
	wchar_t* GetTextInput(uint32_t* count) { UNREF_PARAM(count); return NULL; }

	void HandleMessage(const MSG& msg)
	{
		GAINPUT_ASSERT(state_);
		GAINPUT_ASSERT(previousState_);

		if (msg.message != WM_INPUT)
		{
			return;
		}

		UINT dwSize = 40;
		static BYTE lpb[40];
	    
		GetRawInputData((HRAWINPUT)msg.lParam, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER));
		RAWINPUT* raw = (RAWINPUT*)lpb;
	    
		if (raw->header.dwType == RIM_TYPEKEYBOARD) 
		{
			USHORT vkey = raw->data.keyboard.VKey;

			if (vkey == VK_CONTROL)
			{
				if (raw->data.keyboard.Flags & RI_KEY_E0)
					vkey = VK_RCONTROL;
				else
					vkey = VK_LCONTROL;
			}
			else if (vkey == VK_SHIFT)
			{
				if (raw->data.keyboard.MakeCode == 0x36)
					vkey = VK_RSHIFT;
				else
					vkey = VK_LSHIFT;
			}
			else if (vkey == VK_MENU)
			{
				if (raw->data.keyboard.Flags & RI_KEY_E0)
					vkey = VK_RMENU;
				else
					vkey = VK_LMENU;
			}

			if (dialect_.count(vkey))
			{
				const DeviceButtonId buttonId = dialect_[vkey];
#ifdef GAINPUT_DEBUG
				GAINPUT_LOG("%d mapped to: %d\n", vkey, buttonId);
#endif
				const bool pressed = raw->data.keyboard.Message == WM_KEYDOWN || raw->data.keyboard.Message == WM_SYSKEYDOWN;
				HandleButton(device_, nextState_, delta_, buttonId, pressed);
			}
#ifdef GAINPUT_DEBUG
			else
			{
				GAINPUT_LOG("Unmapped raw vkey: %d\n", vkey);
			}
#endif
		}
	}

private:
	InputManager& manager_;
	InputDevice& device_;
	InputDevice::DeviceState deviceState_;
	HashMap<unsigned, DeviceButtonId> dialect_;
	InputState* state_;
	InputState* previousState_;
	InputState nextState_;
	InputDeltaState* delta_;
};


}

#endif

