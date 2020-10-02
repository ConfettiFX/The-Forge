#ifndef DGA_INPUT
/*
 * Copyright (c) 2018-2020 The Forge Interactive Inc.
 *
 * This file is part of The-Forge
 * (see https://github.com/ConfettiFX/The-Forge).
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include "../../ThirdParty/OpenSource/EASTL/unordered_map.h"
#include "../../ThirdParty/OpenSource/EASTL/unordered_set.h"
#include "../../ThirdParty/OpenSource/EASTL/array.h"

#include "../../ThirdParty/OpenSource/gainput/lib/include/gainput/gainput.h"
#ifdef __APPLE__
#ifdef TARGET_IOS
#include "../../ThirdParty/OpenSource/gainput/lib/include/gainput/GainputIos.h"
#else
#include "../../ThirdParty/OpenSource/gainput/lib/include/gainput/GainputMac.h"
#endif
#endif

#ifdef __linux__
#include <climits>
#endif

#ifdef METAL
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>

#ifdef TARGET_IOS
#include <UIKit/UIView.h>
#include "../../ThirdParty/OpenSource/gainput/lib/source/gainput/touch/GainputInputDeviceTouchIos.h"
#else
#import <Cocoa/Cocoa.h>
#endif
#endif

#include "../Interfaces/ILog.h"
#include "../Interfaces/IOperatingSystem.h"
#include "../Interfaces/IFileSystem.h"
#include "../Interfaces/IInput.h"
#include "../Interfaces/IMemory.h"

#ifdef GAINPUT_PLATFORM_GGP
namespace gainput
{
	extern void SetWindow(void* pData);
}
#endif

#if defined(TARGET_IOS) || defined(__ANDROID__) || defined(NX64)
#define TOUCH_INPUT 1
#endif

#if TOUCH_INPUT
#define TOUCH_DOWN(id) (((id) << 2) + 0)
#define TOUCH_X(id) (((id) << 2) + 1)
#define TOUCH_Y(id) (((id) << 2) + 2)
#define TOUCH_PRESSURE(id) (((id) << 2) + 3)
#define TOUCH_USER(btn) ((btn) >> 2)
#define TOUCH_AXIS(btn) (((btn) % 4) - 1)
#endif

#define MAX_DEVICES 16U

uint32_t MAX_INPUT_GAMEPADS = 4;
uint32_t MAX_INPUT_MULTI_TOUCHES = 4;
uint32_t MAX_INPUT_ACTIONS = 128;

/**
//List of TODO:
//Change HandleButtonBool to mirror HandleButtonFloat and unify GetButtonData + HandleButton common logic for detecting which buttons need to be queried.
//Add potential callback for KeyMappingDescription to make it easier to map custom input to a joystick button/axis.  (Touch -> Mouse -> Joystick need to all be the same on all client code + camera code).
//Fix UI navigation and selection (Unify mouse vs joystick dpad)
//Sometimes Touch joystick gets stuck. Need to investigate further, could be caused by gainput or some bad logic in detecting release of touch.
//Add concept of virtual joystick for unifying button data. It's needed for Touch data Working with Virtual joystick in UI.
//need to add max,min to input mapping
**/

struct InputAction
{
	InputActionDesc mDesc;
};

struct InputSystemImpl : public gainput::InputListener
{
	enum InputControlType
	{
		CONTROL_BUTTON = 0,
		CONTROL_FLOAT,
		CONTROL_AXIS,
		CONTROL_VIRTUAL_JOYSTICK,
		CONTROL_COMPOSITE,
		CONTROL_COMBO,
	};

	enum InputAreaType
	{
		AREA_LEFT = 0,
		AREA_RIGHT,
	};

	struct IControl
	{
		InputAction*     pAction;
		InputControlType mType;
	};

	struct CompositeControl : public IControl
	{
		CompositeControl(const eastl::array<uint32_t, 4>& controls, uint8_t composite)
		{
			memset(this, 0, sizeof(*this));
			mComposite = composite;
			memcpy(mControls, controls.data(), sizeof(mControls));
			mType = CONTROL_COMPOSITE;
		}
		float2          mValue;
		uint32_t        mControls[4];
		uint8_t         mComposite;
		uint8_t         mStarted;
		uint8_t         mPerformed[4];
		uint8_t         mPressedVal[4];
	};

	struct FloatControl : public IControl
	{
		FloatControl(uint16_t start, uint8_t target, bool raw, bool delta)
		{
			memset(this, 0, sizeof(*this));
			mStartButton = start;
			mTarget = target;
			mType = CONTROL_FLOAT;
			mDelta = (1 << (uint8_t)raw) | (uint8_t)delta;
		}
		float3          mValue;
		uint16_t        mStartButton;
		uint8_t         mTarget;
		uint8_t         mStarted;
		uint8_t         mPerformed;
		uint8_t         mDelta;
	};

	struct AxisControl : public IControl
	{
		AxisControl(uint16_t start, uint8_t target, uint8_t axis)
		{
			memset(this, 0, sizeof(*this));
			mStartButton = start;
			mTarget = target;
			mAxisCount = axis;
			mType = CONTROL_AXIS;
		}
		float3          mValue;
		float3          mNewValue;
		uint16_t        mStartButton;
		uint8_t         mTarget;
		uint8_t         mAxisCount;
		uint8_t         mStarted;
		uint8_t         mPerformed;
	};

	struct VirtualJoystickControl : public IControl
	{
		float2   mStartPos;
		float2   mCurrPos;
		float    mOutsideRadius;
		float    mDeadzone;
		float    mScale;
		uint8_t  mTouchIndex;
		uint8_t  mStarted;
		uint8_t  mPerformed;
		uint8_t  mArea;
		uint8_t  mIsPressed;
		uint8_t  mInitialized;
		uint8_t  mActive;
	};

	struct ComboControl : public IControl
	{
		uint16_t mPressButton;
		uint16_t mTriggerButton;
		uint8_t  mPressed;
	};

	const eastl::unordered_map<uint32_t, gainput::MouseButton> mMouseMap =
	{
#ifndef NO_DEFAULT_BINDINGS
		{ InputBindings::BUTTON_SOUTH, gainput::MouseButtonLeft },
		// Following are for UI windows
		{ InputBindings::BUTTON_MOUSE_RIGHT, gainput::MouseButtonRight },
		{ InputBindings::BUTTON_MOUSE_MIDDLE, gainput::MouseButtonMiddle },
		{ InputBindings::BUTTON_MOUSE_SCROLL_UP, gainput::MouseButtonWheelUp },
		{ InputBindings::BUTTON_MOUSE_SCROLL_DOWN, gainput::MouseButtonWheelDown },
#else

		{ InputBindings::BUTTON_MOUSE_LEFT, gainput::MouseButtonLeft },
		{ InputBindings::BUTTON_MOUSE_RIGHT, gainput::MouseButtonRight },
		{ InputBindings::BUTTON_MOUSE_MIDDLE, gainput::MouseButtonMiddle },
		{ InputBindings::BUTTON_MOUSE_SCROLL_UP, gainput::MouseButtonWheelUp },
		{ InputBindings::BUTTON_MOUSE_SCROLL_DOWN, gainput::MouseButtonWheelDown },
		{ InputBindings::BUTTON_MOUSE_5, gainput::MouseButton5},
		{ InputBindings::BUTTON_MOUSE_6, gainput::MouseButton6},
#endif
	};

	const eastl::unordered_map<uint32_t, gainput::Key> mKeyMap =
	{
#ifndef NO_DEFAULT_BINDINGS
		{ InputBindings::BUTTON_EXIT, gainput::KeyEscape },
		{ InputBindings::BUTTON_BACK, gainput::KeyBackSpace },
		{ InputBindings::BUTTON_NORTH, gainput::KeySpace },
		{ InputBindings::BUTTON_R3, gainput::KeyF1 },
		{ InputBindings::BUTTON_L3, gainput::KeyF2 },
		{ InputBindings::BUTTON_DUMP, gainput::KeyF3 },
		// Following are for UI text inputs
		{ InputBindings::BUTTON_KEYLEFT, gainput::KeyLeft},
		{ InputBindings::BUTTON_KEYRIGHT, gainput::KeyRight},
		{ InputBindings::BUTTON_KEYSHIFTL, gainput::KeyShiftL},
		{ InputBindings::BUTTON_KEYSHIFTR, gainput::KeyShiftR},
		{ InputBindings::BUTTON_KEYHOME, gainput::KeyHome},
		{ InputBindings::BUTTON_KEYEND, gainput::KeyEnd},
		{ InputBindings::BUTTON_KEYDELETE, gainput::KeyDelete},
#else
		{ InputBindings::BUTTON_KEYESCAPE, gainput::KeyEscape},
		{ InputBindings::BUTTON_KEYF1, gainput::KeyF1},
		{ InputBindings::BUTTON_KEYF2, gainput::KeyF2},
		{ InputBindings::BUTTON_KEYF3, gainput::KeyF3},
		{ InputBindings::BUTTON_KEYF4, gainput::KeyF4},
		{ InputBindings::BUTTON_KEYF5, gainput::KeyF5},
		{ InputBindings::BUTTON_KEYF6, gainput::KeyF6},
		{ InputBindings::BUTTON_KEYF7, gainput::KeyF7},
		{ InputBindings::BUTTON_KEYF8, gainput::KeyF8},
		{ InputBindings::BUTTON_KEYF9, gainput::KeyF9},
		{ InputBindings::BUTTON_KEYF10, gainput::KeyF10},
		{ InputBindings::BUTTON_KEYF11, gainput::KeyF11},
		{ InputBindings::BUTTON_KEYF12, gainput::KeyF12},
		{ InputBindings::BUTTON_KEYF13, gainput::KeyF13},
		{ InputBindings::BUTTON_KEYF14, gainput::KeyF14},
		{ InputBindings::BUTTON_KEYF15, gainput::KeyF15},
		{ InputBindings::BUTTON_KEYF16, gainput::KeyF16},
		{ InputBindings::BUTTON_KEYF17, gainput::KeyF17},
		{ InputBindings::BUTTON_KEYF18, gainput::KeyF18},
		{ InputBindings::BUTTON_KEYF19, gainput::KeyF19},
		{ InputBindings::BUTTON_KEYPRINT, gainput::KeyPrint},
		{ InputBindings::BUTTON_KEYSCROLLLOCK, gainput::KeyScrollLock},
		{ InputBindings::BUTTON_KEYBREAK, gainput::KeyBreak},
		{ InputBindings::BUTTON_KEYSPACE, gainput::KeySpace},
		{ InputBindings::BUTTON_KEYAPOSTROPHE, gainput::KeyApostrophe},
		{ InputBindings::BUTTON_KEYCOMMA, gainput::KeyComma},
		{ InputBindings::BUTTON_KEYMINUS, gainput::KeyMinus},
		{ InputBindings::BUTTON_KEYPERIOD, gainput::KeyPeriod},
		{ InputBindings::BUTTON_KEYSLASH, gainput::KeySlash},
		{ InputBindings::BUTTON_KEY0, gainput::Key0},
		{ InputBindings::BUTTON_KEY1, gainput::Key1},
		{ InputBindings::BUTTON_KEY2, gainput::Key2},
		{ InputBindings::BUTTON_KEY3, gainput::Key3},
		{ InputBindings::BUTTON_KEY4, gainput::Key4},
		{ InputBindings::BUTTON_KEY5, gainput::Key5},
		{ InputBindings::BUTTON_KEY6, gainput::Key6},
		{ InputBindings::BUTTON_KEY7, gainput::Key7},
		{ InputBindings::BUTTON_KEY8, gainput::Key8},
		{ InputBindings::BUTTON_KEY9, gainput::Key9},
		{ InputBindings::BUTTON_KEYSEMICOLON, gainput::KeySemicolon},
		{ InputBindings::BUTTON_KEYLESS, gainput::KeyLess},
		{ InputBindings::BUTTON_KEYEQUAL, gainput::KeyEqual},
		{ InputBindings::BUTTON_KEYA, gainput::KeyA},
		{ InputBindings::BUTTON_KEYB, gainput::KeyB},
		{ InputBindings::BUTTON_KEYC, gainput::KeyC},
		{ InputBindings::BUTTON_KEYD, gainput::KeyD},
		{ InputBindings::BUTTON_KEYE, gainput::KeyE},
		{ InputBindings::BUTTON_KEYF, gainput::KeyF},
		{ InputBindings::BUTTON_KEYG, gainput::KeyG},
		{ InputBindings::BUTTON_KEYH, gainput::KeyH},
		{ InputBindings::BUTTON_KEYI, gainput::KeyI},
		{ InputBindings::BUTTON_KEYJ, gainput::KeyJ},
		{ InputBindings::BUTTON_KEYK, gainput::KeyK},
		{ InputBindings::BUTTON_KEYL, gainput::KeyL},
		{ InputBindings::BUTTON_KEYM, gainput::KeyM},
		{ InputBindings::BUTTON_KEYN, gainput::KeyN},
		{ InputBindings::BUTTON_KEYO, gainput::KeyO},
		{ InputBindings::BUTTON_KEYP, gainput::KeyP},
		{ InputBindings::BUTTON_KEYQ, gainput::KeyQ},
		{ InputBindings::BUTTON_KEYR, gainput::KeyR},
		{ InputBindings::BUTTON_KEYS, gainput::KeyS},
		{ InputBindings::BUTTON_KEYT, gainput::KeyT},
		{ InputBindings::BUTTON_KEYU, gainput::KeyU},
		{ InputBindings::BUTTON_KEYV, gainput::KeyV},
		{ InputBindings::BUTTON_KEYW, gainput::KeyW},
		{ InputBindings::BUTTON_KEYX, gainput::KeyX},
		{ InputBindings::BUTTON_KEYY, gainput::KeyY},
		{ InputBindings::BUTTON_KEYZ, gainput::KeyZ},
		{ InputBindings::BUTTON_KEYBRACKETLEFT, gainput::KeyBracketLeft},
		{ InputBindings::BUTTON_KEYBACKSLASH, gainput::KeyBackslash},
		{ InputBindings::BUTTON_KEYBRACKETRIGHT, gainput::KeyBracketRight},
		{ InputBindings::BUTTON_KEYGRAVE, gainput::KeyGrave},
		{ InputBindings::BUTTON_KEYLEFT, gainput::KeyLeft},
		{ InputBindings::BUTTON_KEYRIGHT, gainput::KeyRight},
		{ InputBindings::BUTTON_KEYUP, gainput::KeyUp},
		{ InputBindings::BUTTON_KEYDOWN, gainput::KeyDown},
		{ InputBindings::BUTTON_KEYINSERT, gainput::KeyInsert},
		{ InputBindings::BUTTON_KEYHOME, gainput::KeyHome},
		{ InputBindings::BUTTON_KEYDELETE, gainput::KeyDelete},
		{ InputBindings::BUTTON_KEYEND, gainput::KeyEnd},
		{ InputBindings::BUTTON_KEYPAGEUP, gainput::KeyPageUp},
		{ InputBindings::BUTTON_KEYPAGEDOWN, gainput::KeyPageDown},
		{ InputBindings::BUTTON_KEYNUMLOCK, gainput::KeyNumLock},
		{ InputBindings::BUTTON_KEYKPEQUAL, gainput::KeyKpEqual},
		{ InputBindings::BUTTON_KEYKPDIVIDE, gainput::KeyKpDivide},
		{ InputBindings::BUTTON_KEYKPMULTIPLY, gainput::KeyKpMultiply},
		{ InputBindings::BUTTON_KEYKPSUBTRACT, gainput::KeyKpSubtract},
		{ InputBindings::BUTTON_KEYKPADD, gainput::KeyKpAdd},
		{ InputBindings::BUTTON_KEYKPENTER, gainput::KeyKpEnter},
		{ InputBindings::BUTTON_KEYKPINSERT, gainput::KeyKpInsert},
		{ InputBindings::BUTTON_KEYKPEND, gainput::KeyKpEnd},
		{ InputBindings::BUTTON_KEYKPDOWN, gainput::KeyKpDown},
		{ InputBindings::BUTTON_KEYKPPAGEDOWN, gainput::KeyKpPageDown},
		{ InputBindings::BUTTON_KEYKPLEFT, gainput::KeyKpLeft},
		{ InputBindings::BUTTON_KEYKPBEGIN, gainput::KeyKpBegin},
		{ InputBindings::BUTTON_KEYKPRIGHT, gainput::KeyKpRight},
		{ InputBindings::BUTTON_KEYKPHOME, gainput::KeyKpHome},
		{ InputBindings::BUTTON_KEYKPUP, gainput::KeyKpUp},
		{ InputBindings::BUTTON_KEYKPPAGEUP, gainput::KeyKpPageUp},
		{ InputBindings::BUTTON_KEYKPDELETE, gainput::KeyKpDelete},
		{ InputBindings::BUTTON_KEYBACKSPACE, gainput::KeyBackSpace},
		{ InputBindings::BUTTON_KEYTAB, gainput::KeyTab},
		{ InputBindings::BUTTON_KEYRETURN, gainput::KeyReturn},
		{ InputBindings::BUTTON_KEYCAPSLOCK, gainput::KeyCapsLock},
		{ InputBindings::BUTTON_KEYSHIFTL, gainput::KeyShiftL},
		{ InputBindings::BUTTON_KEYCTRLL, gainput::KeyCtrlL},
		{ InputBindings::BUTTON_KEYSUPERL, gainput::KeySuperL},
		{ InputBindings::BUTTON_KEYALTL, gainput::KeyAltL},
		{ InputBindings::BUTTON_KEYALTR, gainput::KeyAltR},
		{ InputBindings::BUTTON_KEYSUPERR, gainput::KeySuperR},
		{ InputBindings::BUTTON_KEYMENU, gainput::KeyMenu},
		{ InputBindings::BUTTON_KEYCTRLR, gainput::KeyCtrlR},
		{ InputBindings::BUTTON_KEYSHIFTR, gainput::KeyShiftR},
		{ InputBindings::BUTTON_KEYBACK, gainput::KeyBack},
		{ InputBindings::BUTTON_KEYSOFTLEFT, gainput::KeySoftLeft},
		{ InputBindings::BUTTON_KEYSOFTRIGHT, gainput::KeySoftRight},
		{ InputBindings::BUTTON_KEYCALL, gainput::KeyCall},
		{ InputBindings::BUTTON_KEYENDCALL, gainput::KeyEndcall},
		{ InputBindings::BUTTON_KEYSTAR, gainput::KeyStar},
		{ InputBindings::BUTTON_KEYPOUND, gainput::KeyPound},
		{ InputBindings::BUTTON_KEYDPADCENTER, gainput::KeyDpadCenter},
		{ InputBindings::BUTTON_KEYVOLUMEUP, gainput::KeyVolumeUp},
		{ InputBindings::BUTTON_KEYVOLUMEDOWN, gainput::KeyVolumeDown},
		{ InputBindings::BUTTON_KEYPOWER, gainput::KeyPower},
		{ InputBindings::BUTTON_KEYCAMERA, gainput::KeyCamera},
		{ InputBindings::BUTTON_KEYCLEAR, gainput::KeyClear},
		{ InputBindings::BUTTON_KEYSYMBOL, gainput::KeySymbol},
		{ InputBindings::BUTTON_KEYEXPLORER, gainput::KeyExplorer},
		{ InputBindings::BUTTON_KEYENVELOPE, gainput::KeyEnvelope},
		{ InputBindings::BUTTON_KEYEQUALS, gainput::KeyEquals},
		{ InputBindings::BUTTON_KEYAT, gainput::KeyAt},
		{ InputBindings::BUTTON_KEYHEADSETHOOK, gainput::KeyHeadsethook},
		{ InputBindings::BUTTON_KEYFOCUS, gainput::KeyFocus},
		{ InputBindings::BUTTON_KEYPLUS, gainput::KeyPlus},
		{ InputBindings::BUTTON_KEYNOTIFICATION, gainput::KeyNotification},
		{ InputBindings::BUTTON_KEYSEARCH, gainput::KeySearch},
		{ InputBindings::BUTTON_KEYMEDIAPLAYPAUSE, gainput::KeyMediaPlayPause},
		{ InputBindings::BUTTON_KEYMEDIASTOP, gainput::KeyMediaStop},
		{ InputBindings::BUTTON_KEYMEDIANEXT, gainput::KeyMediaNext},
		{ InputBindings::BUTTON_KEYMEDIAPREVIOUS, gainput::KeyMediaPrevious},
		{ InputBindings::BUTTON_KEYMEDIAREWIND, gainput::KeyMediaRewind},
		{ InputBindings::BUTTON_KEYMEDIAFASTFORWARD, gainput::KeyMediaFastForward},
		{ InputBindings::BUTTON_KEYMUTE, gainput::KeyMute},
		{ InputBindings::BUTTON_KEYPICTSYMBOLS, gainput::KeyPictsymbols},
		{ InputBindings::BUTTON_KEYSWITCHCHARSET, gainput::KeySwitchCharset},
		{ InputBindings::BUTTON_KEYFORWARD, gainput::KeyForward},
		{ InputBindings::BUTTON_KEYEXTRA1, gainput::KeyExtra1},
		{ InputBindings::BUTTON_KEYEXTRA2, gainput::KeyExtra2},
		{ InputBindings::BUTTON_KEYEXTRA3, gainput::KeyExtra3},
		{ InputBindings::BUTTON_KEYEXTRA4, gainput::KeyExtra4},
		{ InputBindings::BUTTON_KEYEXTRA5, gainput::KeyExtra5},
		{ InputBindings::BUTTON_KEYEXTRA6, gainput::KeyExtra6},
		{ InputBindings::BUTTON_KEYFN, gainput::KeyFn},
		{ InputBindings::BUTTON_KEYCIRCUMFLEX, gainput::KeyCircumflex},
		{ InputBindings::BUTTON_KEYSSHARP, gainput::KeySsharp},
		{ InputBindings::BUTTON_KEYACUTE, gainput::KeyAcute},
		{ InputBindings::BUTTON_KEYALTGR, gainput::KeyAltGr},
		{ InputBindings::BUTTON_KEYNUMBERSIGN, gainput::KeyNumbersign},
		{ InputBindings::BUTTON_KEYUDIAERESIS, gainput::KeyUdiaeresis},
		{ InputBindings::BUTTON_KEYADIAERESIS, gainput::KeyAdiaeresis},
		{ InputBindings::BUTTON_KEYODIAERESIS, gainput::KeyOdiaeresis},
		{ InputBindings::BUTTON_KEYSECTION, gainput::KeySection},
		{ InputBindings::BUTTON_KEYARING, gainput::KeyAring},
		{ InputBindings::BUTTON_KEYDIAERESIS, gainput::KeyDiaeresis},
		{ InputBindings::BUTTON_KEYTWOSUPERIOR, gainput::KeyTwosuperior},
		{ InputBindings::BUTTON_KEYRIGHTPARENTHESIS, gainput::KeyRightParenthesis},
		{ InputBindings::BUTTON_KEYDOLLAR, gainput::KeyDollar},
		{ InputBindings::BUTTON_KEYUGRAVE, gainput::KeyUgrave},
		{ InputBindings::BUTTON_KEYASTERISK, gainput::KeyAsterisk},
		{ InputBindings::BUTTON_KEYCOLON, gainput::KeyColon},
		{ InputBindings::BUTTON_KEYEXCLAM, gainput::KeyExclam},
		{ InputBindings::BUTTON_KEYBRACELEFT, gainput::KeyBraceLeft},
		{ InputBindings::BUTTON_KEYBRACERIGHT, gainput::KeyBraceRight},
		{ InputBindings::BUTTON_KEYSYSRQ, gainput::KeySysRq},

#endif
	};

	const eastl::unordered_map<uint32_t, gainput::PadButton> mGamepadMap =
	{
		{ InputBindings::BUTTON_DPAD_LEFT, gainput::PadButtonLeft },
		{ InputBindings::BUTTON_DPAD_RIGHT, gainput::PadButtonRight },
		{ InputBindings::BUTTON_DPAD_UP, gainput::PadButtonUp },
		{ InputBindings::BUTTON_DPAD_DOWN, gainput::PadButtonDown },
		{ InputBindings::BUTTON_SOUTH, gainput::PadButtonA }, // A/CROSS
		{ InputBindings::BUTTON_EAST, gainput::PadButtonB }, // B/CIRCLE
		{ InputBindings::BUTTON_WEST, gainput::PadButtonX }, // X/SQUARE
		{ InputBindings::BUTTON_NORTH, gainput::PadButtonY }, // Y/TRIANGLE
		{ InputBindings::BUTTON_L1, gainput::PadButtonL1 },
		{ InputBindings::BUTTON_R1, gainput::PadButtonR1 },
		{ InputBindings::BUTTON_L2, gainput::PadButtonL2 },
		{ InputBindings::BUTTON_R2, gainput::PadButtonR2 },
		{ InputBindings::BUTTON_L3, gainput::PadButtonL3 }, // LEFT THUMB
		{ InputBindings::BUTTON_R3, gainput::PadButtonR3 }, // RIGHT THUMB
		{ InputBindings::BUTTON_START, gainput::PadButtonStart },
		{ InputBindings::BUTTON_SELECT, gainput::PadButtonSelect },
		{ InputBindings::BUTTON_TOUCH, gainput::PadButton17 }, // PS BUTTON
		{ InputBindings::BUTTON_HOME, gainput::PadButtonHome}, // PS BUTTON
	};

	const eastl::unordered_map<uint32_t, AxisControl> mGamepadAxisMap =
	{
		{ InputBindings::FLOAT_L2, { (uint16_t)gainput::PadButtonAxis4, 1, 1 } },
		{ InputBindings::FLOAT_R2, { (uint16_t)gainput::PadButtonAxis5, 1, 1 } },
		{ InputBindings::FLOAT_LEFTSTICK, { (uint16_t)gainput::PadButtonLeftStickX, (1 << 1) | 1, 2 } },
		{ InputBindings::FLOAT_RIGHTSTICK, { (uint16_t)gainput::PadButtonRightStickX, (1 << 1) | 1, 2 } },
	};

#ifndef NO_DEFAULT_BINDINGS
	const eastl::unordered_map<uint32_t, CompositeControl> mGamepadCompositeMap
	{
		{ InputBindings::FLOAT_LEFTSTICK, { { gainput::KeyD, gainput::KeyA, gainput::KeyW, gainput::KeyS }, 4 } },
	};
#endif

	const eastl::unordered_map<uint32_t, FloatControl> mGamepadFloatMap =
	{
		{ InputBindings::FLOAT_RIGHTSTICK, { (uint16_t)gainput::MouseAxisX, (1 << 1) | 1, true, true } },
	};

	/// Maps the gainput button to the InputBindings::Binding enum
	eastl::vector<uint32_t>                  mControlMapReverse[MAX_DEVICES];
	/// List of all input controls per device
	eastl::vector<eastl::vector<IControl*> > mControls[MAX_DEVICES];
	/// List of gestures
	eastl::vector<InputAction*>              mGestureControls;
	/// List of all text input actions
	/// These actions will be invoked everytime there is a text character typed on a physical / virtual keyboard
	eastl::vector<InputAction*>              mTextInputControls;
	/// List of controls which need to be canceled at the end of the frame
	eastl::unordered_set<FloatControl*>      mFloatDeltaControlCancelQueue;
	eastl::unordered_set<IControl*>          mButtonControlPerformQueue;

	eastl::vector<InputAction>               mActions;
	eastl::vector<IControl*>                 mControlPool;
#if TOUCH_INPUT
	float2                                   mTouchPositions[gainput::TouchCount_ >> 2];
#else
	float2                                   mMousePosition;
#endif

	/// Window pointer passed by the app
	/// Input capture will be performed on this window
	WindowsDesc*                             pWindow = NULL;

	/// Gainput Manager which lets us talk with the gainput backend
	gainput::InputManager*                   pInputManager = NULL;
#ifdef __APPLE__
	void*                                    pGainputView = NULL;
#endif

	InputDeviceType*                         pDeviceTypes;
	gainput::DeviceId*                       pGamepadDeviceIDs;
	gainput::DeviceId                        mMouseDeviceID;
	gainput::DeviceId                        mRawMouseDeviceID;
	gainput::DeviceId                        mKeyboardDeviceID;
	gainput::DeviceId                        mTouchDeviceID;

	bool                                     mVirtualKeyboardActive;
	bool                                     mInputCaptured;
	bool                                     mDefaultCapture;

	bool Init(WindowsDesc* window)
	{
		pWindow = window;

#ifdef GAINPUT_PLATFORM_GGP
		gainput::SetWindow(pWindow->handle.window);
#endif

		// Defaults
		mVirtualKeyboardActive = false;
		mDefaultCapture = true;
		mInputCaptured = false;

		pGamepadDeviceIDs = (gainput::DeviceId*)tf_calloc(MAX_INPUT_GAMEPADS, sizeof(gainput::DeviceId));
		pDeviceTypes = (InputDeviceType*)tf_calloc(MAX_INPUT_GAMEPADS + 4, sizeof(InputDeviceType));

		// Default device ids
		mMouseDeviceID = gainput::InvalidDeviceId;
		mRawMouseDeviceID = gainput::InvalidDeviceId;
		mKeyboardDeviceID = gainput::InvalidDeviceId;
		for (uint32_t i = 0; i < MAX_INPUT_GAMEPADS; ++i)
			pGamepadDeviceIDs[i] = gainput::InvalidDeviceId;
		mTouchDeviceID = gainput::InvalidDeviceId;

		// create input manager
		pInputManager = tf_new(gainput::InputManager);
		ASSERT(pInputManager);
#ifdef _WIN32
		pInputManager->window_instance_ = window->handle.window;
#endif

		// create all necessary devices
		mMouseDeviceID = pInputManager->CreateDevice<gainput::InputDeviceMouse>();
		mRawMouseDeviceID = pInputManager->CreateDevice<gainput::InputDeviceMouse>(gainput::InputDevice::AutoIndex, gainput::InputDeviceMouse::DV_RAW);
		mKeyboardDeviceID = pInputManager->CreateDevice<gainput::InputDeviceKeyboard>();
		mTouchDeviceID = pInputManager->CreateDevice<gainput::InputDeviceTouch>();
		for (uint32_t i = 0; i < MAX_INPUT_GAMEPADS; ++i)
			pGamepadDeviceIDs[i] = pInputManager->CreateDevice<gainput::InputDevicePad>();

		// Assign device types
		pDeviceTypes[mMouseDeviceID] = InputDeviceType::INPUT_DEVICE_MOUSE;
		pDeviceTypes[mRawMouseDeviceID] = InputDeviceType::INPUT_DEVICE_MOUSE;
		pDeviceTypes[mKeyboardDeviceID] = InputDeviceType::INPUT_DEVICE_KEYBOARD;
		pDeviceTypes[mTouchDeviceID] = InputDeviceType::INPUT_DEVICE_TOUCH;

		// Create control maps
		mControls[mKeyboardDeviceID].resize(gainput::KeyCount_);
		mControls[mMouseDeviceID].resize(gainput::MouseButtonCount_);
		mControls[mRawMouseDeviceID].resize(gainput::MouseButtonCount_);
		mControls[mTouchDeviceID].resize(gainput::TouchCount_);

		mControlMapReverse[mMouseDeviceID] = eastl::vector<uint32_t>(gainput::MouseButtonCount, UINT_MAX);
		mControlMapReverse[mKeyboardDeviceID] = eastl::vector<uint32_t>(gainput::KeyCount_, UINT_MAX);
		mControlMapReverse[mTouchDeviceID] = eastl::vector<uint32_t>(gainput::TouchCount_, UINT_MAX);

		for (decltype(mMouseMap)::const_iterator it = mMouseMap.begin(); it != mMouseMap.end(); ++it)
			mControlMapReverse[mMouseDeviceID][it->second] = it->first;

		for (decltype(mKeyMap)::const_iterator it = mKeyMap.begin(); it != mKeyMap.end(); ++it)
			mControlMapReverse[mKeyboardDeviceID][it->second] = it->first;

		// Every touch can map to the same button
		for (uint32_t i = 0; i < gainput::TouchCount_; ++i)
			mControlMapReverse[mTouchDeviceID][i] = InputBindings::BUTTON_SOUTH;

		for (uint32_t i = 0; i < MAX_INPUT_GAMEPADS; ++i)
		{
			gainput::DeviceId deviceId = pGamepadDeviceIDs[i];

			pDeviceTypes[deviceId] = InputDeviceType::INPUT_DEVICE_GAMEPAD;
			mControls[deviceId].resize(gainput::PadButtonMax_);
			mControlMapReverse[deviceId] = eastl::vector<uint32_t>(gainput::PadButtonMax_, UINT_MAX);

			for (decltype(mGamepadMap)::const_iterator it = mGamepadMap.begin(); it != mGamepadMap.end(); ++it)
				mControlMapReverse[deviceId][it->second] = it->first;
		}

		mActions.reserve(MAX_INPUT_ACTIONS);

		pInputManager->AddListener(this);

		return InitSubView();
	}

	void Exit()
	{
		ASSERT(pInputManager);

		for (uint32_t i = 0; i < (uint32_t)mControlPool.size(); ++i)
			tf_free(mControlPool[i]);

		tf_free(pGamepadDeviceIDs);
		tf_free(pDeviceTypes);

		ShutdownSubView();
		tf_delete(pInputManager);
	}

	void Update(uint32_t width, uint32_t height)
	{
		ASSERT(pInputManager);

		for (FloatControl* pControl : mFloatDeltaControlCancelQueue)
		{
			pControl->mStarted = 0;
			pControl->mPerformed = 0;
			pControl->mValue = float3(0.0f);

			if (pControl->pAction->mDesc.pFunction)
			{
				InputActionContext ctx = {};
				ctx.pUserData = pControl->pAction->mDesc.pUserData;
				ctx.mPhase = INPUT_ACTION_PHASE_CANCELED;
				ctx.pCaptured = &mDefaultCapture;
#if TOUCH_INPUT
				ctx.pPosition = &mTouchPositions[pControl->pAction->mDesc.mUserId];
#else
				ctx.pPosition = &mMousePosition;
#endif
				pControl->pAction->mDesc.pFunction(&ctx);
			}
		}

#if TOUCH_INPUT
		for (IControl* pControl : mButtonControlPerformQueue)
		{
			InputActionContext ctx = {};
			ctx.pUserData = pControl->pAction->mDesc.pUserData;
			ctx.mPhase = INPUT_ACTION_PHASE_PERFORMED;
			ctx.pCaptured = &mDefaultCapture;
			ctx.mPhase = INPUT_ACTION_PHASE_PERFORMED;
			ctx.mBinding = InputBindings::BUTTON_SOUTH;
			ctx.pPosition = &mTouchPositions[pControl->pAction->mDesc.mUserId];
			ctx.mBool = true;
			pControl->pAction->mDesc.pFunction(&ctx);
		}
#endif

		mButtonControlPerformQueue.clear();
		mFloatDeltaControlCancelQueue.clear();

		gainput::InputDeviceKeyboard* keyboard = (gainput::InputDeviceKeyboard*)pInputManager->GetDevice(mKeyboardDeviceID);
		if (keyboard)
		{
			uint32_t count = 0;
			wchar_t* pText = keyboard->GetTextInput(&count);
			if (count)
			{
				InputActionContext ctx = {};
				ctx.pText = pText;
				ctx.mPhase = INPUT_ACTION_PHASE_PERFORMED;
				for (InputAction* pAction : mTextInputControls)
				{
					ctx.pUserData = pAction->mDesc.pUserData;
					if (!pAction->mDesc.pFunction(&ctx))
						break;
				}
			}
		}

		// update gainput manager
		pInputManager->SetDisplaySize(width, height);
		pInputManager->Update();

#if defined(__linux__) && !defined(__ANDROID__) && !defined(GAINPUT_PLATFORM_GGP)
		//this needs to be done before updating the events
		//that way current frame data will be delta after resetting mouse position
		if (mInputCaptured)
		{
			ASSERT(pWindow);

			float x = 0;
			float y = 0;
			x = (pWindow->windowedRect.right - pWindow->windowedRect.left) / 2;
			y = (pWindow->windowedRect.bottom - pWindow->windowedRect.top) / 2;
			XWarpPointer(pWindow->handle.display, None, pWindow->handle.window, 0, 0, 0, 0, x, y);
			gainput::InputDevice* device = pInputManager->GetDevice(mRawMouseDeviceID);
			device->WarpMouse(x, y);
			XFlush(pWindow->handle.display);
		}
#endif
	}

	template<typename T>
	T* AllocateControl()
	{
		T* pControl = (T*)tf_calloc(1, sizeof(T));
		mControlPool.emplace_back(pControl);
		return pControl;
	}

	InputAction* AddInputAction(const InputActionDesc* pDesc)
	{
		ASSERT(pDesc);

		mActions.emplace_back(InputAction{});
		InputAction* pAction = &mActions.back();
		ASSERT(pAction);

		pAction->mDesc = *pDesc;

#if defined(TARGET_IOS)
		if (pGainputView && InputBindings::GESTURE_BINDINGS_BEGIN <= pDesc->mBinding && InputBindings::GESTURE_BINDINGS_END >= pDesc->mBinding)
		{
			const InputBindings::GestureDesc* pGesture = pDesc->pGesture;
			ASSERT(pGesture);

			GainputView* view = (__bridge GainputView*)pGainputView;
			uint32_t gestureId = (uint32_t)mGestureControls.size();
			gainput::GestureConfig gestureConfig = {};
			gestureConfig.mType = (gainput::GestureType)(pDesc->mBinding - InputBindings::GESTURE_BINDINGS_BEGIN);
			gestureConfig.mMaxNumberOfTouches = pGesture->mMaxNumberOfTouches;
			gestureConfig.mMinNumberOfTouches = pGesture->mMinNumberOfTouches;
			gestureConfig.mMinimumPressDuration = pGesture->mMinimumPressDuration;
			gestureConfig.mNumberOfTapsRequired = pGesture->mNumberOfTapsRequired;
			[view addGestureMapping:gestureId withConfig:gestureConfig];
			mGestureControls.emplace_back(pAction);
			return pAction;
		}
#endif

		if (pDesc->mBinding == InputBindings::TEXT)
		{
			ASSERT(pDesc->pFunction);
			mTextInputControls.emplace_back(pAction);
			return pAction;
		}

		const uint32_t control = pDesc->mBinding;
		const gainput::DeviceId gamepadDeviceId = pGamepadDeviceIDs[pDesc->mUserId];

		if (InputBindings::BUTTON_ANY == control)
		{
			IControl* pControl = AllocateControl<IControl>();
			ASSERT(pControl);

			pControl->mType = CONTROL_BUTTON;
			pControl->pAction = pAction;

			for (decltype(mGamepadMap)::const_iterator it = mGamepadMap.begin(); it != mGamepadMap.end(); ++it)
				mControls[gamepadDeviceId][it->second].emplace_back(pControl);
			for (decltype(mKeyMap)::const_iterator it = mKeyMap.begin(); it != mKeyMap.end(); ++it)
				mControls[mKeyboardDeviceID][it->second].emplace_back(pControl);
#if TOUCH_INPUT
			mControls[mTouchDeviceID][TOUCH_DOWN(pDesc->mUserId)].emplace_back(pControl);
#else
			for (decltype(mMouseMap)::const_iterator it = mMouseMap.begin(); it != mMouseMap.end(); ++it)
				mControls[mMouseDeviceID][it->second].emplace_back(pControl);
#endif
			return pAction;
		}
		else if (InputBindings::BUTTON_FULLSCREEN == control)
		{
			ComboControl* pControl = AllocateControl<ComboControl>();
			ASSERT(pControl);

			pControl->mType = CONTROL_COMBO;
			pControl->pAction = pAction;
			pControl->mPressButton = gainput::KeyAltL;
			pControl->mTriggerButton = gainput::KeyReturn;
			mControls[mKeyboardDeviceID][gainput::KeyReturn].emplace_back(pControl);
			mControls[mKeyboardDeviceID][gainput::KeyAltL].emplace_back(pControl);
		}
		else if (InputBindings::BUTTON_DUMP == control)
		{
			ComboControl* pGamePadControl = AllocateControl<ComboControl>();
			ASSERT(pGamePadControl);

			pGamePadControl->mType = CONTROL_COMBO;
			pGamePadControl->pAction = pAction;
			pGamePadControl->mPressButton = gainput::PadButtonStart;
			pGamePadControl->mTriggerButton = gainput::PadButtonB;
			mControls[gamepadDeviceId][pGamePadControl->mTriggerButton].emplace_back(pGamePadControl);
			mControls[gamepadDeviceId][pGamePadControl->mPressButton].emplace_back(pGamePadControl);

			ComboControl* pControl = AllocateControl<ComboControl>();
			ASSERT(pControl);
			pControl->mType = CONTROL_BUTTON;
			pControl->pAction = pAction;
			decltype(mKeyMap)::const_iterator keyIt = mKeyMap.find(control);
			if (keyIt != mKeyMap.end())
				mControls[mKeyboardDeviceID][keyIt->second].emplace_back(pControl);
		}
		else if (InputBindings::BUTTON_BINDINGS_BEGIN <= control && InputBindings::BUTTON_BINDINGS_END >= control)
		{
			IControl* pControl = AllocateControl<IControl>();
			ASSERT(pControl);

			pControl->mType = CONTROL_BUTTON;
			pControl->pAction = pAction;

			decltype(mGamepadMap)::const_iterator gamepadIt = mGamepadMap.find(control);
			if (gamepadIt != mGamepadMap.end())
				mControls[gamepadDeviceId][gamepadIt->second].emplace_back(pControl);
#if TOUCH_INPUT
			if (InputBindings::BUTTON_SOUTH == control)
				mControls[mTouchDeviceID][TOUCH_DOWN(pDesc->mUserId)].emplace_back(pControl);
#else
			decltype(mKeyMap)::const_iterator keyIt = mKeyMap.find(control);
			if (keyIt != mKeyMap.end())
				mControls[mKeyboardDeviceID][keyIt->second].emplace_back(pControl);

			decltype(mMouseMap)::const_iterator mouseIt = mMouseMap.find(control);
			if (mouseIt != mMouseMap.end())
				mControls[mMouseDeviceID][mouseIt->second].emplace_back(pControl);
#endif
		}
		else if (InputBindings::FLOAT_BINDINGS_BEGIN <= control && InputBindings::FLOAT_BINDINGS_END >= control)
		{
			if (InputBindings::FLOAT_DPAD == control)
			{
				CompositeControl* pControl = AllocateControl<CompositeControl>();
				ASSERT(pControl);

				pControl->mType = CONTROL_COMPOSITE;
				pControl->pAction = pAction;
				pControl->mComposite = 4;
				pControl->mControls[0] = gainput::PadButtonRight;
				pControl->mControls[1] = gainput::PadButtonLeft;
				pControl->mControls[2] = gainput::PadButtonUp;
				pControl->mControls[3] = gainput::PadButtonDown;
				for (uint32_t i = 0; i < pControl->mComposite; ++i)
					mControls[gamepadDeviceId][pControl->mControls[i]].emplace_back(pControl);
			}
			else
			{
				uint32_t axisCount = 0;
				decltype(mGamepadAxisMap)::const_iterator gamepadIt = mGamepadAxisMap.find(control);
				ASSERT(gamepadIt != mGamepadAxisMap.end());
				if (gamepadIt != mGamepadAxisMap.end())
				{
					AxisControl* pControl = AllocateControl<AxisControl>();
					ASSERT(pControl);

					*pControl = gamepadIt->second;
					pControl->pAction = pAction;
					for (uint32_t i = 0; i < pControl->mAxisCount; ++i)
						mControls[gamepadDeviceId][pControl->mStartButton + i].emplace_back(pControl);

					axisCount = pControl->mAxisCount;
				}
#if TOUCH_INPUT
				if ((InputBindings::FLOAT_LEFTSTICK == control || InputBindings::FLOAT_RIGHTSTICK == control) && (pDesc->mOutsideRadius && pDesc->mScale))
				{
					VirtualJoystickControl* pControl = AllocateControl<VirtualJoystickControl>();
					ASSERT(pControl);

					pControl->mType = CONTROL_VIRTUAL_JOYSTICK;
					pControl->pAction = pAction;
					pControl->mOutsideRadius = pDesc->mOutsideRadius;
					pControl->mDeadzone = pDesc->mDeadzone;
					pControl->mScale = pDesc->mScale;
					pControl->mTouchIndex = 0xFF;
					pControl->mArea = InputBindings::FLOAT_LEFTSTICK == control ? AREA_LEFT : AREA_RIGHT;
					mControls[mTouchDeviceID][gainput::Touch0Down].emplace_back(pControl);
					mControls[mTouchDeviceID][gainput::Touch0X].emplace_back(pControl);
					mControls[mTouchDeviceID][gainput::Touch0Y].emplace_back(pControl);
					mControls[mTouchDeviceID][gainput::Touch1Down].emplace_back(pControl);
					mControls[mTouchDeviceID][gainput::Touch1X].emplace_back(pControl);
					mControls[mTouchDeviceID][gainput::Touch1Y].emplace_back(pControl);
				}
#else

#ifndef NO_DEFAULT_BINDINGS
				decltype(mGamepadCompositeMap)::const_iterator keyIt = mGamepadCompositeMap.find(control);
				if (keyIt != mGamepadCompositeMap.end())
				{
					CompositeControl* pControl = AllocateControl<CompositeControl>();
					ASSERT(pControl);

					*pControl = keyIt->second;
					pControl->pAction = pAction;
					for (uint32_t i = 0; i < pControl->mComposite; ++i)
						mControls[mKeyboardDeviceID][pControl->mControls[i]].emplace_back(pControl);
				}
#endif

				decltype(mGamepadFloatMap)::const_iterator floatIt = mGamepadFloatMap.find(control);
				if (floatIt != mGamepadFloatMap.end())
				{
					FloatControl* pControl = AllocateControl<FloatControl>();
					ASSERT(pControl);

					*pControl = floatIt->second;
					pControl->pAction = pAction;

					gainput::DeviceId deviceId = ((pControl->mDelta >> 1) & 0x1) ? mRawMouseDeviceID : mMouseDeviceID;

					for (uint32_t i = 0; i < axisCount; ++i)
						mControls[deviceId][pControl->mStartButton + i].emplace_back(pControl);
				}
#endif
			}
		}

		return pAction;
	}

	void RemoveInputAction(InputAction* pAction)
	{
		ASSERT(pAction);

		decltype(mGestureControls)::const_iterator it = eastl::find(mGestureControls.begin(), mGestureControls.end(), pAction);
		if (it != mGestureControls.end())
			mGestureControls.erase(it);

		tf_free(pAction);
	}

	bool InitSubView()
	{
#ifdef __APPLE__
		if (pWindow)
		{
			void* view = pWindow->handle.window;
			if (!view)
				return false;
			
#ifdef TARGET_IOS
			UIView*      mainView = (UIView*)CFBridgingRelease(view);
			GainputView* newView = [[GainputView alloc] initWithFrame:mainView.bounds inputManager : *pInputManager];
			//we want everything to resize with main view.
			[newView setAutoresizingMask : (UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight | UIViewAutoresizingFlexibleTopMargin |
				UIViewAutoresizingFlexibleLeftMargin | UIViewAutoresizingFlexibleRightMargin |
				UIViewAutoresizingFlexibleBottomMargin)];
#else
			NSView* mainView = (__bridge NSView*)view;
			float retinScale = ((CAMetalLayer*)(mainView.layer)).drawableSize.width / mainView.frame.size.width;
			GainputMacInputView* newView = [[GainputMacInputView alloc] initWithFrame:mainView.bounds
				window : mainView.window
				retinaScale : retinScale
				inputManager : *pInputManager];
			newView.nextKeyView = mainView;
			[newView setAutoresizingMask : NSViewWidthSizable | NSViewHeightSizable];
#endif

			[mainView addSubview : newView];
		
#ifdef TARGET_IOS
#else
			NSWindow* window = [newView window];
			BOOL madeFirstResponder = [window makeFirstResponder:newView];
			if (!madeFirstResponder)
				return false;
#endif
			
			pGainputView = (__bridge void*)newView;
		}
#endif

		return true;
	}

	void ShutdownSubView()
	{
#ifdef __APPLE__
		if (!pGainputView)
			return;

		//automatic reference counting
		//it will get deallocated.
		if (pGainputView)
		{
#ifndef TARGET_IOS
			GainputMacInputView* view = (GainputMacInputView*)CFBridgingRelease(pGainputView);
#else
			GainputView* view = (GainputView*)CFBridgingRelease(pGainputView);
#endif
			[view removeFromSuperview];
			pGainputView = NULL;
		}
#endif
	}

	bool SetEnableCaptureInput(bool enable)
	{
		ASSERT(pWindow);

#if defined(_WIN32) && !defined(XBOX)
		static int32_t lastCursorPosX = 0;
		static int32_t lastCursorPosY = 0;

		if (enable != mInputCaptured)
		{
			if (enable)
			{
				POINT lastCursorPoint;
				GetCursorPos(&lastCursorPoint);
				lastCursorPosX = lastCursorPoint.x;
				lastCursorPosY = lastCursorPoint.y;

				HWND handle = (HWND)pWindow->handle.window;
				SetCapture(handle);

				RECT clientRect;
				GetClientRect(handle, &clientRect);
				//convert screen rect to client coordinates.
				POINT ptClientUL = { clientRect.left, clientRect.top };
				// Add one to the right and bottom sides, because the
				// coordinates retrieved by GetClientRect do not
				// include the far left and lowermost pixels.
				POINT ptClientLR = { clientRect.right + 1, clientRect.bottom + 1 };
				ClientToScreen(handle, &ptClientUL);
				ClientToScreen(handle, &ptClientLR);

				// Copy the client coordinates of the client area
				// to the rcClient structure. Confine the mouse cursor
				// to the client area by passing the rcClient structure
				// to the ClipCursor function.
				SetRect(&clientRect, ptClientUL.x, ptClientUL.y, ptClientLR.x, ptClientLR.y);
				ClipCursor(&clientRect);
				ShowCursor(FALSE);
			}
			else
			{
				ClipCursor(NULL);
				ShowCursor(TRUE);
				ReleaseCapture();
				SetCursorPos(lastCursorPosX, lastCursorPosY);
			}

			mInputCaptured = enable;
			return true;
		}
#elif defined(__APPLE__)
		if (mInputCaptured != enable)
		{
#if !defined(TARGET_IOS)
			if (enable)
			{
				CGDisplayHideCursor(kCGDirectMainDisplay);
				CGAssociateMouseAndMouseCursorPosition(false);
			}
			else
			{
				CGDisplayShowCursor(kCGDirectMainDisplay);
				CGAssociateMouseAndMouseCursorPosition(true);
			}
#endif
			mInputCaptured = enable;

			return true;
		}
#elif defined(__linux__) && !defined(__ANDROID__) && !defined(GAINPUT_PLATFORM_GGP)
		if (mInputCaptured != enable)
		{
			if (enable)
			{
				// Create invisible cursor that will be used when mouse is captured
				Cursor      invisibleCursor = {};
				Pixmap      bitmapEmpty = {};
				XColor      emptyColor = {};
				static char emptyData[] = { 0, 0, 0, 0, 0, 0, 0, 0 };
				emptyColor.red = emptyColor.green = emptyColor.blue = 0;
				bitmapEmpty = XCreateBitmapFromData(pWindow->handle.display, pWindow->handle.window, emptyData, 8, 8);
				invisibleCursor = XCreatePixmapCursor(pWindow->handle.display, bitmapEmpty, bitmapEmpty, &emptyColor, &emptyColor, 0, 0);
				// Capture mouse
				unsigned int masks = PointerMotionMask |    //Mouse movement
					ButtonPressMask |      //Mouse click
					ButtonReleaseMask;     // Mouse release
				int XRes = XGrabPointer(
					pWindow->handle.display, pWindow->handle.window, 1 /*reports with respect to the grab window*/, masks, GrabModeAsync, GrabModeAsync, None,
					invisibleCursor, CurrentTime);
			}
			else
			{
				XUngrabPointer(pWindow->handle.display, CurrentTime);
			}

			mInputCaptured = enable;

			return true;
		}
#elif defined(__ANDROID__)
		if (mInputCaptured != enable)
		{
			mInputCaptured = enable;
			return true;
		}
#endif

		return false;
	}

	void SetVirtualKeyboard(uint32_t type)
	{
#ifdef TARGET_IOS
		if (!pGainputView)
			return;

		if ((type > 0) != mVirtualKeyboardActive)
			mVirtualKeyboardActive = (type > 0);
		else
			return;

		GainputView* view = (__bridge GainputView*)(pGainputView);
		[view setVirtualKeyboard : type];
#elif defined(__ANDROID__)		
		if ((type > 0) != mVirtualKeyboardActive)
		{
			mVirtualKeyboardActive = (type > 0);

			/* Note: native activity's API for soft input (ANativeActivity_showSoftInput & ANativeActivity_hideSoftInput) do not work.
			 *       So we do it manually using JNI.
		     */

			ANativeActivity* activity = pWindow->handle.activity;
			JNIEnv* jni;
			jint result = activity->vm->AttachCurrentThread(&jni, NULL);
			if (result == JNI_ERR)
			{
				ASSERT(0);
				return;
			}

			jclass cls = jni->GetObjectClass(activity->clazz);
			jmethodID methodID = jni->GetMethodID(cls, "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;");
			jstring serviceName = jni->NewStringUTF("input_method");
			jobject inputService = jni->CallObjectMethod(activity->clazz, methodID, serviceName);

			jclass inputServiceCls = jni->GetObjectClass(inputService);
			methodID = jni->GetMethodID(inputServiceCls, "toggleSoftInput", "(II)V");
			jni->CallVoidMethod(inputService, methodID, 0, 0);

			jni->DeleteLocalRef(serviceName);
			activity->vm->DetachCurrentThread();

		}
		else
			return;
#endif
	}

	inline constexpr bool IsPointerType(gainput::DeviceId device) const
	{
#if TOUCH_INPUT
		return false;
#else
		return (device == mMouseDeviceID || device == mRawMouseDeviceID);
#endif
	}

	bool OnDeviceButtonBool(gainput::DeviceId device, gainput::DeviceButtonId deviceButton, bool oldValue, bool newValue)
	{
		if (oldValue == newValue)
			return false;

		if (mControls[device].size())
		{
			InputActionContext ctx = {};
			ctx.mDeviceType = pDeviceTypes[device];
			ctx.pCaptured = IsPointerType(device) ? &mInputCaptured : &mDefaultCapture;
#if TOUCH_INPUT
			uint32_t touchIndex = 0;
			if (device == mTouchDeviceID)
			{
				touchIndex = TOUCH_USER(deviceButton);
				gainput::InputDeviceTouch* pTouch = (gainput::InputDeviceTouch*)pInputManager->GetDevice(mTouchDeviceID);
				mTouchPositions[touchIndex][0] = pTouch->GetFloat(TOUCH_X(touchIndex));
				mTouchPositions[touchIndex][1] = pTouch->GetFloat(TOUCH_Y(touchIndex));
				ctx.pPosition = &mTouchPositions[touchIndex];
			}
#else
			if (IsPointerType(device))
			{
				gainput::InputDeviceMouse* pMouse = (gainput::InputDeviceMouse*)pInputManager->GetDevice(mMouseDeviceID);
				mMousePosition[0] = pMouse->GetFloat(gainput::MouseAxisX);
				mMousePosition[1] = pMouse->GetFloat(gainput::MouseAxisY);
				ctx.pPosition = &mMousePosition;
				ctx.mScrollValue = pMouse->GetFloat(gainput::MouseButtonMiddle);
			}
#endif
			bool executeNext = true;

			const eastl::vector<IControl*>& controls = mControls[device][deviceButton];
			for (IControl* control : controls)
			{
				if (!executeNext)
					return true;

				const InputControlType type = control->mType;
				const InputActionDesc* pDesc = &control->pAction->mDesc;
				ctx.pUserData = pDesc->pUserData;
				ctx.mBinding = mControlMapReverse[device][deviceButton];

				switch (type)
				{
				case CONTROL_BUTTON:
				{
					if (pDesc->pFunction)
					{
						ctx.mBool = newValue;
						if (newValue && !oldValue)
						{
							ctx.mPhase = INPUT_ACTION_PHASE_STARTED;
							executeNext = pDesc->pFunction(&ctx) && executeNext;
#if TOUCH_INPUT
							mButtonControlPerformQueue.insert(control);
#else
							ctx.mPhase = INPUT_ACTION_PHASE_PERFORMED;
							executeNext = pDesc->pFunction(&ctx) && executeNext;
#endif
						}
						else if (oldValue && !newValue)
						{
							ctx.mPhase = INPUT_ACTION_PHASE_CANCELED;
							executeNext = pDesc->pFunction(&ctx) && executeNext;
						}
					}
					break;
				}
				case CONTROL_COMPOSITE:
				{
					CompositeControl* pControl = (CompositeControl*)control;
					uint32_t index = 0;
					for (; index < pControl->mComposite; ++index)
						if (deviceButton == pControl->mControls[index])
							break;

					const uint32_t axis = (index > 1);
					if (newValue)
					{
						pControl->mPressedVal[index] = 1;
						pControl->mValue[axis] = (float)pControl->mPressedVal[axis * 2 + 0] - (float)pControl->mPressedVal[axis * 2 + 1];
					}

					if (pControl->mComposite == 2)
					{
						ctx.mFloat = pControl->mValue[axis];
					}
					else
					{
						if (!pControl->mValue[0] && !pControl->mValue[1])
							ctx.mFloat2 = float2(0.0f);
						else
							ctx.mFloat2 = pControl->mValue;
					}

					// Action Started
					if (!pControl->mStarted && !oldValue && newValue)
					{
						pControl->mStarted = 1;
						ctx.mPhase = INPUT_ACTION_PHASE_STARTED;
						if (pDesc->pFunction)
							executeNext = pDesc->pFunction(&ctx) && executeNext;
					}
					// Action Performed
					if (pControl->mStarted && newValue && !pControl->mPerformed[index])
					{
						pControl->mPerformed[index] = 1;
						ctx.mPhase = INPUT_ACTION_PHASE_PERFORMED;
						if (pDesc->pFunction)
							executeNext = pDesc->pFunction(&ctx) && executeNext;
					}
					// Action Canceled
					if (oldValue && !newValue)
					{
						pControl->mPerformed[index] = 0;
						pControl->mPressedVal[index] = 0;
						bool allReleased = true;
						for (uint8_t i = 0; i < pControl->mComposite; ++i)
						{
							if (pControl->mPerformed[i])
							{
								allReleased = false;
								break;
							}
						}
						if (allReleased)
						{
							pControl->mValue = float2(0.0f);
							pControl->mStarted = 0;
							ctx.mFloat2 = pControl->mValue;
							ctx.mPhase = INPUT_ACTION_PHASE_CANCELED;
							if (pDesc->pFunction)
								executeNext = pDesc->pFunction(&ctx) && executeNext;
						}
						else if (pDesc->pFunction)
						{
							ctx.mPhase = INPUT_ACTION_PHASE_PERFORMED;
							pControl->mValue[axis] = (float)pControl->mPressedVal[axis * 2 + 0] - (float)pControl->mPressedVal[axis * 2 + 1];
							ctx.mFloat2 = pControl->mValue;
							executeNext = pDesc->pFunction(&ctx) && executeNext;
						}
					}

					break;
				}
				// Mouse scroll is using OnDeviceButtonBool
				case CONTROL_FLOAT:
				{
					if (!oldValue && newValue)
					{
						ASSERT(deviceButton == gainput::MouseButtonWheelUp || deviceButton == gainput::MouseButtonWheelDown);

						FloatControl* pControl = (FloatControl*)control;
						ctx.mFloat2[1] = deviceButton == gainput::MouseButtonWheelUp ? 1.0f : -1.0f;
						if (pDesc->pFunction)
						{
							ctx.mPhase = INPUT_ACTION_PHASE_PERFORMED;
							executeNext = pDesc->pFunction(&ctx) && executeNext;
						}

						mFloatDeltaControlCancelQueue.insert(pControl);
					}
				}
#if TOUCH_INPUT
				case CONTROL_VIRTUAL_JOYSTICK:
				{
					VirtualJoystickControl* pControl = (VirtualJoystickControl*)control;

					if (!oldValue && newValue && !pControl->mStarted)
					{
						pControl->mStartPos = mTouchPositions[touchIndex];
						if ((AREA_LEFT == pControl->mArea && pControl->mStartPos[0] <= pInputManager->GetDisplayWidth() * 0.5f) ||
							(AREA_RIGHT == pControl->mArea && pControl->mStartPos[0] > pInputManager->GetDisplayWidth() * 0.5f))
						{
							pControl->mStarted = 0x3;
							pControl->mTouchIndex = touchIndex;
							pControl->mCurrPos = pControl->mStartPos;

							if (pDesc->pFunction)
							{
								ctx.mPhase = INPUT_ACTION_PHASE_STARTED;
								ctx.mFloat2 = float2(0.0f);
								ctx.pPosition = &pControl->mCurrPos;
								executeNext = pDesc->pFunction(&ctx) && executeNext;
							}
						}
						else
						{
							pControl->mStarted = 0;
							pControl->mTouchIndex = 0xFF;
						}
					}
					else if (oldValue && !newValue)
					{
						if (pControl->mTouchIndex == touchIndex)
						{
							pControl->mIsPressed = 0;
							pControl->mTouchIndex = 0xFF;
							pControl->mStarted = 0;
							pControl->mPerformed = 0;
							if (pDesc->pFunction)
							{
								ctx.mFloat2 = float2(0.0f);
								ctx.mPhase = INPUT_ACTION_PHASE_CANCELED;
								executeNext = pDesc->pFunction(&ctx) && executeNext;
							}
						}
					}
					break;
				}
#endif
				case CONTROL_COMBO:
				{
					ComboControl* pControl = (ComboControl*)control;
					if (deviceButton == pControl->mPressButton)
					{
						pControl->mPressed = (uint8_t)newValue;
					}
					else if (pControl->mPressed && oldValue && !newValue && pDesc->pFunction)
					{
						ctx.mBool = true;
						ctx.mPhase = INPUT_ACTION_PHASE_PERFORMED;
						pDesc->pFunction(&ctx);
					}
					break;
				}
				default:
					break;
				}
			}
		}

		return true;
	}

	bool OnDeviceButtonFloat(gainput::DeviceId device, gainput::DeviceButtonId deviceButton, float oldValue, float newValue)
	{
#if TOUCH_INPUT
		if (mTouchDeviceID == device)
		{
			const uint32_t touchIndex = TOUCH_USER(deviceButton);
			gainput::InputDeviceTouch* pTouch = (gainput::InputDeviceTouch*)pInputManager->GetDevice(mTouchDeviceID);
			mTouchPositions[touchIndex][0] = pTouch->GetFloat(TOUCH_X(touchIndex));
			mTouchPositions[touchIndex][1] = pTouch->GetFloat(TOUCH_Y(touchIndex));
		}
#else
		if (mMouseDeviceID == device)
		{
			gainput::InputDeviceMouse* pMouse = (gainput::InputDeviceMouse*)pInputManager->GetDevice(mMouseDeviceID);
			mMousePosition[0] = pMouse->GetFloat(gainput::MouseAxisX);
			mMousePosition[1] = pMouse->GetFloat(gainput::MouseAxisY);
		}
#endif

		if (mControls[device].size())
		{
			bool executeNext = true;

			const eastl::vector<IControl*>& controls = mControls[device][deviceButton];
			for (IControl* control : controls)
			{
				if (!executeNext)
					return true;

				const InputControlType type = control->mType;
				const InputActionDesc* pDesc = &control->pAction->mDesc;
				InputActionContext ctx = {};
				ctx.mDeviceType = pDeviceTypes[device];
				ctx.pUserData = pDesc->pUserData;
				ctx.pCaptured = IsPointerType(device) ? &mInputCaptured : &mDefaultCapture;

				switch (type)
				{
				case CONTROL_FLOAT:
				{
					FloatControl* pControl = (FloatControl*)control;
					const InputActionDesc* pDesc = &pControl->pAction->mDesc;
					const uint32_t axis = (deviceButton - pControl->mStartButton);

					if (pControl->mDelta & 0x1)
					{
						pControl->mValue[axis] += (axis > 0 ? -1.0f : 1.0f) * (newValue - oldValue);
						ctx.mFloat3 = pControl->mValue;

						if (((pControl->mStarted >> axis) & 0x1) == 0)
						{
							pControl->mStarted |= (1 << axis);
							if (pControl->mStarted == pControl->mTarget && pDesc->pFunction)
							{
								ctx.mPhase = INPUT_ACTION_PHASE_STARTED;
								executeNext = pDesc->pFunction(&ctx) && executeNext;
							}

							mFloatDeltaControlCancelQueue.insert(pControl);
						}

						pControl->mPerformed |= (1 << axis);

						if (pControl->mPerformed == pControl->mTarget)
						{
							pControl->mPerformed = 0;
							ctx.mPhase = INPUT_ACTION_PHASE_PERFORMED;
							if (pDesc->pFunction)
								executeNext = pDesc->pFunction(&ctx) && executeNext;
						}
					}
					else if (pDesc->pFunction)
					{
						pControl->mPerformed |= (1 << axis);
						pControl->mValue[axis] = newValue;
						if (pControl->mPerformed == pControl->mTarget)
						{
							ctx.mPhase = INPUT_ACTION_PHASE_PERFORMED;
							pControl->mPerformed = 0;
							ctx.mFloat3 = pControl->mValue;
							executeNext = pDesc->pFunction(&ctx) && executeNext;
						}
					}
					break;
				}
				case CONTROL_AXIS:
				{
					AxisControl* pControl = (AxisControl*)control;
					const InputActionDesc* pDesc = &pControl->pAction->mDesc;

					const uint32_t axis = (deviceButton - pControl->mStartButton);

					pControl->mNewValue[axis] = newValue;
					bool equal = true;
					for (uint32_t i = 0; i < pControl->mAxisCount; ++i)
						equal = equal && (pControl->mValue[i] == pControl->mNewValue[i]);

					pControl->mPerformed |= (1 << axis);
					pControl->mValue[axis] = pControl->mNewValue[axis];

					if (pControl->mPerformed == pControl->mTarget && pDesc->pFunction)
					{
						ctx.mPhase = INPUT_ACTION_PHASE_PERFORMED;
						ctx.mFloat3 = pControl->mValue;
						executeNext = pDesc->pFunction(&ctx) && executeNext;
					}

					if (pControl->mPerformed != pControl->mTarget)
						continue;

					pControl->mPerformed = 0;
					break;
				}
#if TOUCH_INPUT
				case CONTROL_VIRTUAL_JOYSTICK:
				{
					VirtualJoystickControl* pControl = (VirtualJoystickControl*)control;

					const uint32_t axis = TOUCH_AXIS(deviceButton);

					if (!pControl->mStarted || TOUCH_USER(deviceButton) != pControl->mTouchIndex)
						continue;

					pControl->mPerformed |= (1 << axis);
					pControl->mCurrPos[axis] = newValue;
					if (pControl->mPerformed == 0x3)
					{
						// Calculate the new joystick positions
						vec2 delta = f2Tov2(pControl->mCurrPos - pControl->mStartPos);
						float halfRad = (pControl->mOutsideRadius * 0.5f) - pControl->mDeadzone;
						if (length(delta) > halfRad)
							pControl->mCurrPos = pControl->mStartPos + halfRad * v2ToF2(normalize(delta));

						ctx.mPhase = INPUT_ACTION_PHASE_PERFORMED;
						float2 dir = ((pControl->mCurrPos - pControl->mStartPos) / halfRad) * pControl->mScale;
						ctx.mFloat2 = float2(dir[0], -dir[1]);
						ctx.pPosition = &pControl->mCurrPos;
						if (pDesc->pFunction)
							executeNext = pDesc->pFunction(&ctx) && executeNext;
					}
					break;
				}
#endif
				default:
					break;
				}
			}
		}

		return true;
	}

	bool OnDeviceButtonGesture(gainput::DeviceId device, gainput::DeviceButtonId deviceButton, const struct gainput::GestureChange& gesture)
	{
#if defined(TARGET_IOS)
		const InputActionDesc* pDesc = &mGestureControls[deviceButton]->mDesc;
		if (pDesc->pFunction)
		{
			InputActionContext ctx = {};
			ctx.pUserData = pDesc->pUserData;
			ctx.mDeviceType = pDeviceTypes[device];
			ctx.pPosition = (float2*)gesture.position;
			if (gesture.type == gainput::GesturePan)
			{
				ctx.mFloat2 = { gesture.translation[0], gesture.translation[1] };
			}
			else if (gesture.type == gainput::GesturePinch)
			{
				ctx.mFloat4 =
				{
					gesture.velocity,
					gesture.scale,
					gesture.distance[0],
					gesture.distance[1]
				};
			}

			ctx.mPhase = INPUT_ACTION_PHASE_PERFORMED;
			pDesc->pFunction(&ctx);
		}
#endif

		return true;
	}

	int GetPriority() const
	{
		return 0;
	}

	void SetDeadZone(unsigned int controllerIndex, float deadZoneSize)
	{
		if (controllerIndex >= MAX_INPUT_GAMEPADS)
			return;
		gainput::InputDevicePad* pDevicePad = (gainput::InputDevicePad*)pInputManager->GetDevice(pGamepadDeviceIDs[controllerIndex]);
		pDevicePad->SetDeadZone(gainput::PadButton::PadButtonL3, deadZoneSize);
		pDevicePad->SetDeadZone(gainput::PadButton::PadButtonR3, deadZoneSize);
		pDevicePad->SetDeadZone(gainput::PadButton::PadButtonL2, deadZoneSize);
		pDevicePad->SetDeadZone(gainput::PadButton::PadButtonR2, deadZoneSize);
		pDevicePad->SetDeadZone(gainput::PadButton::PadButtonLeftStickX, deadZoneSize);
		pDevicePad->SetDeadZone(gainput::PadButton::PadButtonLeftStickY, deadZoneSize);
		pDevicePad->SetDeadZone(gainput::PadButton::PadButtonRightStickX, deadZoneSize);
		pDevicePad->SetDeadZone(gainput::PadButton::PadButtonRightStickY, deadZoneSize);
		pDevicePad->SetDeadZone(gainput::PadButton::PadButtonAxis4, deadZoneSize);
		pDevicePad->SetDeadZone(gainput::PadButton::PadButtonAxis5, deadZoneSize);
	}

	const char* GetGamePadName(unsigned  gamePadIndex)
	{
		if (gamePadIndex >= MAX_INPUT_GAMEPADS)
			return "Incorrect gamePadIndex";
		gainput::InputDevicePad* pDevicePad = (gainput::InputDevicePad*)pInputManager->GetDevice(pGamepadDeviceIDs[gamePadIndex]);
		return pDevicePad->GetDeviceName();
	}

	bool GamePadConnected(unsigned int gamePadIndex)
	{
		if (gamePadIndex >= MAX_INPUT_GAMEPADS)
			return false;
		gainput::InputDevicePad* pDevicePad = (gainput::InputDevicePad*)pInputManager->GetDevice(pGamepadDeviceIDs[gamePadIndex]);
		return pDevicePad->IsAvailable();
	}

	bool SetRumbleEffect(unsigned gamePadIndex, float left_motor, float right_motor, uint32_t duration_ms)
	{
		if (gamePadIndex >= MAX_INPUT_GAMEPADS)
			return false;
		gainput::InputDevicePad* pDevicePad = (gainput::InputDevicePad*)pInputManager->GetDevice(pGamepadDeviceIDs[gamePadIndex]);
		return pDevicePad->SetRumbleEffect(left_motor, right_motor, duration_ms);
	}

	void SetLEDColor(unsigned gamePadIndex, uint8_t r, uint8_t g, uint8_t b)
	{
		if (gamePadIndex >= MAX_INPUT_GAMEPADS)
			return;
		gainput::InputDevicePad* pDevicePad = (gainput::InputDevicePad*)pInputManager->GetDevice(pGamepadDeviceIDs[gamePadIndex]);
		pDevicePad->SetLEDColor(r, g, b);
	}
};

static InputSystemImpl* pInputSystem = NULL;

#if defined(_WIN32) && !defined(XBOX)
static void ResetInputStates()
{
	pInputSystem->pInputManager->ClearAllStates(pInputSystem->mMouseDeviceID);
	pInputSystem->pInputManager->ClearAllStates(pInputSystem->mKeyboardDeviceID);
	for (uint32_t i = 0; i < MAX_INPUT_GAMEPADS; ++i)
	{
		pInputSystem->pInputManager->ClearAllStates(pInputSystem->pGamepadDeviceIDs[i]);
	}
}
#endif

int32_t InputSystemHandleMessage(WindowsDesc* pWindow, void* msg)
{
	if (pInputSystem == nullptr)
	{
		return 0;
	}
#if defined(_WIN32) && !defined(XBOX)
	pInputSystem->pInputManager->HandleMessage(*(MSG*)msg);
	if ((*(MSG*)msg).message == WM_ACTIVATEAPP && (*(MSG*)msg).wParam == WA_INACTIVE)
	{
		ResetInputStates();
	}
#elif defined(__ANDROID__)
	return pInputSystem->pInputManager->HandleInput((AInputEvent*)msg, pWindow->handle.activity);
#elif defined(__linux__) && !defined(GAINPUT_PLATFORM_GGP)
	pInputSystem->pInputManager->HandleEvent(*(XEvent*)msg);
#endif

	return 0;
}

bool initInputSystem(WindowsDesc* window)
{
	pInputSystem = tf_new(InputSystemImpl);

	setCustomMessageProcessor(InputSystemHandleMessage);

	return pInputSystem->Init(window);
}

void exitInputSystem()
{
	ASSERT(pInputSystem);

	setCustomMessageProcessor(nullptr);

	pInputSystem->Exit();
	tf_delete(pInputSystem);
	pInputSystem = NULL;
}

void updateInputSystem(uint32_t width, uint32_t height)
{
	ASSERT(pInputSystem);

	pInputSystem->Update(width, height);
}

InputAction* addInputAction(const InputActionDesc* pDesc)
{
	ASSERT(pInputSystem);

	return pInputSystem->AddInputAction(pDesc);
}

void removeInputAction(InputAction* pAction)
{
	ASSERT(pInputSystem);

	pInputSystem->RemoveInputAction(pAction);
}

bool setEnableCaptureInput(bool enable)
{
	ASSERT(pInputSystem);

	return pInputSystem->SetEnableCaptureInput(enable);
}

void setVirtualKeyboard(uint32_t type)
{
	ASSERT(pInputSystem);

	pInputSystem->SetVirtualKeyboard(type);
}

void setDeadZone(unsigned int controllerIndex, float deadZoneSize)
{
	ASSERT(pInputSystem);

	pInputSystem->SetDeadZone(controllerIndex, deadZoneSize);
}

const char* getGamePadName(int gamePadIndex)
{
	ASSERT(pInputSystem);

	return pInputSystem->GetGamePadName(gamePadIndex);
}

bool gamePadConnected(int gamePadIndex)
{
	ASSERT(pInputSystem);

	return pInputSystem->GamePadConnected(gamePadIndex);
}

bool setRumbleEffect(int gamePadIndex, float left_motor, float right_motor, uint32_t duration_ms)
{
	ASSERT(pInputSystem);

	return pInputSystem->SetRumbleEffect(gamePadIndex, left_motor, right_motor, duration_ms);
}

void setLEDColor(int gamePadIndex, uint8_t r, uint8_t g, uint8_t b)
{
	ASSERT(pInputSystem);

	pInputSystem->SetLEDColor(gamePadIndex, r, g, b);
}
#endif
