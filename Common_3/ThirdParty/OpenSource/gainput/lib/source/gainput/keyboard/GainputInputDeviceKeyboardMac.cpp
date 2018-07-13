
#include "../../../include/gainput/gainput.h"

#ifdef GAINPUT_PLATFORM_MAC
#include "GainputKeyboardKeyNames.h"
#include "../../../include/gainput/GainputInputDeltaState.h"
#include "../../../include/gainput/GainputHelpers.h"
#include "../../../include/gainput/GainputLog.h"

#include "GainputInputDeviceKeyboardMac.h"

#import <CoreFoundation/CoreFoundation.h>
#import <IOKit/hid/IOHIDManager.h>
#import <IOKit/hid/IOHIDUsageTables.h>


namespace gainput
{

extern bool MacIsApplicationKey();

namespace {

static void OnDeviceInput(void* inContext, IOReturn inResult, void* inSender, IOHIDValueRef value)
{
	if (!MacIsApplicationKey())
	{
		return;
	}

	IOHIDElementRef elem = IOHIDValueGetElement(value);

	InputDeviceKeyboardImplMac* device = reinterpret_cast<InputDeviceKeyboardImplMac*>(inContext);
	GAINPUT_ASSERT(device);

	uint16_t scancode = IOHIDElementGetUsage(elem);

	if (IOHIDElementGetUsagePage(elem) != kHIDPage_KeyboardOrKeypad
			|| scancode == kHIDUsage_KeyboardErrorRollOver
			|| scancode == kHIDUsage_KeyboardPOSTFail
			|| scancode == kHIDUsage_KeyboardErrorUndefined
			|| scancode == kHIDUsage_Keyboard_Reserved)
	{
		return;
	}

	const bool pressed = IOHIDValueGetIntegerValue(value) == 1;

	if (device->dialect_.count(scancode))
	{
		const DeviceButtonId buttonId = device->dialect_[scancode];
        InputManager& manager = device->manager_;
		manager.EnqueueConcurrentChange(device->device_, device->nextState_, device->delta_, buttonId, pressed);
	}
#ifdef GAINPUT_DEBUG
	else
	{
		GAINPUT_LOG("Unmapped key >> scancode: %d\n", int(scancode));
	}
#endif
}

static void OnDeviceConnected(void* inContext, IOReturn inResult, void* inSender, IOHIDDeviceRef inIOHIDDeviceRef)
{
	InputDeviceKeyboardImplMac* device = reinterpret_cast<InputDeviceKeyboardImplMac*>(inContext);
	GAINPUT_ASSERT(device);
	device->deviceState_ = InputDevice::DS_OK;
}

static void OnDeviceRemoved(void* inContext, IOReturn inResult, void* inSender, IOHIDDeviceRef inIOHIDDeviceRef)
{
	InputDeviceKeyboardImplMac* device = reinterpret_cast<InputDeviceKeyboardImplMac*>(inContext);
	GAINPUT_ASSERT(device);
	device->deviceState_ = InputDevice::DS_UNAVAILABLE;
}

}

InputDeviceKeyboardImplMac::InputDeviceKeyboardImplMac(InputManager& manager, InputDevice& device, InputState& state, InputState& previousState) :
	manager_(manager),
	device_(device),
	deviceState_(InputDevice::DS_UNAVAILABLE),
	textInputEnabled_(true),
	dialect_(manager_.GetAllocator()),
	state_(&state),
	previousState_(&previousState),
	nextState_(manager.GetAllocator(), KeyCount_),
	delta_(0)
{
	IOHIDManagerRef ioManager = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDManagerOptionNone);

	if (CFGetTypeID(ioManager) != IOHIDManagerGetTypeID())
	{
		return;
	}

	ioManager_ = ioManager;

	static const unsigned kKeyCount = 2;

	int usagePage = kHIDPage_GenericDesktop;
	int usage = kHIDUsage_GD_Keyboard;

	CFStringRef keys[kKeyCount] = {
		CFSTR(kIOHIDDeviceUsagePageKey),
		CFSTR(kIOHIDDeviceUsageKey),
	};

	CFNumberRef values[kKeyCount] = {
		CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &usagePage),
		CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &usage),
	};

	CFDictionaryRef matchingDict = CFDictionaryCreate(kCFAllocatorDefault,
			(const void **) keys, (const void **) values, kKeyCount,
			&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

	for (unsigned i = 0; i < kKeyCount; ++i)
	{
		CFRelease(keys[i]);
		CFRelease(values[i]);
	}

	IOHIDManagerSetDeviceMatching(ioManager, matchingDict);
	CFRelease(matchingDict);

	IOHIDManagerRegisterDeviceMatchingCallback(ioManager, OnDeviceConnected, this);
	IOHIDManagerRegisterDeviceRemovalCallback(ioManager, OnDeviceRemoved, this);
	IOHIDManagerRegisterInputValueCallback(ioManager, OnDeviceInput, this);

	IOHIDManagerOpen(ioManager, kIOHIDOptionsTypeNone);

	IOHIDManagerScheduleWithRunLoop(ioManager, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);

	dialect_[kHIDUsage_KeyboardEscape] = KeyEscape;
	dialect_[kHIDUsage_KeyboardF1] = KeyF1;
	dialect_[kHIDUsage_KeyboardF2] = KeyF2;
	dialect_[kHIDUsage_KeyboardF3] = KeyF3;
	dialect_[kHIDUsage_KeyboardF4] = KeyF4;
	dialect_[kHIDUsage_KeyboardF5] = KeyF5;
	dialect_[kHIDUsage_KeyboardF6] = KeyF6;
	dialect_[kHIDUsage_KeyboardF7] = KeyF7;
	dialect_[kHIDUsage_KeyboardF8] = KeyF8;
	dialect_[kHIDUsage_KeyboardF9] = KeyF9;
	dialect_[kHIDUsage_KeyboardF10] = KeyF10;
	dialect_[kHIDUsage_KeyboardF11] = KeyF11;
	dialect_[kHIDUsage_KeyboardF12] = KeyF12;
	dialect_[kHIDUsage_KeyboardF13] = KeyF13;
	dialect_[kHIDUsage_KeyboardF14] = KeyF14;
	dialect_[kHIDUsage_KeyboardF15] = KeyF15;
	dialect_[kHIDUsage_KeyboardF16] = KeyF16;
	dialect_[kHIDUsage_KeyboardF17] = KeyF17;
	dialect_[kHIDUsage_KeyboardF18] = KeyF18;
	dialect_[kHIDUsage_KeyboardF19] = KeyF19;
	dialect_[kHIDUsage_KeyboardPrintScreen] = KeyPrint;
	dialect_[kHIDUsage_KeyboardScrollLock] = KeyScrollLock;
	dialect_[kHIDUsage_KeyboardPause] = KeyBreak;

	dialect_[kHIDUsage_KeyboardSpacebar] = KeySpace;

	dialect_[kHIDUsage_KeyboardComma] = KeyComma;
	dialect_[kHIDUsage_KeyboardHyphen] = KeyMinus;
	dialect_[kHIDUsage_KeyboardPeriod] = KeyPeriod;
	dialect_[kHIDUsage_KeyboardSlash] = KeySlash;
	dialect_[kHIDUsage_KeyboardQuote] = KeyApostrophe;

	dialect_[kHIDUsage_Keyboard0] = Key0;
	dialect_[kHIDUsage_Keyboard1] = Key1;
	dialect_[kHIDUsage_Keyboard2] = Key2;
	dialect_[kHIDUsage_Keyboard3] = Key3;
	dialect_[kHIDUsage_Keyboard4] = Key4;
	dialect_[kHIDUsage_Keyboard5] = Key5;
	dialect_[kHIDUsage_Keyboard6] = Key6;
	dialect_[kHIDUsage_Keyboard7] = Key7;
	dialect_[kHIDUsage_Keyboard8] = Key8;
	dialect_[kHIDUsage_Keyboard9] = Key9;

	dialect_[kHIDUsage_KeyboardSemicolon] = KeySemicolon;
	dialect_[kHIDUsage_KeyboardEqualSign] = KeyEqual;

	dialect_[kHIDUsage_KeyboardA] = KeyA;
	dialect_[kHIDUsage_KeyboardB] = KeyB;
	dialect_[kHIDUsage_KeyboardC] = KeyC;
	dialect_[kHIDUsage_KeyboardD] = KeyD;
	dialect_[kHIDUsage_KeyboardE] = KeyE;
	dialect_[kHIDUsage_KeyboardF] = KeyF;
	dialect_[kHIDUsage_KeyboardG] = KeyG;
	dialect_[kHIDUsage_KeyboardH] = KeyH;
	dialect_[kHIDUsage_KeyboardI] = KeyI;
	dialect_[kHIDUsage_KeyboardJ] = KeyJ;
	dialect_[kHIDUsage_KeyboardK] = KeyK;
	dialect_[kHIDUsage_KeyboardL] = KeyL;
	dialect_[kHIDUsage_KeyboardM] = KeyM;
	dialect_[kHIDUsage_KeyboardN] = KeyN;
	dialect_[kHIDUsage_KeyboardO] = KeyO;
	dialect_[kHIDUsage_KeyboardP] = KeyP;
	dialect_[kHIDUsage_KeyboardQ] = KeyQ;
	dialect_[kHIDUsage_KeyboardR] = KeyR;
	dialect_[kHIDUsage_KeyboardS] = KeyS;
	dialect_[kHIDUsage_KeyboardT] = KeyT;
	dialect_[kHIDUsage_KeyboardU] = KeyU;
	dialect_[kHIDUsage_KeyboardV] = KeyV;
	dialect_[kHIDUsage_KeyboardW] = KeyW;
	dialect_[kHIDUsage_KeyboardX] = KeyX;
	dialect_[kHIDUsage_KeyboardY] = KeyY;
	dialect_[kHIDUsage_KeyboardZ] = KeyZ;

	dialect_[kHIDUsage_KeyboardOpenBracket] = KeyBracketLeft;
	dialect_[kHIDUsage_KeyboardBackslash] = KeyBackslash;
	dialect_[kHIDUsage_KeyboardCloseBracket] = KeyBracketRight;

	dialect_[kHIDUsage_KeyboardGraveAccentAndTilde] = KeyGrave;

	dialect_[kHIDUsage_KeyboardLeftArrow] = KeyLeft;
	dialect_[kHIDUsage_KeyboardRightArrow] = KeyRight;
	dialect_[kHIDUsage_KeyboardUpArrow] = KeyUp;
	dialect_[kHIDUsage_KeyboardDownArrow] = KeyDown;
	dialect_[kHIDUsage_KeyboardInsert] = KeyInsert;
	dialect_[kHIDUsage_KeyboardHome] = KeyHome;
	dialect_[kHIDUsage_KeyboardDeleteForward] = KeyDelete;
	dialect_[kHIDUsage_KeyboardEnd] = KeyEnd;
	dialect_[kHIDUsage_KeyboardPageUp] = KeyPageUp;
	dialect_[kHIDUsage_KeyboardPageDown] = KeyPageDown;

	dialect_[kHIDUsage_KeypadNumLock] = KeyNumLock;
	dialect_[kHIDUsage_KeypadEqualSign] = KeyKpEqual;
	dialect_[kHIDUsage_KeypadSlash] = KeyKpDivide;
	dialect_[kHIDUsage_KeypadAsterisk] = KeyKpMultiply;
	dialect_[kHIDUsage_KeypadHyphen] = KeyKpSubtract;
	dialect_[kHIDUsage_KeypadPlus] = KeyKpAdd;
	dialect_[kHIDUsage_KeypadEnter] = KeyKpEnter;
	dialect_[kHIDUsage_Keypad0] = KeyKpInsert;
	dialect_[kHIDUsage_Keypad1] = KeyKpEnd;
	dialect_[kHIDUsage_Keypad2] = KeyKpDown;
	dialect_[kHIDUsage_Keypad3] = KeyKpPageDown;
	dialect_[kHIDUsage_Keypad4] = KeyKpLeft;
	dialect_[kHIDUsage_Keypad5] = KeyKpBegin;
	dialect_[kHIDUsage_Keypad6] = KeyKpRight;
	dialect_[kHIDUsage_Keypad7] = KeyKpHome;
	dialect_[kHIDUsage_Keypad8] = KeyKpUp;
	dialect_[kHIDUsage_Keypad9] = KeyKpPageUp;
	dialect_[kHIDUsage_KeypadPeriod] = KeyKpDelete;

	dialect_[kHIDUsage_KeyboardDeleteOrBackspace] = KeyBackSpace;
	dialect_[kHIDUsage_KeyboardTab] = KeyTab;
	dialect_[kHIDUsage_KeyboardReturnOrEnter] = KeyReturn;
	dialect_[kHIDUsage_KeyboardCapsLock] = KeyCapsLock;
	dialect_[kHIDUsage_KeyboardLeftShift] = KeyShiftL;
	dialect_[kHIDUsage_KeyboardLeftControl] = KeyCtrlL;
	dialect_[kHIDUsage_KeyboardLeftGUI] = KeySuperL;
	dialect_[kHIDUsage_KeyboardLeftAlt] = KeyAltL;
	dialect_[kHIDUsage_KeyboardRightAlt] = KeyAltR;
	dialect_[kHIDUsage_KeyboardRightGUI] = KeySuperR;
	dialect_[kHIDUsage_KeyboardRightControl] = KeyCtrlR;
	dialect_[kHIDUsage_KeyboardRightShift] = KeyShiftR;

	dialect_[kHIDUsage_KeyboardNonUSPound] = KeyNumbersign;
}

InputDeviceKeyboardImplMac::~InputDeviceKeyboardImplMac()
{
	IOHIDManagerRef ioManager = reinterpret_cast<IOHIDManagerRef>(ioManager_);
	IOHIDManagerUnscheduleFromRunLoop(ioManager, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
	IOHIDManagerClose(ioManager, 0);
	CFRelease(ioManager);
}

}

#endif
