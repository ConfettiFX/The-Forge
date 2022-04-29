/*
 * Copyright (c) 2017-2022 The Forge Interactive Inc.
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

#pragma once

#include "../Core/Config.h"
#include "IOperatingSystem.h"
#include "../Math/MathTypes.h"

const uint32_t MAX_INPUT_GAMEPADS = 4;
const uint32_t MAX_INPUT_MULTI_TOUCHES = 4;
const uint32_t MAX_INPUT_ACTIONS = 128;

typedef struct InputBindings
{
	typedef enum Binding
	{
		/**********************************************/
		// Gamepad Bindings
		/**********************************************/
		FLOAT_BINDINGS_BEGIN,
		FLOAT_LEFTSTICK = FLOAT_BINDINGS_BEGIN,
		FLOAT_RIGHTSTICK,
		FLOAT_L2,
		FLOAT_R2,
		FLOAT_GRAVITY,
		FLOAT_GYROSCOPE,
		FLOAT_MAGNETICFIELD,
		FLOAT_DPAD,
		FLOAT_BINDINGS_END = FLOAT_DPAD,

		BUTTON_BINDINGS_BEGIN,
		BUTTON_DPAD_LEFT = BUTTON_BINDINGS_BEGIN,
		BUTTON_DPAD_RIGHT,
		BUTTON_DPAD_UP,
		BUTTON_DPAD_DOWN,
		BUTTON_SOUTH,    // A/CROSS
		BUTTON_EAST,     // B/CIRCLE
		BUTTON_WEST,     // X/SQUARE
		BUTTON_NORTH,    // Y/TRIANGLE
		BUTTON_L1,
		BUTTON_R1,
		BUTTON_L2,
		BUTTON_R2,
		BUTTON_L3,      // LEFT THUMB
		BUTTON_R3,      // RIGHT THUMB
		BUTTON_HOME,    // PS BUTTON
		BUTTON_START,
		BUTTON_SELECT,
		BUTTON_TOUCH,    //PS4 TOUCH
		BUTTON_BACK,
		BUTTON_FULLSCREEN,
		BUTTON_EXIT,
		BUTTON_DUMP,
		/**********************************************/
		// KeyBoard bindings
		/**********************************************/
		BUTTON_KEYESCAPE,
		BUTTON_KEYF1,
		BUTTON_KEYF2,
		BUTTON_KEYF3,
		BUTTON_KEYF4,
		BUTTON_KEYF5,
		BUTTON_KEYF6,
		BUTTON_KEYF7,
		BUTTON_KEYF8,
		BUTTON_KEYF9,
		BUTTON_KEYF10,
		BUTTON_KEYF11,
		BUTTON_KEYF12,
		BUTTON_KEYF13,
		BUTTON_KEYF14,
		BUTTON_KEYF15,
		BUTTON_KEYF16,
		BUTTON_KEYF17,
		BUTTON_KEYF18,
		BUTTON_KEYF19,
		BUTTON_KEYPRINT,
		BUTTON_KEYSCROLLLOCK,
		BUTTON_KEYBREAK,
		BUTTON_KEYSPACE,
		BUTTON_KEYAPOSTROPHE,
		BUTTON_KEYCOMMA,
		BUTTON_KEYMINUS,
		BUTTON_KEYPERIOD,
		BUTTON_KEYSLASH,
		BUTTON_KEY0,
		BUTTON_KEY1,
		BUTTON_KEY2,
		BUTTON_KEY3,
		BUTTON_KEY4,
		BUTTON_KEY5,
		BUTTON_KEY6,
		BUTTON_KEY7,
		BUTTON_KEY8,
		BUTTON_KEY9,
		BUTTON_KEYSEMICOLON,
		BUTTON_KEYLESS,
		BUTTON_KEYEQUAL,
		BUTTON_KEYA,
		BUTTON_KEYB,
		BUTTON_KEYC,
		BUTTON_KEYD,
		BUTTON_KEYE,
		BUTTON_KEYF,
		BUTTON_KEYG,
		BUTTON_KEYH,
		BUTTON_KEYI,
		BUTTON_KEYJ,
		BUTTON_KEYK,
		BUTTON_KEYL,
		BUTTON_KEYM,
		BUTTON_KEYN,
		BUTTON_KEYO,
		BUTTON_KEYP,
		BUTTON_KEYQ,
		BUTTON_KEYR,
		BUTTON_KEYS,
		BUTTON_KEYT,
		BUTTON_KEYU,
		BUTTON_KEYV,
		BUTTON_KEYW,
		BUTTON_KEYX,
		BUTTON_KEYY,
		BUTTON_KEYZ,
		BUTTON_KEYBRACKETLEFT,
		BUTTON_KEYBACKSLASH,
		BUTTON_KEYBRACKETRIGHT,
		BUTTON_KEYGRAVE,
		BUTTON_KEYLEFT,
		BUTTON_KEYRIGHT,
		BUTTON_KEYUP,
		BUTTON_KEYDOWN,
		BUTTON_KEYINSERT,
		BUTTON_KEYHOME,
		BUTTON_KEYDELETE,
		BUTTON_KEYEND,
		BUTTON_KEYPAGEUP,
		BUTTON_KEYPAGEDOWN,
		BUTTON_KEYNUMLOCK,
		BUTTON_KEYKPEQUAL,
		BUTTON_KEYKPDIVIDE,
		BUTTON_KEYKPMULTIPLY,
		BUTTON_KEYKPSUBTRACT,
		BUTTON_KEYKPADD,
		BUTTON_KEYKPENTER,
		BUTTON_KEYKPINSERT,      // 0
		BUTTON_KEYKPEND,         // 1
		BUTTON_KEYKPDOWN,        // 2
		BUTTON_KEYKPPAGEDOWN,    // 3
		BUTTON_KEYKPLEFT,        // 4
		BUTTON_KEYKPBEGIN,       // 5
		BUTTON_KEYKPRIGHT,       // 6
		BUTTON_KEYKPHOME,        // 7
		BUTTON_KEYKPUP,          // 8
		BUTTON_KEYKPPAGEUP,      // 9
		BUTTON_KEYKPDELETE,      // ,
		BUTTON_KEYBACKSPACE,
		BUTTON_KEYTAB,
		BUTTON_KEYRETURN,
		BUTTON_KEYCAPSLOCK,
		BUTTON_KEYSHIFTL,
		BUTTON_KEYCTRLL,
		BUTTON_KEYSUPERL,
		BUTTON_KEYALTL,
		BUTTON_KEYALTR,
		BUTTON_KEYSUPERR,
		BUTTON_KEYMENU,
		BUTTON_KEYCTRLR,
		BUTTON_KEYSHIFTR,
		BUTTON_KEYBACK,
		BUTTON_KEYSOFTLEFT,
		BUTTON_KEYSOFTRIGHT,
		BUTTON_KEYCALL,
		BUTTON_KEYENDCALL,
		BUTTON_KEYSTAR,
		BUTTON_KEYPOUND,
		BUTTON_KEYDPADCENTER,
		BUTTON_KEYVOLUMEUP,
		BUTTON_KEYVOLUMEDOWN,
		BUTTON_KEYPOWER,
		BUTTON_KEYCAMERA,
		BUTTON_KEYCLEAR,
		BUTTON_KEYSYMBOL,
		BUTTON_KEYEXPLORER,
		BUTTON_KEYENVELOPE,
		BUTTON_KEYEQUALS,
		BUTTON_KEYAT,
		BUTTON_KEYHEADSETHOOK,
		BUTTON_KEYFOCUS,
		BUTTON_KEYPLUS,
		BUTTON_KEYNOTIFICATION,
		BUTTON_KEYSEARCH,
		BUTTON_KEYMEDIAPLAYPAUSE,
		BUTTON_KEYMEDIASTOP,
		BUTTON_KEYMEDIANEXT,
		BUTTON_KEYMEDIAPREVIOUS,
		BUTTON_KEYMEDIAREWIND,
		BUTTON_KEYMEDIAFASTFORWARD,
		BUTTON_KEYMUTE,
		BUTTON_KEYPICTSYMBOLS,
		BUTTON_KEYSWITCHCHARSET,
		BUTTON_KEYFORWARD,
		BUTTON_KEYEXTRA1,
		BUTTON_KEYEXTRA2,
		BUTTON_KEYEXTRA3,
		BUTTON_KEYEXTRA4,
		BUTTON_KEYEXTRA5,
		BUTTON_KEYEXTRA6,
		BUTTON_KEYFN,
		BUTTON_KEYCIRCUMFLEX,
		BUTTON_KEYSSHARP,
		BUTTON_KEYACUTE,
		BUTTON_KEYALTGR,
		BUTTON_KEYNUMBERSIGN,
		BUTTON_KEYUDIAERESIS,
		BUTTON_KEYADIAERESIS,
		BUTTON_KEYODIAERESIS,
		BUTTON_KEYSECTION,
		BUTTON_KEYARING,
		BUTTON_KEYDIAERESIS,
		BUTTON_KEYTWOSUPERIOR,
		BUTTON_KEYRIGHTPARENTHESIS,
		BUTTON_KEYDOLLAR,
		BUTTON_KEYUGRAVE,
		BUTTON_KEYASTERISK,
		BUTTON_KEYCOLON,
		BUTTON_KEYEXCLAM,
		BUTTON_KEYBRACELEFT,
		BUTTON_KEYBRACERIGHT,
		BUTTON_KEYSYSRQ,
		/**********************************************/
		// Mouse bindings
		/**********************************************/
		BUTTON_MOUSE_LEFT,
		BUTTON_MOUSE_RIGHT,
		BUTTON_MOUSE_MIDDLE,
		BUTTON_MOUSE_SCROLL_UP,
		BUTTON_MOUSE_SCROLL_DOWN,
		BUTTON_MOUSE_5,
		BUTTON_MOUSE_6,
		BUTTON_MOUSE_7,
		/**********************************************/
		/**********************************************/
		BUTTON_ANY,
		BUTTON_BINDINGS_END = BUTTON_ANY,
		/**********************************************/
		// Gesture bindings
		/**********************************************/
		GESTURE_BINDINGS_BEGIN,
		GESTURE_TAP = GESTURE_BINDINGS_BEGIN,
		GESTURE_PAN,
		GESTURE_LONG_PRESS,
		GESTURE_BINDINGS_END = GESTURE_LONG_PRESS,
		/**********************************************/
		/**********************************************/
		TEXT,
	} Binding;

	typedef struct GestureDesc
	{
		/// Configuring Pan gesture
		uint32_t mMinNumberOfTouches;
		uint32_t mMaxNumberOfTouches;
		/// Configuring Tap gesture (single tap, double tap, ...)
		uint32_t mNumberOfTapsRequired;
		/// Configuring Long press gesture
		float mMinimumPressDuration;
	} GestureDesc;
} InputBindings;

typedef struct InputAction InputAction;

typedef enum InputDeviceType
{
	INPUT_DEVICE_INVALID = 0,
	INPUT_DEVICE_GAMEPAD,
	INPUT_DEVICE_TOUCH,
	INPUT_DEVICE_KEYBOARD,
	INPUT_DEVICE_MOUSE,
} InputDeviceType;

typedef enum InputActionPhase
{
	/// Action is initiated
	INPUT_ACTION_PHASE_STARTED = 0,
	/// Example: mouse delta changed, key pressed, ...
	INPUT_ACTION_PHASE_UPDATED,
	/// Example: mouse delta changed, key pressed, ...
	INPUT_ACTION_PHASE_ENDED,
	/// Example: left mouse button was pressed and now released, gesture was started but got canceled
	INPUT_ACTION_PHASE_CANCELED,
} InputActionPhase;

typedef struct InputActionContext
{
	void*		pUserData;
	/// Indicer of fingers for detected gesture
	int32_t		mFingerIndices[MAX_INPUT_MULTI_TOUCHES];
	union
	{
		/// Gesture input
		float4 mFloat4;
		/// 3D input (gyroscope, ...)
		float3 mFloat3;
		/// 2D input (mouse position, delta, composite input (wasd), gamepad stick, joystick, ...)
		float2 mFloat2;
		/// 1D input (composite input (ws), gamepad left trigger, ...)
		float mFloat;
		/// Button input (mouse left button, keyboard keys, ...)
		bool mBool;
		/// Text input
		wchar_t* pText;
	};

	float2*     pPosition;
	const bool* pCaptured;
	float       mScrollValue;
	uint32_t    mBinding;
	/// What phase is the action currently in
	uint8_t mPhase;
	uint8_t mDeviceType;
} InputActionContext;

typedef bool (*InputActionCallback)(InputActionContext* pContext);

typedef struct InputActionDesc
{
	/// Value from InputBindings::Binding enum
	uint32_t mBinding;
	/// Callback when an action is initiated, performed or canceled
	InputActionCallback pFunction;
	/// User data which will be assigned to InputActionContext::pUserData when calling pFunction
	void* pUserData;
	/// Virtual joystick
	float mDeadzone;
	float mOutsideRadius;
	float mScale;
	/// User management (which user does this action apply to)
	uint8_t mUserId;
	/// Gesture desc
	InputBindings::GestureDesc* pGesture;
} InputActionDesc;

typedef struct InputSystemDesc
{

	void*          pRenderer = NULL; // Renderer*
	WindowDesc* pWindow = NULL;
	
	bool           mDisableVirtualJoystick = false; 

} InputSystemDesc;

bool         initInputSystem(InputSystemDesc* pDesc);
void         exitInputSystem();
void         updateInputSystem(uint32_t width, uint32_t height);

InputAction* addInputAction(const InputActionDesc* pDesc);
void         removeInputAction(InputAction* pAction);
bool         setEnableCaptureInput(bool enable);

/// Used to enable/disable text input for non-keyboard setups (virtual keyboards for console/mobile, ...)
void setVirtualKeyboard(uint32_t type);

void setDeadZone(unsigned int controllerIndex, float deadZoneSize);
void setLEDColor(int gamePadIndex, uint8_t r, uint8_t g, uint8_t b);
bool setRumbleEffect(int gamePadIndex, float left_motor, float right_motor, uint32_t duration_ms);

const char* getGamePadName(int gamePadIndex);
bool 		gamePadConnected(int gamePadIndex);
void 		setOnDeviceChangeCallBack(void (*onDeviceChnageCallBack)(const char* name, bool added), unsigned int gamePadIndex);
