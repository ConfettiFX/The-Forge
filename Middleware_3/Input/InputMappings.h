#ifndef INPUT_MAPPING_H
#define INPUT_MAPPING_H
#pragma once
#include "InputSystem.h"

enum UserInputKeys
{
	//if Mouse then
	//Left trigger maps to space
	//Right trigger maps to E
	KEY_LEFT_TRIGGER = 0,
	KEY_RIGHT_TRIGGER,

	//Left bumper maps to left shift
	//Right bumper maps to right click
	KEY_LEFT_BUMPER,
	KEY_RIGHT_BUMPER,

	//This would map to wasd on windows or left stick on xbox
	KEY_LEFT_STICK,
	//this maps to mouse movement or right joystick
	KEY_RIGHT_STICK,

	//Left joystick press or F1
	KEY_LEFT_STICK_BUTTON,
	//right joystick press or F2
	KEY_RIGHT_STICK_BUTTON,

	//TODO: Decide which. arrow keys, dPad, Q and E
	KEY_PAD_UP,
	KEY_PAD_DOWN,
	KEY_PAD_LEFT,
	KEY_PAD_RIGHT,

	//Maps to A on controller
	//Maps to left click of mouse
	KEY_CONFIRM,

	//Maps to B on controller
	//Maps to Escape on keyboard
	KEY_CANCEL,

	//Maps to X on controller
	//Maps to Q on keyboard
	KEY_BUTTON_X,
	//Maps to Y on controller
	//Maps to F on keyboard
	KEY_BUTTON_Y,

	//Maps to Mouse on Windows
	KEY_UI_MOVE,

	//Maps to Menu button on controller
	//Maps to Return on keyboard
	KEY_MENU,

	KEY_JOYSTICK_CONTROLS,
	KEY_MOUSE_WHEEL,
	KEY_MOUSE_WHEEL_BUTTON,
	//Virtual keyboards or actual keyboards will need these.
	KEY_LEFT_SHIFT,
	KEY_RIGHT_SHIFT,
	KEY_RIGHT_CTRL,
	KEY_LEFT_CTRL,
	KEY_RIGHT_ALT,
	KEY_LEFT_ALT,
	KEY_RIGHT_SUPER,    //windows key, cmd key
	KEY_LEFT_SUPER,     //windows key, cmd key

	KEY_DELETE,

	KEY_CHAR,

	GESTURE_SWIPE_2,
	KEY_COUNT
};

//TODO: Add callbacks to mappings as Actions
//this will unify iOS as joysticks are virtual and can be mapped to any finger.
//TODO: Separate per device for simpler GetButtonData
static KeyMappingDescription gUserKeys[] = {

	//Triggers
	//Keyboard
	{ KEY_LEFT_TRIGGER, GainputDeviceType::GAINPUT_KEYBOARD, 1, { { INPUT_X_AXIS, 1, gainput::KeySpace }, {}, {}, {} } },
	{ KEY_RIGHT_TRIGGER, GainputDeviceType::GAINPUT_KEYBOARD, 1, { { INPUT_X_AXIS, 1, gainput::KeyE }, {}, {}, {} } },

	//Bumbers
	//Keyboard/Mouse
	{ KEY_LEFT_BUMPER, GainputDeviceType::GAINPUT_KEYBOARD, 1, { { INPUT_X_AXIS, 1, gainput::KeyShiftL }, {}, {}, {} } },
	{ KEY_RIGHT_BUMPER, GainputDeviceType::GAINPUT_MOUSE, 1, { { INPUT_X_AXIS, 1, gainput::MouseButton2 }, {}, {}, {} } },

	//STICKS
	//Keyboard/Mouse
	{ KEY_LEFT_STICK,
	  GainputDeviceType::GAINPUT_KEYBOARD,
	  4,
	  { { INPUT_X_AXIS, 1, gainput::KeyD },
		{ INPUT_X_AXIS, -1, gainput::KeyA },
		{ INPUT_Y_AXIS, 1, gainput::KeyW },
		{ INPUT_Y_AXIS, -1, gainput::KeyS } } },

	{ KEY_UI_MOVE,
	  GainputDeviceType::GAINPUT_MOUSE,
	  2,
	  { { INPUT_X_AXIS, 1, gainput::MouseAxisX }, { INPUT_Y_AXIS, 1, gainput::MouseAxisY }, {}, {} } },
	{ KEY_RIGHT_STICK,
	  GainputDeviceType::GAINPUT_RAW_MOUSE,
	  2,
	  { { INPUT_X_AXIS, 1, gainput::MouseAxisX }, { INPUT_Y_AXIS, 1, gainput::MouseAxisY }, {}, {} } },

	//TOUCH
	{ KEY_UI_MOVE,
	  GainputDeviceType::GAINPUT_TOUCH,
	  2,
	  { { INPUT_X_AXIS, 1, gainput::Touch0X }, { INPUT_Y_AXIS, 1, gainput::Touch0Y }, {}, {} } },

	{ KEY_RIGHT_STICK,
	  GainputDeviceType::GAINPUT_TOUCH,
	  2,
	  { { INPUT_X_AXIS, 1, gainput::Touch1X }, { INPUT_Y_AXIS, 1, gainput::Touch1Y }, {}, {} } },
	{ KEY_RIGHT_STICK,
	  GainputDeviceType::GAINPUT_TOUCH,
	  2,
	  { { INPUT_X_AXIS, 1, gainput::Touch0X }, { INPUT_Y_AXIS, 1, gainput::Touch0Y }, {}, {} } },

	{ KEY_LEFT_STICK,
	  GainputDeviceType::GAINPUT_TOUCH,
	  2,
	  { { INPUT_X_AXIS, 1, gainput::Touch0X }, { INPUT_Y_AXIS, 1, gainput::Touch0Y }, {}, {} } },
	{ KEY_LEFT_STICK,
	  GainputDeviceType::GAINPUT_TOUCH,
	  2,
	  { { INPUT_X_AXIS, 1, gainput::Touch1X }, { INPUT_Y_AXIS, 1, gainput::Touch1Y }, {}, {} } },

	//PAD
	//KEYBOARD
	{ KEY_PAD_UP, GainputDeviceType::GAINPUT_KEYBOARD, 1, { { INPUT_X_AXIS, 1, gainput::KeyUp }, {}, {}, {} } },
	{ KEY_PAD_DOWN, GainputDeviceType::GAINPUT_KEYBOARD, 1, { { INPUT_X_AXIS, -1, gainput::KeyDown }, {}, {}, {} } },
	{ KEY_PAD_LEFT, GainputDeviceType::GAINPUT_KEYBOARD, 1, { { INPUT_X_AXIS, -1, gainput::KeyLeft }, {}, {}, {} } },
	{ KEY_PAD_RIGHT, GainputDeviceType::GAINPUT_KEYBOARD, 1, { { INPUT_X_AXIS, 1, gainput::KeyRight }, {}, {}, {} } },

	//this will map to x y buttons
	{ KEY_BUTTON_X, GainputDeviceType::GAINPUT_KEYBOARD, 1, { { INPUT_X_AXIS, 1, gainput::KeyQ }, {}, {}, {} } },
	{ KEY_BUTTON_Y, GainputDeviceType::GAINPUT_KEYBOARD, 1, { { INPUT_X_AXIS, 1, gainput::KeyF }, {}, {}, {} } },

	//this will map to L3 R3
	{ KEY_LEFT_STICK_BUTTON, GainputDeviceType::GAINPUT_KEYBOARD, 1, { { INPUT_X_AXIS, 1, gainput::KeyF1 }, {}, {}, {} } },
	{ KEY_RIGHT_STICK_BUTTON, GainputDeviceType::GAINPUT_KEYBOARD, 1, { { INPUT_X_AXIS, 1, gainput::KeyBackSpace }, {}, {}, {} } },

	//CONFIRM
	//MOUSE
	{ KEY_CONFIRM, GainputDeviceType::GAINPUT_MOUSE, 1, { { INPUT_X_AXIS, 1, gainput::MouseButton0 }, {}, {}, {} } },
	{ KEY_CONFIRM, GainputDeviceType::GAINPUT_TOUCH, 1, { { INPUT_X_AXIS, 1, gainput::Touch0Down }, {}, {}, {} } },

	//CANCEL
	//Keyboard
	{ KEY_CANCEL, GainputDeviceType::GAINPUT_KEYBOARD, 1, { { INPUT_X_AXIS, 1, gainput::KeyEscape }, {}, {}, {} } },

	//MENU
	//Keyboard
	{ KEY_MENU, GainputDeviceType::GAINPUT_KEYBOARD, 1, { { INPUT_X_AXIS, 1, gainput::KeyReturn }, {}, {}, {} } },

	//Mouse Wheel
	{ KEY_MOUSE_WHEEL,
	  GainputDeviceType::GAINPUT_MOUSE,
	  2,
	  { { INPUT_X_AXIS, 1, gainput::MouseButtonWheelUp }, { INPUT_X_AXIS, -1, gainput::MouseButtonWheelDown }, {}, {} } },
	{ KEY_MOUSE_WHEEL_BUTTON, GainputDeviceType::GAINPUT_MOUSE, 1, { { INPUT_X_AXIS, 1, gainput::MouseButtonMiddle }, {}, {}, {} } },

	{ KEY_LEFT_SHIFT, GainputDeviceType::GAINPUT_KEYBOARD, 1, { { INPUT_X_AXIS, 1, gainput::KeyShiftL }, {}, {}, {} } },
	{ KEY_RIGHT_SHIFT, GainputDeviceType::GAINPUT_KEYBOARD, 1, { { INPUT_X_AXIS, 1, gainput::KeyShiftR }, {}, {}, {} } },
	{ KEY_RIGHT_CTRL, GainputDeviceType::GAINPUT_KEYBOARD, 1, { { INPUT_X_AXIS, 1, gainput::KeyCtrlR }, {}, {}, {} } },
	{ KEY_LEFT_CTRL, GainputDeviceType::GAINPUT_KEYBOARD, 1, { { INPUT_X_AXIS, 1, gainput::KeyCtrlL }, {}, {}, {} } },
	{ KEY_RIGHT_ALT, GainputDeviceType::GAINPUT_KEYBOARD, 1, { { INPUT_X_AXIS, 1, gainput::KeyAltR }, {}, {}, {} } },
	{ KEY_LEFT_ALT, GainputDeviceType::GAINPUT_KEYBOARD, 1, { { INPUT_X_AXIS, 1, gainput::KeyAltL }, {}, {}, {} } },
	{ KEY_RIGHT_SUPER, GainputDeviceType::GAINPUT_KEYBOARD, 1, { { INPUT_X_AXIS, 1, gainput::KeySuperR }, {}, {}, {} } },
	{ KEY_LEFT_SUPER, GainputDeviceType::GAINPUT_KEYBOARD, 1, { { INPUT_X_AXIS, 1, gainput::KeySuperL }, {}, {}, {} } },
	{ KEY_DELETE, GainputDeviceType::GAINPUT_KEYBOARD, 1, { { INPUT_X_AXIS, 1, gainput::KeyDelete }, {}, {}, {} } },

	//Keyboard chars
	{ KEY_CHAR, GainputDeviceType::GAINPUT_KEYBOARD, 1, { { INPUT_X_AXIS, 1, gainput::KeyA }, {}, {}, {} } },
	{ KEY_CHAR, GainputDeviceType::GAINPUT_KEYBOARD, 1, { { INPUT_X_AXIS, 1, gainput::KeyB }, {}, {}, {} } },
	{ KEY_CHAR, GainputDeviceType::GAINPUT_KEYBOARD, 1, { { INPUT_X_AXIS, 1, gainput::KeyC }, {}, {}, {} } },
	{ KEY_CHAR, GainputDeviceType::GAINPUT_KEYBOARD, 1, { { INPUT_X_AXIS, 1, gainput::KeyD }, {}, {}, {} } },
	{ KEY_CHAR, GainputDeviceType::GAINPUT_KEYBOARD, 1, { { INPUT_X_AXIS, 1, gainput::KeyE }, {}, {}, {} } },
	{ KEY_CHAR, GainputDeviceType::GAINPUT_KEYBOARD, 1, { { INPUT_X_AXIS, 1, gainput::KeyF }, {}, {}, {} } },
	{ KEY_CHAR, GainputDeviceType::GAINPUT_KEYBOARD, 1, { { INPUT_X_AXIS, 1, gainput::KeyG }, {}, {}, {} } },
	{ KEY_CHAR, GainputDeviceType::GAINPUT_KEYBOARD, 1, { { INPUT_X_AXIS, 1, gainput::KeyH }, {}, {}, {} } },
	{ KEY_CHAR, GainputDeviceType::GAINPUT_KEYBOARD, 1, { { INPUT_X_AXIS, 1, gainput::KeyI }, {}, {}, {} } },
	{ KEY_CHAR, GainputDeviceType::GAINPUT_KEYBOARD, 1, { { INPUT_X_AXIS, 1, gainput::KeyJ }, {}, {}, {} } },
	{ KEY_CHAR, GainputDeviceType::GAINPUT_KEYBOARD, 1, { { INPUT_X_AXIS, 1, gainput::KeyK }, {}, {}, {} } },
	{ KEY_CHAR, GainputDeviceType::GAINPUT_KEYBOARD, 1, { { INPUT_X_AXIS, 1, gainput::KeyL }, {}, {}, {} } },
	{ KEY_CHAR, GainputDeviceType::GAINPUT_KEYBOARD, 1, { { INPUT_X_AXIS, 1, gainput::KeyM }, {}, {}, {} } },
	{ KEY_CHAR, GainputDeviceType::GAINPUT_KEYBOARD, 1, { { INPUT_X_AXIS, 1, gainput::KeyN }, {}, {}, {} } },
	{ KEY_CHAR, GainputDeviceType::GAINPUT_KEYBOARD, 1, { { INPUT_X_AXIS, 1, gainput::KeyO }, {}, {}, {} } },
	{ KEY_CHAR, GainputDeviceType::GAINPUT_KEYBOARD, 1, { { INPUT_X_AXIS, 1, gainput::KeyP }, {}, {}, {} } },
	{ KEY_CHAR, GainputDeviceType::GAINPUT_KEYBOARD, 1, { { INPUT_X_AXIS, 1, gainput::KeyQ }, {}, {}, {} } },
	{ KEY_CHAR, GainputDeviceType::GAINPUT_KEYBOARD, 1, { { INPUT_X_AXIS, 1, gainput::KeyR }, {}, {}, {} } },
	{ KEY_CHAR, GainputDeviceType::GAINPUT_KEYBOARD, 1, { { INPUT_X_AXIS, 1, gainput::KeyS }, {}, {}, {} } },
	{ KEY_CHAR, GainputDeviceType::GAINPUT_KEYBOARD, 1, { { INPUT_X_AXIS, 1, gainput::KeyT }, {}, {}, {} } },
	{ KEY_CHAR, GainputDeviceType::GAINPUT_KEYBOARD, 1, { { INPUT_X_AXIS, 1, gainput::KeyU }, {}, {}, {} } },
	{ KEY_CHAR, GainputDeviceType::GAINPUT_KEYBOARD, 1, { { INPUT_X_AXIS, 1, gainput::KeyV }, {}, {}, {} } },
	{ KEY_CHAR, GainputDeviceType::GAINPUT_KEYBOARD, 1, { { INPUT_X_AXIS, 1, gainput::KeyW }, {}, {}, {} } },
	{ KEY_CHAR, GainputDeviceType::GAINPUT_KEYBOARD, 1, { { INPUT_X_AXIS, 1, gainput::KeyX }, {}, {}, {} } },
	{ KEY_CHAR, GainputDeviceType::GAINPUT_KEYBOARD, 1, { { INPUT_X_AXIS, 1, gainput::KeyY }, {}, {}, {} } },
	{ KEY_CHAR, GainputDeviceType::GAINPUT_KEYBOARD, 1, { { INPUT_X_AXIS, 1, gainput::KeyZ }, {}, {}, {} } },
	{ KEY_CHAR, GainputDeviceType::GAINPUT_KEYBOARD, 1, { { INPUT_X_AXIS, 1, gainput::KeySpace }, {}, {}, {} } },
	{ KEY_CHAR, GainputDeviceType::GAINPUT_KEYBOARD, 1, { { INPUT_X_AXIS, 1, gainput::KeyComma }, {}, {}, {} } },
	{ KEY_CHAR, GainputDeviceType::GAINPUT_KEYBOARD, 1, { { INPUT_X_AXIS, 1, gainput::KeyPeriod }, {}, {}, {} } },
	{ KEY_CHAR, GainputDeviceType::GAINPUT_KEYBOARD, 1, { { INPUT_X_AXIS, 1, gainput::KeyApostrophe }, {}, {}, {} } },
	{ KEY_CHAR, GainputDeviceType::GAINPUT_KEYBOARD, 1, { { INPUT_X_AXIS, 1, gainput::KeySlash }, {}, {}, {} } },
	{ KEY_CHAR, GainputDeviceType::GAINPUT_KEYBOARD, 1, { { INPUT_X_AXIS, 1, gainput::KeyBackslash }, {}, {}, {} } },

	{ KEY_CHAR, GainputDeviceType::GAINPUT_KEYBOARD, 1, { { INPUT_X_AXIS, 1, gainput::KeyKpSubtract }, {}, {}, {} } },
	{ KEY_CHAR, GainputDeviceType::GAINPUT_KEYBOARD, 1, { { INPUT_X_AXIS, 1, gainput::KeyKpInsert }, {}, {}, {} } },
	{ KEY_CHAR, GainputDeviceType::GAINPUT_KEYBOARD, 1, { { INPUT_X_AXIS, 1, gainput::KeyKpEnd }, {}, {}, {} } },
	{ KEY_CHAR, GainputDeviceType::GAINPUT_KEYBOARD, 1, { { INPUT_X_AXIS, 1, gainput::KeyKpDown }, {}, {}, {} } },
	{ KEY_CHAR, GainputDeviceType::GAINPUT_KEYBOARD, 1, { { INPUT_X_AXIS, 1, gainput::KeyKpPageDown }, {}, {}, {} } },
	{ KEY_CHAR, GainputDeviceType::GAINPUT_KEYBOARD, 1, { { INPUT_X_AXIS, 1, gainput::KeyKpLeft }, {}, {}, {} } },
	{ KEY_CHAR, GainputDeviceType::GAINPUT_KEYBOARD, 1, { { INPUT_X_AXIS, 1, gainput::KeyKpBegin }, {}, {}, {} } },
	{ KEY_CHAR, GainputDeviceType::GAINPUT_KEYBOARD, 1, { { INPUT_X_AXIS, 1, gainput::KeyKpRight }, {}, {}, {} } },
	{ KEY_CHAR, GainputDeviceType::GAINPUT_KEYBOARD, 1, { { INPUT_X_AXIS, 1, gainput::KeyKpHome }, {}, {}, {} } },
	{ KEY_CHAR, GainputDeviceType::GAINPUT_KEYBOARD, 1, { { INPUT_X_AXIS, 1, gainput::KeyKpUp }, {}, {}, {} } },
	{ KEY_CHAR, GainputDeviceType::GAINPUT_KEYBOARD, 1, { { INPUT_X_AXIS, 1, gainput::KeyKpPageUp }, {}, {}, {} } },

	{ KEY_CHAR, GainputDeviceType::GAINPUT_KEYBOARD, 1, { { INPUT_X_AXIS, 1, gainput::KeyMinus }, {}, {}, {} } },
	{ KEY_CHAR, GainputDeviceType::GAINPUT_KEYBOARD, 1, { { INPUT_X_AXIS, 1, gainput::Key0 }, {}, {}, {} } },
	{ KEY_CHAR, GainputDeviceType::GAINPUT_KEYBOARD, 1, { { INPUT_X_AXIS, 1, gainput::Key1 }, {}, {}, {} } },
	{ KEY_CHAR, GainputDeviceType::GAINPUT_KEYBOARD, 1, { { INPUT_X_AXIS, 1, gainput::Key2 }, {}, {}, {} } },
	{ KEY_CHAR, GainputDeviceType::GAINPUT_KEYBOARD, 1, { { INPUT_X_AXIS, 1, gainput::Key3 }, {}, {}, {} } },
	{ KEY_CHAR, GainputDeviceType::GAINPUT_KEYBOARD, 1, { { INPUT_X_AXIS, 1, gainput::Key4 }, {}, {}, {} } },
	{ KEY_CHAR, GainputDeviceType::GAINPUT_KEYBOARD, 1, { { INPUT_X_AXIS, 1, gainput::Key5 }, {}, {}, {} } },
	{ KEY_CHAR, GainputDeviceType::GAINPUT_KEYBOARD, 1, { { INPUT_X_AXIS, 1, gainput::Key6 }, {}, {}, {} } },
	{ KEY_CHAR, GainputDeviceType::GAINPUT_KEYBOARD, 1, { { INPUT_X_AXIS, 1, gainput::Key7 }, {}, {}, {} } },
	{ KEY_CHAR, GainputDeviceType::GAINPUT_KEYBOARD, 1, { { INPUT_X_AXIS, 1, gainput::Key8 }, {}, {}, {} } },
	{ KEY_CHAR, GainputDeviceType::GAINPUT_KEYBOARD, 1, { { INPUT_X_AXIS, 1, gainput::Key9 }, {}, {}, {} } },
};

static GestureMappingDescription gGestureMappings[] = {
	{ GESTURE_SWIPE_2, gainput::GesturePan, { 2, 2 } },
};

//TODO: Add callbacks to mappings as Actions
//this will unify iOS as joysticks are virtual and can be mapped to any finger.
//TODO: Separate per device for simpler GetButtonData
static KeyMappingDescription gXboxMappings[] = {

	//Triggers
	//Keyboard
	{ KEY_LEFT_TRIGGER, GainputDeviceType::GAINPUT_GAMEPAD, 1, { { INPUT_X_AXIS, 1, gainput::PadButtonAxis4 }, {}, {}, {} } },
	{ KEY_RIGHT_TRIGGER, GainputDeviceType::GAINPUT_GAMEPAD, 1, { { INPUT_X_AXIS, 1, gainput::PadButtonAxis5 }, {}, {}, {} } },

	//Bumbers
	//Keyboard/Mouse
	{ KEY_LEFT_BUMPER, GainputDeviceType::GAINPUT_GAMEPAD, 1, { { INPUT_X_AXIS, 1, gainput::PadButtonL1 }, {}, {}, {} } },
	{ KEY_RIGHT_BUMPER, GainputDeviceType::GAINPUT_GAMEPAD, 1, { { INPUT_X_AXIS, 1, gainput::PadButtonR1 }, {}, {}, {} } },

	//STICKS
	//Keyboard/Mouse
	{ KEY_LEFT_STICK,
	  GainputDeviceType::GAINPUT_GAMEPAD,
	  2,
	  { { INPUT_X_AXIS, 1, gainput::PadButtonLeftStickX }, { INPUT_Y_AXIS, 1, gainput::PadButtonLeftStickY }, {}, {} } },
	{ KEY_RIGHT_STICK,
	  GainputDeviceType::GAINPUT_GAMEPAD,
	  2,
	  { { INPUT_X_AXIS, 1, gainput::PadButtonRightStickX }, { INPUT_Y_AXIS, -1, gainput::PadButtonRightStickY }, {}, {} } },

	//DPAD
	{ KEY_PAD_UP, GainputDeviceType::GAINPUT_GAMEPAD, 1, { { INPUT_Y_AXIS, 1, gainput::PadButtonUp }, {}, {}, {} } },
	{ KEY_PAD_DOWN, GainputDeviceType::GAINPUT_GAMEPAD, 1, { { INPUT_Y_AXIS, -1, gainput::PadButtonDown }, {}, {}, {} } },
	{ KEY_PAD_LEFT, GainputDeviceType::GAINPUT_GAMEPAD, 1, { { INPUT_X_AXIS, -1, gainput::PadButtonLeft }, {}, {}, {} } },
	{ KEY_PAD_RIGHT, GainputDeviceType::GAINPUT_GAMEPAD, 1, { { INPUT_X_AXIS, 1, gainput::PadButtonRight }, {}, {}, {} } },

	//this will map to x y buttons
	{ KEY_BUTTON_X, GainputDeviceType::GAINPUT_GAMEPAD, 1, { { INPUT_X_AXIS, 1, gainput::PadButtonX }, {}, {}, {} } },
	{ KEY_BUTTON_Y, GainputDeviceType::GAINPUT_GAMEPAD, 1, { { INPUT_X_AXIS, 1, gainput::PadButtonY }, {}, {}, {} } },

	////this will map to L3 R3
	{ KEY_LEFT_STICK_BUTTON, GainputDeviceType::GAINPUT_GAMEPAD, 1, { { INPUT_X_AXIS, 1, gainput::PadButtonL3 }, {}, {}, {} } },
	{ KEY_RIGHT_STICK_BUTTON, GainputDeviceType::GAINPUT_GAMEPAD, 1, { { INPUT_X_AXIS, 1, gainput::PadButtonR3 }, {}, {}, {} } },

	//CONFIRM
	//MOUSE
	{ KEY_CONFIRM, GainputDeviceType::GAINPUT_GAMEPAD, 1, { { INPUT_X_AXIS, 1, gainput::PadButtonA }, {}, {}, {} } },

	//CANCEL
	//Keyboard
	{ KEY_CANCEL, GainputDeviceType::GAINPUT_GAMEPAD, 1, { { INPUT_X_AXIS, 1, gainput::PadButtonB }, {}, {}, {} } },

	//MENU
	//Keyboard
	{ KEY_MENU, GainputDeviceType::GAINPUT_GAMEPAD, 1, { { INPUT_X_AXIS, 1, gainput::PadButtonStart }, {}, {}, {} } }
};

#endif
