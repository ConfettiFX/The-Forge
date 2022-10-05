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

#include "../Config.h"
#include "../../OS/Interfaces/IOperatingSystem.h"
#include "../../Utilities/Math/MathTypes.h"

typedef struct Renderer Renderer;

const uint32_t MAX_INPUT_GAMEPADS = 4;
const uint32_t MAX_INPUT_MULTI_TOUCHES = 4;
const uint32_t MAX_INPUT_ACTIONS = 256;
const int32_t  BUILTIN_DEVICE_HAPTICS = -1000;

/* This file declares all possible hardware buttons.
 * These are used to associate action mappings to actual device buttons which can then be provided to the input system.
 *
 * NOTE: these enums should exactly match the gainput's enum variants.  They are specifically redefined in this file as to not
 *       expose the gainput library directly to the application.
 */

 // Controllers/Gamepads (gainput::PadButton)
typedef enum GamepadButton
{
	GAMEPAD_BUTTON_LEFT_STICK_X,
	GAMEPAD_BUTTON_LEFT_STICK_Y,
	GAMEPAD_BUTTON_RIGHT_STICK_X,
	GAMEPAD_BUTTON_RIGHT_STICK_Y,
	GAMEPAD_BUTTON_AXIS_4, // L2/Left trigger
	GAMEPAD_BUTTON_AXIS_5, // R2/Right trigger
	GAMEPAD_BUTTON_AXIS_6,
	GAMEPAD_BUTTON_AXIS_7,
	GAMEPAD_BUTTON_AXIS_8,
	GAMEPAD_BUTTON_AXIS_9,
	GAMEPAD_BUTTON_AXIS_10,
	GAMEPAD_BUTTON_AXIS_11,
	GAMEPAD_BUTTON_AXIS_12,
	GAMEPAD_BUTTON_AXIS_13,
	GAMEPAD_BUTTON_AXIS_14,
	GAMEPAD_BUTTON_AXIS_15,
	GAMEPAD_BUTTON_AXIS_16,
	GAMEPAD_BUTTON_AXIS_17,
	GAMEPAD_BUTTON_AXIS_18,
	GAMEPAD_BUTTON_AXIS_19,
	GAMEPAD_BUTTON_AXIS_20,
	GAMEPAD_BUTTON_AXIS_21,
	GAMEPAD_BUTTON_AXIS_22,
	GAMEPAD_BUTTON_AXIS_23,
	GAMEPAD_BUTTON_AXIS_24,
	GAMEPAD_BUTTON_AXIS_25,
	GAMEPAD_BUTTON_AXIS_26,
	GAMEPAD_BUTTON_AXIS_27,
	GAMEPAD_BUTTON_AXIS_28,
	GAMEPAD_BUTTON_AXIS_29,
	GAMEPAD_BUTTON_AXIS_30,
	GAMEPAD_BUTTON_AXIS_31,
	GAMEPAD_BUTTON_ACCELERATION_X,
	GAMEPAD_BUTTON_ACCELERATION_Y,
	GAMEPAD_BUTTON_ACCELERATION_Z,
	GAMEPAD_BUTTON_GRAVITY_X,
	GAMEPAD_BUTTON_GRAVITY_Y,
	GAMEPAD_BUTTON_GRAVITY_Z,
	GAMEPAD_BUTTON_GYROSCOPE_X,
	GAMEPAD_BUTTON_GYROSCOPE_Y,
	GAMEPAD_BUTTON_GYROSCOPE_Z,
	GAMEPAD_BUTTON_MAGNETIC_FIELD_X,
	GAMEPAD_BUTTON_MAGNETIC_FIELD_Y,
	GAMEPAD_BUTTON_MAGNETIC_FIELD_Z,
	GAMEPAD_BUTTON_START,
	GAMEPAD_BUTTON_AXIS_COUNT_ = GAMEPAD_BUTTON_START,
	GAMEPAD_BUTTON_SELECT,
	GAMEPAD_BUTTON_LEFT,
	GAMEPAD_BUTTON_RIGHT,
	GAMEPAD_BUTTON_UP,
	GAMEPAD_BUTTON_DOWN,
	GAMEPAD_BUTTON_A, // Cross
	GAMEPAD_BUTTON_B, // Circle
	GAMEPAD_BUTTON_X, // Square
	GAMEPAD_BUTTON_Y, // Triangle
	GAMEPAD_BUTTON_L1,
	GAMEPAD_BUTTON_R1,
	GAMEPAD_BUTTON_L2,
	GAMEPAD_BUTTON_R2,
	GAMEPAD_BUTTON_L3, // Left thumb
	GAMEPAD_BUTTON_R3, // Right thumb
	GAMEPAD_BUTTON_HOME, // PS button
	GAMEPAD_BUTTON_17,
	GAMEPAD_BUTTON_18,
	GAMEPAD_BUTTON_19,
	GAMEPAD_BUTTON_20,
	GAMEPAD_BUTTON_21,
	GAMEPAD_BUTTON_22,
	GAMEPAD_BUTTON_23,
	GAMEPAD_BUTTON_24,
	GAMEPAD_BUTTON_25,
	GAMEPAD_BUTTON_26,
	GAMEPAD_BUTTON_27,
	GAMEPAD_BUTTON_28,
	GAMEPAD_BUTTON_29,
	GAMEPAD_BUTTON_30,
	GAMEPAD_BUTTON_31,
	GAMEPAD_BUTTON_MAX_,
	GAMEPAD_BUTTON_COUNT_ = GAMEPAD_BUTTON_MAX_ - GAMEPAD_BUTTON_AXIS_COUNT_
} GamepadButton;

// Keyboard (gainput::Key)
typedef enum KeyboardButton
{
	KEYBOARD_BUTTON_INVALID = -1,
	KEYBOARD_BUTTON_ESCAPE,
	KEYBOARD_BUTTON_F1,
	KEYBOARD_BUTTON_F2,
	KEYBOARD_BUTTON_F3,
	KEYBOARD_BUTTON_F4,
	KEYBOARD_BUTTON_F5,
	KEYBOARD_BUTTON_F6,
	KEYBOARD_BUTTON_F7,
	KEYBOARD_BUTTON_F8,
	KEYBOARD_BUTTON_F9,
	KEYBOARD_BUTTON_F10,
	KEYBOARD_BUTTON_F11,
	KEYBOARD_BUTTON_F12,
	KEYBOARD_BUTTON_F13,
	KEYBOARD_BUTTON_F14,
	KEYBOARD_BUTTON_F15,
	KEYBOARD_BUTTON_F16,
	KEYBOARD_BUTTON_F17,
	KEYBOARD_BUTTON_F18,
	KEYBOARD_BUTTON_F19,
	KEYBOARD_BUTTON_PRINT,
	KEYBOARD_BUTTON_SCROLL_LOCK,
	KEYBOARD_BUTTON_BREAK,

	KEYBOARD_BUTTON_SPACE = 0x0020,

	KEYBOARD_BUTTON_APOSTROPHE = 0x0027,
	KEYBOARD_BUTTON_COMMA = 0x002c,
	KEYBOARD_BUTTON_MINUS = 0x002d,
	KEYBOARD_BUTTON_PERIOD = 0x002e,
	KEYBOARD_BUTTON_SLASH = 0x002f,

	KEYBOARD_BUTTON_0 = 0x0030,
	KEYBOARD_BUTTON_1 = 0x0031,
	KEYBOARD_BUTTON_2 = 0x0032,
	KEYBOARD_BUTTON_3 = 0x0033,
	KEYBOARD_BUTTON_4 = 0x0034,
	KEYBOARD_BUTTON_5 = 0x0035,
	KEYBOARD_BUTTON_6 = 0x0036,
	KEYBOARD_BUTTON_7 = 0x0037,
	KEYBOARD_BUTTON_8 = 0x0038,
	KEYBOARD_BUTTON_9 = 0x0039,

	KEYBOARD_BUTTON_SEMICOLON = 0x003b,
	KEYBOARD_BUTTON_LESS = 0x003c,
	KEYBOARD_BUTTON_EQUAL = 0x003d,

	KEYBOARD_BUTTON_A = 0x0041,
	KEYBOARD_BUTTON_B = 0x0042,
	KEYBOARD_BUTTON_C = 0x0043,
	KEYBOARD_BUTTON_D = 0x0044,
	KEYBOARD_BUTTON_E = 0x0045,
	KEYBOARD_BUTTON_F = 0x0046,
	KEYBOARD_BUTTON_G = 0x0047,
	KEYBOARD_BUTTON_H = 0x0048,
	KEYBOARD_BUTTON_I = 0x0049,
	KEYBOARD_BUTTON_J = 0x004a,
	KEYBOARD_BUTTON_K = 0x004b,
	KEYBOARD_BUTTON_L = 0x004c,
	KEYBOARD_BUTTON_M = 0x004d,
	KEYBOARD_BUTTON_N = 0x004e,
	KEYBOARD_BUTTON_O = 0x004f,
	KEYBOARD_BUTTON_P = 0x0050,
	KEYBOARD_BUTTON_Q = 0x0051,
	KEYBOARD_BUTTON_R = 0x0052,
	KEYBOARD_BUTTON_S = 0x0053,
	KEYBOARD_BUTTON_T = 0x0054,
	KEYBOARD_BUTTON_U = 0x0055,
	KEYBOARD_BUTTON_V = 0x0056,
	KEYBOARD_BUTTON_W = 0x0057,
	KEYBOARD_BUTTON_X = 0x0058,
	KEYBOARD_BUTTON_Y = 0x0059,
	KEYBOARD_BUTTON_Z = 0x005a,

	KEYBOARD_BUTTON_BRACKET_LEFT = 0x005b,
	KEYBOARD_BUTTON_BACKSLASH = 0x005c,
	KEYBOARD_BUTTON_BRACKET_RIGHT = 0x005d,

	KEYBOARD_BUTTON_GRAVE = 0x0060,

	KEYBOARD_BUTTON_LEFT,
	KEYBOARD_BUTTON_RIGHT,
	KEYBOARD_BUTTON_UP,
	KEYBOARD_BUTTON_DOWN,
	KEYBOARD_BUTTON_INSERT,
	KEYBOARD_BUTTON_HOME,
	KEYBOARD_BUTTON_DELETE,
	KEYBOARD_BUTTON_END,
	KEYBOARD_BUTTON_PAGE_UP,
	KEYBOARD_BUTTON_PAGE_DOWN,

	KEYBOARD_BUTTON_NUM_LOCK,
	KEYBOARD_BUTTON_KP_EQUAL,
	KEYBOARD_BUTTON_KP_DIVIDE,
	KEYBOARD_BUTTON_KP_MULTIPLY,
	KEYBOARD_BUTTON_KP_SUBTRACT,
	KEYBOARD_BUTTON_KP_ADD,
	KEYBOARD_BUTTON_KP_ENTER,
	KEYBOARD_BUTTON_KP_INSERT, // 0
	KEYBOARD_BUTTON_KP_END, // 1
	KEYBOARD_BUTTON_KP_DOWN, // 2
	KEYBOARD_BUTTON_KP_PAGE_DOWN, // 3
	KEYBOARD_BUTTON_KP_LEFT, // 4
	KEYBOARD_BUTTON_KP_BEGIN, // 5
	KEYBOARD_BUTTON_KP_RIGHT, // 6
	KEYBOARD_BUTTON_KP_HOME, // 7
	KEYBOARD_BUTTON_KP_UP, // 8
	KEYBOARD_BUTTON_KP_PAGE_UP, // 9
	KEYBOARD_BUTTON_KP_DELETE, // ,

	KEYBOARD_BUTTON_BACK_SPACE,
	KEYBOARD_BUTTON_TAB,
	KEYBOARD_BUTTON_RETURN,
	KEYBOARD_BUTTON_CAPS_LOCK,
	KEYBOARD_BUTTON_SHIFT_L,
	KEYBOARD_BUTTON_CTRL_L,
	KEYBOARD_BUTTON_SUPER_L,
	KEYBOARD_BUTTON_ALT_L,
	KEYBOARD_BUTTON_ALT_R,
	KEYBOARD_BUTTON_SUPER_R,
	KEYBOARD_BUTTON_MENU,
	KEYBOARD_BUTTON_CTRL_R,
	KEYBOARD_BUTTON_SHIFT_R,

	KEYBOARD_BUTTON_BACK,
	KEYBOARD_BUTTON_SOFT_LEFT,
	KEYBOARD_BUTTON_SOFT_RIGHT,
	KEYBOARD_BUTTON_CALL,
	KEYBOARD_BUTTON_END_CALL,
	KEYBOARD_BUTTON_START,
	KEYBOARD_BUTTON_POUND,
	KEYBOARD_BUTTON_DPAD_CENTER,
	KEYBOARD_BUTTON_VOLUME_UP,
	KEYBOARD_BUTTON_VOLUME_DOWN,
	KEYBOARD_BUTTON_POWER,
	KEYBOARD_BUTTON_CAMERA,
	KEYBOARD_BUTTON_CLEAR,
	KEYBOARD_BUTTON_SYMBOL,
	KEYBOARD_BUTTON_EXPLORER,
	KEYBOARD_BUTTON_ENVELOPE,
	KEYBOARD_BUTTON_EQUALS,
	KEYBOARD_BUTTON_AT,
	KEYBOARD_BUTTON_HEADSET_HOOK,
	KEYBOARD_BUTTON_FOCUS,
	KEYBOARD_BUTTON_PLUS,
	KEYBOARD_BUTTON_NOTIFICATION,
	KEYBOARD_BUTTON_SEARCH,
	KEYBOARD_BUTTON_MEDIA_PLAY_PAUSE,
	KEYBOARD_BUTTON_MEDIA_STOP,
	KEYBOARD_BUTTON_MEDIA_NEXT,
	KEYBOARD_BUTTON_MEDIA_PREVIOUS,
	KEYBOARD_BUTTON_MEDIA_REWIND,
	KEYBOARD_BUTTON_MEDIA_FAST_FORWARD,
	KEYBOARD_BUTTON_MUTE,
	KEYBOARD_BUTTON_PICT_SYMBOLS,
	KEYBOARD_BUTTON_SWITCH_CHARSET,

	KEYBOARD_BUTTON_FORWARD,
	KEYBOARD_BUTTON_EXTRA1,
	KEYBOARD_BUTTON_EXTRA2,
	KEYBOARD_BUTTON_EXTRA3,
	KEYBOARD_BUTTON_EXTRA4,
	KEYBOARD_BUTTON_EXTRA5,
	KEYBOARD_BUTTON_EXTRA6,
	KEYBOARD_BUTTON_FN,

	KEYBOARD_BUTTON_CIRCUM_FLEX,
	KEYBOARD_BUTTON_S_SHARP,
	KEYBOARD_BUTTON_ACUTE,
	KEYBOARD_BUTTON_ALT_GR,
	KEYBOARD_BUTTON_NUMBER_SIGN,
	KEYBOARD_BUTTON_U_DIAERESIS,
	KEYBOARD_BUTTON_A_DIAERESIS,
	KEYBOARD_BUTTON_O_DIAERESIS,
	KEYBOARD_BUTTON_SECTION,
	KEYBOARD_BUTTON_A_RING,
	KEYBOARD_BUTTON_DIAERESIS,
	KEYBOARD_BUTTON_TWO_SUPERIOR,
	KEYBOARD_BUTTON_RIGHT_PARENTHESIS,
	KEYBOARD_BUTTON_DOLLAR,
	KEYBOARD_BUTTON_U_GRAVE,
	KEYBOARD_BUTTON_ASTERISK,
	KEYBOARD_BUTTON_COLON,
	KEYBOARD_BUTTON_EXCLAM,

	KEYBOARD_BUTTON_BRACE_LEFT,
	KEYBOARD_BUTTON_BRACE_RIGHT,
	KEYBOARD_BUTTON_SYS_RQ,

	KEYBOARD_BUTTON_COUNT_
} KeyboardButton;

// Keyboard (gainput::MouseButton)
typedef enum MouseButton
{
	MOUSE_BUTTON_0 = 0,
	MOUSE_BUTTON_LEFT = MOUSE_BUTTON_0,
	MOUSE_BUTTON_1,
	MOUSE_BUTTON_MIDDLE = MOUSE_BUTTON_1,
	MOUSE_BUTTON_2,
	MOUSE_BUTTON_RIGHT = MOUSE_BUTTON_2,
	MOUSE_BUTTON_3,
	MOUSE_BUTTON_WHEEL_UP = MOUSE_BUTTON_3,
	MOUSE_BUTTON_4,
	MOUSE_BUTTON_WHEEL_DOWN = MOUSE_BUTTON_4,
	MOUSE_BUTTON_5,
	MOUSE_BUTTON_6,
	MOUSE_BUTTON_7,
	MOUSE_BUTTON_8,
	MOUSE_BUTTON_9,
	MOUSE_BUTTON_10,
	MOUSE_BUTTON_11,
	MOUSE_BUTTON_12,
	MOUSE_BUTTON_13,
	MOUSE_BUTTON_14,
	MOUSE_BUTTON_15,
	MOUSE_BUTTON_16,
	MOUSE_BUTTON_17,
	MOUSE_BUTTON_18,
	MOUSE_BUTTON_19,
	MOUSE_BUTTON_20,
	MOUSE_BUTTON_MAX = MOUSE_BUTTON_20,
	MOUSE_BUTTON_COUNT,
	MOUSE_BUTTON_AXIS_X = MOUSE_BUTTON_COUNT,
	MOUSE_BUTTON_AXIS_Y,
	MOUSE_BUTTON_COUNT_,
	MOUSE_BUTTON_AXIS_COUNT = MOUSE_BUTTON_COUNT_ - MOUSE_BUTTON_AXIS_X
} MouseButton;

// Touch (gainput::GestureType)
typedef enum TouchGesture
{
	TOUCH_GESTURE_TAP,
	TOUCH_GESTURE_PAN,
} TouchGesture;

// Area of the screen that will trigger a virtual joystick
enum TouchVirtualJoystickScreenArea
{
	AREA_LEFT = 0,
	AREA_RIGHT,
};

// The type of action mapping
typedef enum InputActionMappingType
{
	// A normal action mapping (ex. exit action -> esc button)
	INPUT_ACTION_MAPPING_NORMAL = 0,
	// A composite action mapping (ex. camera translate action -> DA/WS mapping to stick axis X & Y)
	INPUT_ACTION_MAPPING_COMPOSITE,
	// A combo action mapping (ex. full screen action -> alt+enter) 
	INPUT_ACTION_MAPPING_COMBO,
	// A touch gesture (ex. tap, pinch, pan, etc...)
	INPUT_ACTION_MAPPING_TOUCH_GESTURE,
	// A touch virtual joystick
	INPUT_ACTION_MAPPING_TOUCH_VIRTUAL_JOYSTICK
} InputActionMappingType;

// Action mapping device target.
// Defines which kind of device the actions mapping should target
typedef enum InputActionMappingDeviceTarget
{
	INPUT_ACTION_MAPPING_TARGET_ALL = 0,
	INPUT_ACTION_MAPPING_TARGET_CONTROLLER,		// Supported action mapping type: INPUT_ACTION_MAPPING_NORMAL, INPUT_ACTION_MAPPING_COMPOSITE, INPUT_ACTION_MAPPING_COMBO
	INPUT_ACTION_MAPPING_TARGET_KEYBOARD,		// Supported action mapping type: INPUT_ACTION_MAPPING_NORMAL, INPUT_ACTION_MAPPING_COMPOSITE, INPUT_ACTION_MAPPING_COMBO
	INPUT_ACTION_MAPPING_TARGET_MOUSE,			// Supported action mapping type: INPUT_ACTION_MAPPING_NORMAL, INPUT_ACTION_MAPPING_COMPOSITE, INPUT_ACTION_MAPPING_COMBO
	INPUT_ACTION_MAPPING_TARGET_TOUCH,			// Supported action mapping type: INPUT_ACTION_MAPPING_NORMAL (for basic touch detection), INPUT_ACTION_MAPPING_TOUCH_GESTURE, INPUT_ACTION_MAPPING_TOUCH_VIRTUAL_JOYSTICK
} InputActionMappingDeviceTarget;

// An action mapping description.  Used to define action mappings to be provided to the input system.
typedef struct ActionMappingDesc
{	//-V802 : Very user-facing struct, and order is highly important to convenience
	// The type of the action mapping
	InputActionMappingType mActionMappingType = INPUT_ACTION_MAPPING_NORMAL;

	// Type of device this action targets
	// NOTE: cannot be INPUT_ACTION_MAPPING_TARGET_ALL.  This will cause an assertion in addActionMappings(...).
	InputActionMappingDeviceTarget mActionMappingDeviceTarget = INPUT_ACTION_MAPPING_TARGET_CONTROLLER;

	// A unique ID associated with the action.
	uint32_t mActionId = UINT_MAX;

	// Device buttons that triggers the action (from the GamepadButton/KeyboardButton/MouseButton/TouchGesture enums)
	// For INPUT_ACTION_MAPPING_NORMAL, only the first element will be used (unless it's targetting touch device.  mDeviceButtons will not be used but any touch will act as a "button" press).
	// For INPUT_ACTION_MAPPING_COMPOSITE, all four elements will be used to map 2 axes (element 0->+X, 1->-X, 2->+Y, 3->-Y)
	// For INPUT_ACTION_MAPPING_COMBO, the first two elements will be used (element 0 is the button to hold, element 1 causes the action to trigger)
	int32_t mDeviceButtons[4] = {0};

	// Used for axis buttons
	// For example, if targetting the left joystick, and we want to handle both the x and y axes,
	// then mDeviceButtons[0] should be the start axis (GAMEPAD_BUTTON_LEFT_STICK_X), and mNumAxis should be 2.
	// mNumAxis should be either 1 or 2 when targetting an axis button.
	uint8_t mNumAxis = 1;

	// User id associated with the action (relevant for controller and touch inputs to track fingers)
	uint8_t mUserId = 0;

	// Used with INPUT_ACTION_MAPPING_TOUCH_VIRTUAL_JOYSTICK to tune virtual joystick behavior
	float mDeadzone = 20.f;
	float mOutsideRadius = 200.f;
	float mScale = 1.f;  // Scales the values from this action mapping for virtual joysticks and mice.
	TouchVirtualJoystickScreenArea mVirtualJoystickScreenArea = AREA_LEFT;

	bool mScaleByDT = false;
} ActionMappingDesc;

// UI System reserved input action mapping IDs
// These are reserved action mapping IDs used to drive the UI system
typedef struct UISystemInputActions
{
	typedef enum UISystemInputAction
	{
		UI_ACTION_START_ID_ = MAX_INPUT_ACTIONS - 64,  // Reserve last 64 actions for UI action mappings

		// Keyboard specific buttons
		UI_ACTION_KEY_TAB,
		UI_ACTION_KEY_LEFT_ARROW,
		UI_ACTION_KEY_RIGHT_ARROW,
		UI_ACTION_KEY_UP_ARROW,
		UI_ACTION_KEY_DOWN_ARROW,
		UI_ACTION_KEY_PAGE_UP,
		UI_ACTION_KEY_PAGE_DOWN,
		UI_ACTION_KEY_HOME,
		UI_ACTION_KEY_END,
		UI_ACTION_KEY_INSERT,
		UI_ACTION_KEY_DELETE,
		UI_ACTION_KEY_BACK_SPACE,
		UI_ACTION_KEY_SPACE,
		UI_ACTION_KEY_ENTER,
		UI_ACTION_KEY_ESCAPE,
		UI_ACTION_KEY_CONTROL_L,
		UI_ACTION_KEY_CONTROL_R,
		UI_ACTION_KEY_SHIFT_L,
		UI_ACTION_KEY_SHIFT_R,
		UI_ACTION_KEY_ALT_L,
		UI_ACTION_KEY_ALT_R,
		UI_ACTION_KEY_SUPER_L,
		UI_ACTION_KEY_SUPER_R,
		UI_ACTION_KEY_A, // for select all (ctrl+a)
		UI_ACTION_KEY_C, // for copy (ctrl+c)
		UI_ACTION_KEY_V, // for paste (ctrl+v)
		UI_ACTION_KEY_X, // for cut (ctrl+x)
		UI_ACTION_KEY_Y, // for redo (ctrl+y)
		UI_ACTION_KEY_Z, // for undo (ctrl+z)
		UI_ACTION_KEY_F2,

		// Mouse specific buttons
		UI_ACTION_MOUSE_LEFT,
		UI_ACTION_MOUSE_RIGHT,
		UI_ACTION_MOUSE_MIDDLE,
		UI_ACTION_MOUSE_SCROLL_UP,
		UI_ACTION_MOUSE_SCROLL_DOWN,

		// Navigation
		UI_ACTION_NAV_TOGGLE_UI,				// toggles the user interface					// e.g. L3, ~ (Keyboard)
		UI_ACTION_NAV_ACTIVATE,					// activate / open / toggle / tweak value       // e.g. Cross  (PS4), A (Xbox), A (Switch), Space (Keyboard)
		UI_ACTION_NAV_CANCEL,					// cancel / close / exit                        // e.g. Circle (PS4), B (Xbox), B (Switch), Escape (Keyboard)
		UI_ACTION_NAV_INPUT,					// text input / on-screen keyboard              // e.g. Triang.(PS4), Y (Xbox), X (Switch), Return (Keyboard)
		UI_ACTION_NAV_MENU,						// tap: toggle menu / hold: focus, move, resize // e.g. Square (PS4), X (Xbox), Y (Switch), Alt (Keyboard)
		UI_ACTION_NAV_TWEAK_WINDOW_LEFT,		// move / tweak / resize window (w/ PadMenu)    // e.g. D-pad Left/Right/Up/Down (Gamepads), Arrow keys (Keyboard)
		UI_ACTION_NAV_TWEAK_WINDOW_RIGHT,
		UI_ACTION_NAV_TWEAK_WINDOW_UP,
		UI_ACTION_NAV_TWEAK_WINDOW_DOWN,
		UI_ACTION_NAV_SCROLL_MOVE_WINDOW,		// scroll / move window (w/ PadMenu)            // e.g. Left Analog Stick Left/Right/Up/Down 
		UI_ACTION_NAV_FOCUS_PREV,				// previous window (w/ PadMenu)                 // e.g. L1 or L2 (PS4), LB or LT (Xbox), L or ZL (Switch)
		UI_ACTION_NAV_FOCUS_NEXT,				// next window (w/ PadMenu)                     // e.g. R1 or R2 (PS4), RB or RT (Xbox), R or ZL (Switch)
		UI_ACTION_NAV_TWEAK_SLOW,				// slower tweaks                                // e.g. L1 or L2 (PS4), LB or LT (Xbox), L or ZL (Switch)
		UI_ACTION_NAV_TWEAK_FAST,				// faster tweaks                                // e.g. R1 or R2 (PS4), RB or RT (Xbox), R or ZL (Switch)
	} UISystemInputAction;
} UISystemInputActions;


// By default, the input system will create default action mappings.  The following enums declare the names (and unique IDs) for these actions.  These default actions are generic to map to multiple type of devices.
// On the application side, similar action mappings can be data-driven to then be provided to the input system as the current action mappings to use.  Furthermore, the application could have different sets of 
// action mappings for different type of devices (controller, keyboard, mouse).
// Depending on the current state of the application, a different app-specific action mappings could be made active with the input system.  
// For example, the application could have action mappings for:
// - UI actions, where an action "SELECT" could map to button A
// - Vehicle actions, where an action "ACCELERATE" could map to button A
// - Character actions, where an action "ATTACK" could map to button A 

typedef struct DefaultInputActions
{
	typedef enum DefaultInputAction
	{
		// Default actions for unit-tests
		TRANSLATE_CAMERA = 0,		// Left joystick | Composite for keyboard (DA/WS)
		ROTATE_CAMERA,				// Right joystick | Composite for keyboard (LJ/IK)
		CAPTURE_INPUT,				// Left click
		RESET_CAMERA,				// Y/TRIANGLE | Left mouse button | Spacebar
		DUMP_PROFILE_DATA,			// Combo for controllers (start+B/Circle) | F3
		TOGGLE_FULLSCREEN,			// KB only combo: alt+enter
		EXIT,						// Esc

		// Default actions for UI
		UI_KEY_TAB					= UISystemInputActions::UI_ACTION_KEY_TAB,
		UI_KEY_LEFT_ARROW			= UISystemInputActions::UI_ACTION_KEY_LEFT_ARROW,
		UI_KEY_RIGHT_ARROW			= UISystemInputActions::UI_ACTION_KEY_RIGHT_ARROW,
		UI_KEY_UP_ARROW				= UISystemInputActions::UI_ACTION_KEY_UP_ARROW,
		UI_KEY_DOWN_ARROW			= UISystemInputActions::UI_ACTION_KEY_DOWN_ARROW,
		UI_KEY_PAGE_UP				= UISystemInputActions::UI_ACTION_KEY_PAGE_UP,
		UI_KEY_PAGE_DOWN			= UISystemInputActions::UI_ACTION_KEY_PAGE_DOWN,
		UI_KEY_HOME					= UISystemInputActions::UI_ACTION_KEY_HOME,
		UI_KEY_END					= UISystemInputActions::UI_ACTION_KEY_END,
		UI_KEY_INSERT				= UISystemInputActions::UI_ACTION_KEY_INSERT,
		UI_KEY_DELETE				= UISystemInputActions::UI_ACTION_KEY_DELETE,
		UI_KEY_BACK_SPACE			= UISystemInputActions::UI_ACTION_KEY_BACK_SPACE,
		UI_KEY_SPACE				= UISystemInputActions::UI_ACTION_KEY_SPACE,
		UI_KEY_ENTER				= UISystemInputActions::UI_ACTION_KEY_ENTER,
		UI_KEY_ESCAPE				= UISystemInputActions::UI_ACTION_KEY_ESCAPE,
		UI_KEY_CONTROL_L			= UISystemInputActions::UI_ACTION_KEY_CONTROL_L,
		UI_KEY_CONTROL_R			= UISystemInputActions::UI_ACTION_KEY_CONTROL_R,
		UI_KEY_SHIFT_L				= UISystemInputActions::UI_ACTION_KEY_SHIFT_L,
		UI_KEY_SHIFT_R				= UISystemInputActions::UI_ACTION_KEY_SHIFT_R,
		UI_KEY_ALT_L				= UISystemInputActions::UI_ACTION_KEY_ALT_L,
		UI_KEY_ALT_R				= UISystemInputActions::UI_ACTION_KEY_ALT_R,
		UI_KEY_SUPER_L				= UISystemInputActions::UI_ACTION_KEY_SUPER_L,
		UI_KEY_SUPER_R				= UISystemInputActions::UI_ACTION_KEY_SUPER_R,
		UI_KEY_A					= UISystemInputActions::UI_ACTION_KEY_A,
		UI_KEY_C					= UISystemInputActions::UI_ACTION_KEY_C,
		UI_KEY_V					= UISystemInputActions::UI_ACTION_KEY_V,
		UI_KEY_X					= UISystemInputActions::UI_ACTION_KEY_X,
		UI_KEY_Y					= UISystemInputActions::UI_ACTION_KEY_Y,
		UI_KEY_Z					= UISystemInputActions::UI_ACTION_KEY_Z,
		UI_KEY_F2					= UISystemInputActions::UI_ACTION_KEY_F2,

		// Mouse specific buttons
		UI_MOUSE_LEFT				= UISystemInputActions::UI_ACTION_MOUSE_LEFT,
		UI_MOUSE_RIGHT				= UISystemInputActions::UI_ACTION_MOUSE_RIGHT,
		UI_MOUSE_MIDDLE				= UISystemInputActions::UI_ACTION_MOUSE_MIDDLE,
		UI_MOUSE_SCROLL_UP			= UISystemInputActions::UI_ACTION_MOUSE_SCROLL_UP,
		UI_MOUSE_SCROLL_DOWN		= UISystemInputActions::UI_ACTION_MOUSE_SCROLL_DOWN,

		// Navigation
		UI_NAV_TOGGLE_UI			= UISystemInputActions::UI_ACTION_NAV_TOGGLE_UI,
		UI_NAV_ACTIVATE				= UISystemInputActions::UI_ACTION_NAV_ACTIVATE,
		UI_NAV_CANCEL				= UISystemInputActions::UI_ACTION_NAV_CANCEL,
		UI_NAV_INPUT				= UISystemInputActions::UI_ACTION_NAV_INPUT,
		UI_NAV_MENU					= UISystemInputActions::UI_ACTION_NAV_MENU,
		UI_NAV_TWEAK_WINDOW_LEFT	= UISystemInputActions::UI_ACTION_NAV_TWEAK_WINDOW_LEFT,
		UI_NAV_TWEAK_WINDOW_RIGHT	= UISystemInputActions::UI_ACTION_NAV_TWEAK_WINDOW_RIGHT,
		UI_NAV_TWEAK_WINDOW_UP		= UISystemInputActions::UI_ACTION_NAV_TWEAK_WINDOW_UP,
		UI_NAV_TWEAK_WINDOW_DOWN	= UISystemInputActions::UI_ACTION_NAV_TWEAK_WINDOW_DOWN,
		UI_NAV_SCROLL_MOVE_WINDOW	= UISystemInputActions::UI_ACTION_NAV_SCROLL_MOVE_WINDOW,
		UI_NAV_FOCUS_PREV			= UISystemInputActions::UI_ACTION_NAV_FOCUS_PREV,
		UI_NAV_FOCUS_NEXT			= UISystemInputActions::UI_ACTION_NAV_FOCUS_NEXT,
		UI_NAV_TWEAK_SLOW			= UISystemInputActions::UI_ACTION_NAV_TWEAK_SLOW,
		UI_NAV_TWEAK_FAST			= UISystemInputActions::UI_ACTION_NAV_TWEAK_FAST
	} DefaultInputAction;
} DefaultInputActions;


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
	void*		pUserData = NULL;
	/// Indices of fingers for detected gesture
	int32_t		mFingerIndices[MAX_INPUT_MULTI_TOUCHES] = {0};
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

	float2*     pPosition = NULL;
	const bool* pCaptured = NULL;
	float       mScrollValue = 0.f;
	uint32_t    mActionId = UINT_MAX;
	/// What phase is the action currently in
	uint8_t mPhase = INPUT_ACTION_PHASE_ENDED;
	uint8_t mDeviceType = INPUT_DEVICE_INVALID;
} InputActionContext;

typedef bool (*InputActionCallback)(InputActionContext* pContext);

typedef struct InputActionDesc
{	//-V802 : Very user-facing struct, and order is highly important to convenience
	/// Action ID 
	uint32_t mActionId = UINT_MAX;
	/// Callback when an action is initiated, performed or canceled
	InputActionCallback pFunction = NULL;
	/// User data which will be assigned to InputActionContext::pUserData when calling pFunction
	void* pUserData = NULL;
	/// User management (which user does this action apply to)
	uint8_t mUserId = 0u;

	bool operator==(InputActionDesc const& rhs) const
	{
		// We only care about action ID and user ID when comparing action descs
		return (this->mActionId == rhs.mActionId && this->mUserId == rhs.mUserId);
	}
} InputActionDesc;

// Global actions
// These are handled differently than actions from an action mapping.
typedef struct GlobalInputActionDesc
{
	typedef enum GlobalInputActionType
	{
		// Triggered when any action mapping of a button is triggered
		ANY_BUTTON_ACTION = 0,
		// Used for processing text from keyboard or virtual keyboard
		TEXT
	} GlobalInputActionType;

	GlobalInputActionType mGlobalInputActionType = ANY_BUTTON_ACTION;

	/// Callback when an action is initiated, performed or canceled
	InputActionCallback pFunction = NULL;
	/// User data which will be assigned to InputActionContext::pUserData when calling pFunction
	void* pUserData = NULL;
} GlobalInputActionDesc;

typedef struct InputSystemDesc
{

	Renderer*      pRenderer = NULL;
	WindowDesc*	   pWindow = NULL;
	
	bool           mDisableVirtualJoystick = false; 

} InputSystemDesc;

FORGE_API bool         initInputSystem(InputSystemDesc* pDesc);
FORGE_API void         exitInputSystem();
FORGE_API void         updateInputSystem(float deltaTime, uint32_t width, uint32_t height);

// Input action (callbacks) functionalities //////////////////
/* Adds a new action (callback) for a specific (or all) device target(s) 
 * If INPUT_ACTION_MAPPING_TARGET_ALL is specified, an action will be mapped for all device targets 
 * that have the action ID specified in pDesc.
 */
FORGE_API void		 addInputAction(const InputActionDesc* pDesc, const InputActionMappingDeviceTarget actionMappingTarget = INPUT_ACTION_MAPPING_TARGET_ALL);
FORGE_API void         removeInputAction(const InputActionDesc* pDesc, const InputActionMappingDeviceTarget actionMappingTarget = INPUT_ACTION_MAPPING_TARGET_ALL);
FORGE_API void		 setGlobalInputAction(const GlobalInputActionDesc* pDesc);
//////////////////////////////////////////////////////////////

// Input action mappings functionalities /////////////////////
/* Adds new action mappings for a specific (or all) device target(s)
 * Takes in an array of ActionMappingDesc
 * Note: this will clear all current action mappings, including any callbacks. 
 */
FORGE_API void addActionMappings(ActionMappingDesc* const actionMappings, const uint32_t numActions, const InputActionMappingDeviceTarget actionMappingTarget);

/* Removes currently active action mappings
 * Note: this will clear any callbacks that were added for any action mapping being cleared.
 */
FORGE_API void removeActionMappings(const InputActionMappingDeviceTarget actionMappingTarget);

/* Adds default TF action mappings
 * Note: this will clear all current action mappings, including any callbacks.
 */
FORGE_API void addDefaultActionMappings();
//////////////////////////////////////////////////////////////

FORGE_API bool setEnableCaptureInput(bool enable);

/// Used to enable/disable text input for non-keyboard setups (virtual keyboards for console/mobile, ...)
FORGE_API void setVirtualKeyboard(uint32_t type);

FORGE_API void setDeadZone(unsigned int controllerIndex, float deadZoneSize);
FORGE_API void setLEDColor(int gamePadIndex, uint8_t r, uint8_t g, uint8_t b);

// if gamepad index == BUILTIN_DEVICE_HAPTICS it will try to set rumble on the actual device
// in the case of iOS and iphones that would mean the actual phone vibrates
FORGE_API bool setRumbleEffect(int gamePadIndex, float left_motor, float right_motor, uint32_t duration_ms);

FORGE_API const char* getGamePadName(int gamePadIndex);
FORGE_API bool 		gamePadConnected(int gamePadIndex);
FORGE_API void 		setOnDeviceChangeCallBack(void (*onDeviceChnageCallBack)(const char* name, bool added, int gamePadIndex));
