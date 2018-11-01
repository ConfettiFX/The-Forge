
#ifndef GAINPUTINPUTDEVICEKEYBOARDANDROID_H_
#define GAINPUTINPUTDEVICEKEYBOARDANDROID_H_

#ifdef ANDROID
#include <android/native_activity.h>

#include "GainputInputDeviceKeyboardImpl.h"
#include "gainput/GainputHelpers.h"

namespace gainput
{

class InputDeviceKeyboardImplAndroid : public InputDeviceKeyboardImpl
{
public:
	InputDeviceKeyboardImplAndroid(InputManager& manager, InputDevice& device, InputState& state, InputState& previousState) :
		manager_(manager),
		device_(device),
		textInputEnabled_(true),
		dialect_(manager_.GetAllocator()),
		state_(&state),
		previousState_(&previousState),
		nextState_(manager.GetAllocator(), KeyCount_),
		delta_(0)
	{
		dialect_[AKEYCODE_SPACE] = KeySpace;

		dialect_[AKEYCODE_APOSTROPHE] = KeyApostrophe;
		dialect_[AKEYCODE_COMMA] = KeyComma;

		dialect_[AKEYCODE_0] = Key0;
		dialect_[AKEYCODE_1] = Key1;
		dialect_[AKEYCODE_2] = Key2;
		dialect_[AKEYCODE_3] = Key3;
		dialect_[AKEYCODE_4] = Key4;
		dialect_[AKEYCODE_5] = Key5;
		dialect_[AKEYCODE_6] = Key6;
		dialect_[AKEYCODE_7] = Key7;
		dialect_[AKEYCODE_8] = Key8;
		dialect_[AKEYCODE_9] = Key9;

		dialect_[AKEYCODE_A] = KeyA;
		dialect_[AKEYCODE_B] = KeyB;
		dialect_[AKEYCODE_C] = KeyC;
		dialect_[AKEYCODE_D] = KeyD;
		dialect_[AKEYCODE_E] = KeyE;
		dialect_[AKEYCODE_F] = KeyF;
		dialect_[AKEYCODE_G] = KeyG;
		dialect_[AKEYCODE_H] = KeyH;
		dialect_[AKEYCODE_I] = KeyI;
		dialect_[AKEYCODE_J] = KeyJ;
		dialect_[AKEYCODE_K] = KeyK;
		dialect_[AKEYCODE_L] = KeyL;
		dialect_[AKEYCODE_M] = KeyM;
		dialect_[AKEYCODE_N] = KeyN;
		dialect_[AKEYCODE_O] = KeyO;
		dialect_[AKEYCODE_P] = KeyP;
		dialect_[AKEYCODE_Q] = KeyQ;
		dialect_[AKEYCODE_R] = KeyR;
		dialect_[AKEYCODE_S] = KeyS;
		dialect_[AKEYCODE_T] = KeyT;
		dialect_[AKEYCODE_U] = KeyU;
		dialect_[AKEYCODE_V] = KeyV;
		dialect_[AKEYCODE_W] = KeyW;
		dialect_[AKEYCODE_X] = KeyX;
		dialect_[AKEYCODE_Y] = KeyY;
		dialect_[AKEYCODE_Z] = KeyZ;

		dialect_[AKEYCODE_DPAD_LEFT] = KeyLeft;
		dialect_[AKEYCODE_DPAD_RIGHT] = KeyRight;
		dialect_[AKEYCODE_DPAD_UP] = KeyUp;
		dialect_[AKEYCODE_DPAD_DOWN] = KeyDown;
		dialect_[AKEYCODE_HOME] = KeyHome;
		dialect_[AKEYCODE_DEL] = KeyDelete;
		dialect_[AKEYCODE_PAGE_UP] = KeyPageUp;
		dialect_[AKEYCODE_PAGE_DOWN] = KeyPageDown;

		dialect_[AKEYCODE_TAB] = KeyTab;
		dialect_[AKEYCODE_ENTER] = KeyReturn;
		dialect_[AKEYCODE_SHIFT_LEFT] = KeyShiftL;
		dialect_[AKEYCODE_ALT_LEFT] = KeyAltL;
		dialect_[AKEYCODE_ALT_RIGHT] = KeyAltR;
		dialect_[AKEYCODE_MENU] = KeyMenu;
		dialect_[AKEYCODE_SHIFT_RIGHT] = KeyShiftR;

		dialect_[AKEYCODE_BACKSLASH] = KeyBackslash;
		dialect_[AKEYCODE_LEFT_BRACKET] = KeyBracketLeft;
		dialect_[AKEYCODE_RIGHT_BRACKET] = KeyBracketRight;
		dialect_[AKEYCODE_NUM] = KeyNumLock;
		dialect_[AKEYCODE_GRAVE] = KeyGrave;

		dialect_[AKEYCODE_BACK] = KeyBack;
		dialect_[AKEYCODE_SOFT_LEFT] = KeySoftLeft;
		dialect_[AKEYCODE_SOFT_RIGHT] = KeySoftRight;
		dialect_[AKEYCODE_CALL] = KeyCall;
		dialect_[AKEYCODE_ENDCALL] = KeyEndcall;
		dialect_[AKEYCODE_STAR] = KeyStar;
		dialect_[AKEYCODE_POUND] = KeyPound;
		dialect_[AKEYCODE_DPAD_CENTER] = KeyDpadCenter;
		dialect_[AKEYCODE_VOLUME_UP] = KeyVolumeUp;
		dialect_[AKEYCODE_VOLUME_DOWN] = KeyVolumeDown;
		dialect_[AKEYCODE_POWER] = KeyPower;
		dialect_[AKEYCODE_CAMERA] = KeyCamera;
		dialect_[AKEYCODE_CLEAR] = KeyClear;
		dialect_[AKEYCODE_PERIOD] = KeyPeriod;
		dialect_[AKEYCODE_SYM] = KeySymbol;
		dialect_[AKEYCODE_EXPLORER] = KeyExplorer;
		dialect_[AKEYCODE_ENVELOPE] = KeyEnvelope;
		dialect_[AKEYCODE_MINUS] = KeyMinus;
		dialect_[AKEYCODE_EQUALS] = KeyEquals;
		dialect_[AKEYCODE_SEMICOLON] = KeySemicolon;
		dialect_[AKEYCODE_SLASH] = KeySlash;
		dialect_[AKEYCODE_AT] = KeyAt;
		dialect_[AKEYCODE_HEADSETHOOK] = KeyHeadsethook;
		dialect_[AKEYCODE_FOCUS] = KeyFocus;
		dialect_[AKEYCODE_PLUS] = KeyPlus;
		dialect_[AKEYCODE_NOTIFICATION] = KeyNotification;
		dialect_[AKEYCODE_SEARCH] = KeySearch;
		dialect_[AKEYCODE_MEDIA_PLAY_PAUSE] = KeyMediaPlayPause;
		dialect_[AKEYCODE_MEDIA_STOP] = KeyMediaStop;
		dialect_[AKEYCODE_MEDIA_NEXT] = KeyMediaNext;
		dialect_[AKEYCODE_MEDIA_PREVIOUS] = KeyMediaPrevious;
		dialect_[AKEYCODE_MEDIA_REWIND] = KeyMediaRewind;
		dialect_[AKEYCODE_MEDIA_FAST_FORWARD] = KeyMediaFastForward;
		dialect_[AKEYCODE_MUTE] = KeyMute;
		dialect_[AKEYCODE_PICTSYMBOLS] = KeyPictsymbols;
		dialect_[AKEYCODE_SWITCH_CHARSET] = KeySwitchCharset;
	}

	InputDevice::DeviceVariant GetVariant() const override
	{
		return InputDevice::DV_STANDARD;
	}

	void Update(InputDeltaState* delta) override
	{
		delta_ = delta;
		*state_ = nextState_;
	}

	bool IsTextInputEnabled() const override { return textInputEnabled_; }
	void SetTextInputEnabled(bool enabled) override { textInputEnabled_ = enabled; }

	virtual InputState * GetNextInputState() override
    {
		return &nextState_;
	}

	char GetNextCharacter(gainput::DeviceButtonId buttonId) override
	{
		if (!textBuffer_.CanGet())
		{
			return 0;
		}
		InputCharDesc currentDesc = textBuffer_.Get();

		//Removed buffered inputs for which we didn't call GetNextCharacter
		if (buttonId != gainput::InvalidDeviceButtonId && buttonId < gainput::KeyCount_)
		{
			while (currentDesc.buttonId != buttonId)
			{
				if (!textBuffer_.CanGet())
				{
					return 0;
				}
				currentDesc = textBuffer_.Get();
			}
		}

		//if button id was provided then we return the appropriate character
		//else we return the first buffered character
		return currentDesc.inputChar;
	}

	int32_t HandleInput(AInputEvent* event)
	{
		GAINPUT_ASSERT(event);
		GAINPUT_ASSERT(state_);

		if (AInputEvent_getType(event) != AINPUT_EVENT_TYPE_KEY)
		{
			return 0;
		}

		const bool pressed = AKeyEvent_getAction(event) == AKEY_EVENT_ACTION_DOWN;
		const int32_t keyCode = AKeyEvent_getKeyCode(event);
		if (dialect_.count(keyCode))
		{
			const DeviceButtonId buttonId = dialect_[keyCode];
			HandleButton(device_, nextState_, delta_, buttonId, pressed);
			return 1;
		}

		return 0;
	}

	DeviceButtonId Translate(int keyCode) const
	{
		HashMap<unsigned, DeviceButtonId>::const_iterator  it = dialect_.find(keyCode);
		if (it != dialect_.end())
		{
			return it->second;
		}
		return InvalidDeviceButtonId;
	}

private:
	InputManager& manager_;
	InputDevice& device_;
	bool textInputEnabled_;
	struct InputCharDesc
	{
		char inputChar;
		gainput::DeviceButtonId buttonId;
	};
	RingBuffer<GAINPUT_TEXT_INPUT_QUEUE_LENGTH, InputCharDesc> textBuffer_;
	HashMap<unsigned, DeviceButtonId> dialect_;
	InputState* state_;
	InputState* previousState_;
	InputState nextState_;
	InputDeltaState* delta_;
};


}
#endif
#endif

