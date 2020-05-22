
#ifndef GAINPUTINPUTDEVICEKEYBOARD_H_
#define GAINPUTINPUTDEVICEKEYBOARD_H_

namespace gainput
{

/// All valid device buttons for InputDeviceKeyboard.
enum Key
{
	KeyInvalid =-1,
	KeyEscape,
	KeyF1,
	KeyF2,
	KeyF3,
	KeyF4,
	KeyF5,
	KeyF6,
	KeyF7,
	KeyF8,
	KeyF9,
	KeyF10,
	KeyF11,
	KeyF12,
	KeyF13,
	KeyF14,
	KeyF15,
	KeyF16,
	KeyF17,
	KeyF18,
	KeyF19,
	KeyPrint,
	KeyScrollLock,
	KeyBreak,

	KeySpace = 0x0020,

	KeyApostrophe = 0x0027,
	KeyComma = 0x002c,
	KeyMinus = 0x002d,
	KeyPeriod = 0x002e,
	KeySlash = 0x002f,

	Key0 = 0x0030,
	Key1 = 0x0031,
	Key2 = 0x0032,
	Key3 = 0x0033,
	Key4 = 0x0034,
	Key5 = 0x0035,
	Key6 = 0x0036,
	Key7 = 0x0037,
	Key8 = 0x0038,
	Key9 = 0x0039,

	KeySemicolon = 0x003b,
	KeyLess = 0x003c,
	KeyEqual = 0x003d,

	KeyA = 0x0041,
	KeyB = 0x0042,
	KeyC = 0x0043,
	KeyD = 0x0044,
	KeyE = 0x0045,
	KeyF = 0x0046,
	KeyG = 0x0047,
	KeyH = 0x0048,
	KeyI = 0x0049,
	KeyJ = 0x004a,
	KeyK = 0x004b,
	KeyL = 0x004c,
	KeyM = 0x004d,
	KeyN = 0x004e,
	KeyO = 0x004f,
	KeyP = 0x0050,
	KeyQ = 0x0051,
	KeyR = 0x0052,
	KeyS = 0x0053,
	KeyT = 0x0054,
	KeyU = 0x0055,
	KeyV = 0x0056,
	KeyW = 0x0057,
	KeyX = 0x0058,
	KeyY = 0x0059,
	KeyZ = 0x005a,

	KeyBracketLeft = 0x005b,
	KeyBackslash = 0x005c,
	KeyBracketRight = 0x005d,

	KeyGrave = 0x0060,

	KeyLeft,
	KeyRight,
	KeyUp,
	KeyDown,
	KeyInsert,
	KeyHome,
	KeyDelete,
	KeyEnd,
	KeyPageUp,
	KeyPageDown,

	KeyNumLock,
	KeyKpEqual,
	KeyKpDivide,
	KeyKpMultiply,
	KeyKpSubtract,
	KeyKpAdd,
	KeyKpEnter,
	KeyKpInsert, // 0
	KeyKpEnd, // 1
	KeyKpDown, // 2
	KeyKpPageDown, // 3
	KeyKpLeft, // 4
	KeyKpBegin, // 5
	KeyKpRight, // 6
	KeyKpHome, // 7
	KeyKpUp, // 8
	KeyKpPageUp, // 9
	KeyKpDelete, // ,

	KeyBackSpace,
	KeyTab,
	KeyReturn,
	KeyCapsLock,
	KeyShiftL,
	KeyCtrlL,
	KeySuperL,
	KeyAltL,
	KeyAltR,
	KeySuperR,
	KeyMenu,
	KeyCtrlR,
	KeyShiftR,

	KeyBack,
	KeySoftLeft,
	KeySoftRight,
	KeyCall,
	KeyEndcall,
	KeyStar,
	KeyPound,
	KeyDpadCenter,
	KeyVolumeUp,
	KeyVolumeDown,
	KeyPower,
	KeyCamera,
	KeyClear,
	KeySymbol,
	KeyExplorer,
	KeyEnvelope,
	KeyEquals,
	KeyAt,
	KeyHeadsethook,
	KeyFocus,
	KeyPlus,
	KeyNotification,
	KeySearch,
	KeyMediaPlayPause,
	KeyMediaStop,
	KeyMediaNext,
	KeyMediaPrevious,
	KeyMediaRewind,
	KeyMediaFastForward,
	KeyMute,
	KeyPictsymbols,
	KeySwitchCharset,

	KeyForward,
	KeyExtra1,
	KeyExtra2,
	KeyExtra3,
	KeyExtra4,
	KeyExtra5,
	KeyExtra6,
	KeyFn,

	KeyCircumflex,
	KeySsharp,
	KeyAcute,
	KeyAltGr,
	KeyNumbersign,
	KeyUdiaeresis,
	KeyAdiaeresis,
	KeyOdiaeresis,
	KeySection,
	KeyAring,
	KeyDiaeresis,
	KeyTwosuperior,
	KeyRightParenthesis,
	KeyDollar,
	KeyUgrave,
	KeyAsterisk,
	KeyColon,
	KeyExclam,

	KeyBraceLeft,
	KeyBraceRight,
	KeySysRq,

	KeyCount_
};


class InputDeviceKeyboardImpl;

/// A keyboard input device.
/**
 * This input device provides support for standard keyboard devices. The valid device buttons are defined
 * in the ::Key enum.
 *
 * This device is implemented on Android NDK, Linux, and Windows. Note that no support for
 * virtual keyboards (on-screen) is present.
 *
 * The raw variants (InputDevice::DV_RAW) of this device do not support text input.
 */
class GAINPUT_LIBEXPORT InputDeviceKeyboard : public InputDevice
{
public:
	/// Initializes the device.
	/**
	 * Instantiate the device using InputManager::CreateDevice().
	 *
	 * \param manager The input manager this device is managed by.
	 * \param device The ID of this device.
	 */
	InputDeviceKeyboard(InputManager& manager, DeviceId device, unsigned index, DeviceVariant variant);
	/// Shuts down the device.
	~InputDeviceKeyboard();

	/// Returns DT_KEYBOARD.
	DeviceType GetType() const override{ return DT_KEYBOARD; }
	DeviceVariant GetVariant() const override;
	const char* GetTypeName() const override{ return "keyboard"; }
	bool IsValidButtonId(DeviceButtonId deviceButton) const override { return deviceButton < KeyCount_; }

	size_t GetAnyButtonDown(DeviceButtonSpec* outButtons, size_t maxButtonCount) const override;

	size_t GetButtonName(DeviceButtonId deviceButton, char* buffer, size_t bufferLength) const override;
	ButtonType GetButtonType(DeviceButtonId deviceButton) const override;
	DeviceButtonId GetButtonByName(const char* name) const override;

	InputState* GetNextInputState() override;
	virtual void ClearButtons() override;
	
	/// Returns if text input is enabled.
	bool IsTextInputEnabled() const;
	/// Sets if text input is enabled and therefore if calling GetNextCharacter() make sense.
	void SetTextInputEnabled(bool enabled);
	/// Returns the next pending input character if text input is enabled.
	wchar_t* GetTextInput(uint32_t* count);
	/// Returns the platform-specific implementation of this device (internal use only).
	InputDeviceKeyboardImpl* GetPimpl() { return impl_; }

protected:
	void InternalUpdate(InputDeltaState* delta) override;
	DeviceState InternalGetState() const override;

private:
	InputDeviceKeyboardImpl* impl_;

	HashMap<Key, const char*> keyNames_;
};

}

#endif

