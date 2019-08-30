
#ifndef GAINPUTINPUTDEVICEKEYBOARDIOS_H_
#define GAINPUTINPUTDEVICEKEYBOARDIOS_H_

#ifdef GAINPUT_PLATFORM_IOS
#include "GainputInputDeviceKeyboardImpl.h"
#include "../../../include/gainput/GainputInputDeltaState.h"
#include "../../../include/gainput/GainputHelpers.h"
#include "../../../include/gainput/GainputLog.h"

namespace gainput
{

//Updated from gainputIOS.mm using KeyboardView (Virtual keyboard on screen)
class InputDeviceKeyboardImplIOS : public InputDeviceKeyboardImpl
{
public:
	InputDeviceKeyboardImplIOS(InputManager& manager, InputDevice& device, InputState& state, InputState& previousState) :
	manager_(manager),
	device_(device),
	deviceState_(InputDevice::DS_OK),
	textInputEnabled_(true),
	dialect_(manager_.GetAllocator()),
	state_(&state),
	previousState_(&previousState),
	nextState_(manager.GetAllocator(), KeyCount_),
	delta_(0),
	textCount_(0)
	{
		dialect_['['] = KeyBracketLeft;
		dialect_[']'] = KeyBracketRight;
		dialect_[' '] = KeySpace;
		dialect_['\n'] = KeyReturn;
		dialect_['\b'] = KeyBackSpace;
		dialect_[','] = KeyComma;
		dialect_['-'] = KeyMinus;
		dialect_['='] = KeyEqual;
		dialect_['.'] = KeyPeriod;
		dialect_[';'] = KeySemicolon;
		dialect_['/'] = KeySlash;
		dialect_['\''] = KeyApostrophe;
		dialect_['\\'] = KeyBackslash;
		dialect_['`'] = KeyGrave;
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
		dialect_['a'] = KeyA;
		dialect_['b'] = KeyB;
		dialect_['c'] = KeyC;
		dialect_['d'] = KeyD;
		dialect_['e'] = KeyE;
		dialect_['f'] = KeyF;
		dialect_['g'] = KeyG;
		dialect_['h'] = KeyH;
		dialect_['i'] = KeyI;
		dialect_['j'] = KeyJ;
		dialect_['k'] = KeyK;
		dialect_['l'] = KeyL;
		dialect_['m'] = KeyM;
		dialect_['n'] = KeyN;
		dialect_['o'] = KeyO;
		dialect_['p'] = KeyP;
		dialect_['q'] = KeyQ;
		dialect_['r'] = KeyR;
		dialect_['s'] = KeyS;
		dialect_['t'] = KeyT;
		dialect_['u'] = KeyU;
		dialect_['v'] = KeyV;
		dialect_['w'] = KeyW;
		dialect_['x'] = KeyX;
		dialect_['y'] = KeyY;
		dialect_['z'] = KeyZ;
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
	}

	InputDevice::DeviceVariant GetVariant() const override
	{
		return InputDevice::DV_STANDARD;
	}
	
	~InputDeviceKeyboardImplIOS()
	{
		
	}
	
	virtual InputState * GetNextInputState() override {
		return &nextState_;
	}
	
	InputDevice::DeviceState GetState() const override{ return deviceState_; }
	void ClearButtons() override
	{
		//reset button states as we don't have detction for release yet.
		for(uint32_t i = 0 ; i < gainput::KeyCount_; i++)
		{
			HandleButton(device_, nextState_, delta_, gainput::KeyEscape + i, false);
		}
	}

	void Update(InputDeltaState* delta) override
	{
		delta_ = delta;
		*state_ = nextState_;
		textCount_ = 0;
        memset(textBuffer_, 0, sizeof(textBuffer_));
	}
	
	void TriggerBackspace()
	{
		HandleBool(gainput::KeyBackSpace,true);
	}
	void TriggerReturn()
	{
		HandleBool(gainput::KeyReturn,true);
	}
	void TriggerSpace()
	{
		HandleBool(gainput::KeySpace,true);
	}

	void HandleKey(wchar_t inputChar)
	{
		//if we pass in empty char that means backspace
		if(inputChar != '\0' && dialect_.count(inputChar))
		{
            const DeviceButtonId buttonId = dialect_[inputChar];
			textBuffer_[textCount_++] = inputChar;
			HandleBool(buttonId,true);
		}
	}
	
	bool IsTextInputEnabled() const override{ return textInputEnabled_; }
	void SetTextInputEnabled(bool enabled) override{ textInputEnabled_ = enabled; }
	wchar_t* GetTextInput(uint32_t* count) override
	{
		*count = textCount_;
		return textBuffer_;
	}

private:
	InputManager& manager_;
	InputDevice& device_;
	InputDevice::DeviceState deviceState_;
	bool textInputEnabled_;
	wchar_t textBuffer_[GAINPUT_TEXT_INPUT_QUEUE_LENGTH];
	HashMap<unsigned, DeviceButtonId> dialect_;
	InputState* state_;
	__unused InputState* previousState_;
	InputState nextState_;
	InputDeltaState* delta_;
	uint32_t textCount_;

	void HandleBool(DeviceButtonId buttonId, bool value)
	{
		if (delta_)
		{
			const bool oldValue = nextState_.GetBool(buttonId);
			delta_->AddChange(device_.GetDeviceId(), buttonId, oldValue, value);
		}
		nextState_.Set(buttonId, value);
	}
};


}

#endif
#endif
