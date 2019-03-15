#ifndef INPUT_MAPPING_H
#define INPUT_MAPPING_H
#pragma once
#include "InputSystem.h"

static float k_joystickRotationSpeed = 0.01f;
static float k_mouseRotationSpeed = 0.003f;

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

	// IMGUI navinput bindings
	IMGUI_NAVINPUT_ACTIVATE,
	IMGUI_NAVINPUT_CANCEL,
	IMGUI_NAVINPUT_MENU,
	IMGUI_NAVINPUT_INPUT,
	IMGUI_NAVINPUT_DPADLEFT,
	IMGUI_NAVINPUT_DPADRIGHT,
	IMGUI_NAVINPUT_DPADUP,
	IMGUI_NAVINPUT_DPADDOWN,
	IMGUI_NAVINPUT_FOCUSNEXT,
	IMGUI_NAVINPUT_FOCUSPREV,
	IMGUI_NAVINPUT_TWEAKFAST,
	IMGUI_NAVINPUT_TWEAKSLOW,

	// Camera bindings
	CAMERA_INPUT_MOVE,
	CAMERA_INPUT_ROTATE,

	VIRTUAL_JOYSTICK_TOUCH0,
	VIRTUAL_JOYSTICK_TOUCH1,
	
	// Misc unit test bindings
	KEY_BUTTON_X_TRIGGERED,
	KEY_BUTTON_Y_TRIGGERED,
	KEY_LEFT_TRIGGER_TRIGGERED,
	KEY_RIGHT_TRIGGER_TRIGGERED,
	KEY_LEFT_STICK_BUTTON_TRIGGERED,
	KEY_RIGHT_STICK_BUTTON_TRIGGERED,
	KEY_CONFIRM_TRIGGERED,
	KEY_CONFIRM_PRESSED,
	KEY_CANCEL_TRIGGERED,
	KEY_MENU_TRIGGERED,
	KEY_PAD_UP_PRESSED,
	KEY_PAD_DOWN_PRESSED,
	KEY_PAD_LEFT_PRESSED,
	KEY_PAD_RIGHT_PRESSED,
	KEY_LEFT_STICK_PRESSED,
	KEY_RIGHT_STICK_PRESSED,
	KEY_LEFT_BUMPER_PRESSED,
	KEY_RIGHT_BUMPER_PRESSED,
	KEY_LEFT_ALT_PRESSED,
	KEY_RIGHT_ALT_PRESSED,
	KEY_LEFT_CTRL_PRESSED,
	KEY_RIGHT_CTRL_PRESSED,
	KEY_LEFT_SHIFT_PRESSED,
	KEY_RIGHT_SHIFT_PRESSED,
	KEY_MOUSE_WHEEL_BUTTON_PRESSED,

	KEY_COUNT
};

//TODO: Add callbacks to mappings as Actions
//this will unify iOS as joysticks are virtual and can be mapped to any finger.
//TODO: Separate per device for simpler GetButtonData
static KeyMappingDescription gUserKeys[] = {
	// IMGUI mappings
	{ IMGUI_NAVINPUT_ACTIVATE, INPUT_X_AXIS, GainputDeviceType::GAINPUT_MOUSE, DEFINE_DEVICE_ACTION(gainput::MouseButton0, ACTION_PRESSED), 1.0f },
	{ IMGUI_NAVINPUT_ACTIVATE, INPUT_X_AXIS, GainputDeviceType::GAINPUT_TOUCH, DEFINE_DEVICE_ACTION(gainput::Touch0Down, ACTION_PRESSED), 1.0f },
	{ IMGUI_NAVINPUT_CANCEL, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, DEFINE_DEVICE_ACTION(gainput::KeyEscape, ACTION_PRESSED), 1.0f },
	{ IMGUI_NAVINPUT_MENU, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, DEFINE_DEVICE_ACTION(gainput::KeyQ, ACTION_PRESSED), 1.0f },
	{ IMGUI_NAVINPUT_INPUT, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, DEFINE_DEVICE_ACTION(gainput::KeyF, ACTION_PRESSED), 1.0f },
	{ IMGUI_NAVINPUT_DPADUP, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, DEFINE_DEVICE_ACTION(gainput::KeyUp, ACTION_PRESSED), 1.0f },
	{ IMGUI_NAVINPUT_DPADDOWN, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, DEFINE_DEVICE_ACTION(gainput::KeyDown, ACTION_PRESSED), 1.0f },
	{ IMGUI_NAVINPUT_DPADLEFT, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, DEFINE_DEVICE_ACTION(gainput::KeyLeft, ACTION_PRESSED), 1.0f },
	{ IMGUI_NAVINPUT_DPADRIGHT, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, DEFINE_DEVICE_ACTION(gainput::KeyRight, ACTION_PRESSED), 1.0f },
	{ IMGUI_NAVINPUT_FOCUSNEXT, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, DEFINE_DEVICE_ACTION(gainput::KeyShiftL, ACTION_PRESSED), 1.0f },
	{ IMGUI_NAVINPUT_FOCUSPREV, INPUT_X_AXIS, GainputDeviceType::GAINPUT_MOUSE, DEFINE_DEVICE_ACTION(gainput::MouseButton2, ACTION_PRESSED), 1.0f },
	{ IMGUI_NAVINPUT_TWEAKFAST, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, DEFINE_DEVICE_ACTION(gainput::KeyShiftL, ACTION_PRESSED), 1.0f },
	{ IMGUI_NAVINPUT_TWEAKSLOW, INPUT_X_AXIS, GainputDeviceType::GAINPUT_MOUSE, DEFINE_DEVICE_ACTION(gainput::MouseButton2, ACTION_PRESSED), 1.0f },

	// Camera bindings
	{ CAMERA_INPUT_MOVE, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, DEFINE_DEVICE_ACTION(gainput::KeyD, ACTION_PRESSED), 1.0f },
	{ CAMERA_INPUT_MOVE, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, DEFINE_DEVICE_ACTION(gainput::KeyA, ACTION_PRESSED), -1.0f },
	{ CAMERA_INPUT_MOVE, INPUT_Y_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, DEFINE_DEVICE_ACTION(gainput::KeyW, ACTION_PRESSED), 1.0f },
	{ CAMERA_INPUT_MOVE, INPUT_Y_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, DEFINE_DEVICE_ACTION(gainput::KeyS, ACTION_PRESSED), -1.0f },
	{ CAMERA_INPUT_MOVE, INPUT_X_AXIS, GainputDeviceType::GAINPUT_TOUCH, gainput::Touch0X, 1.0f },
	{ CAMERA_INPUT_MOVE, INPUT_Y_AXIS, GainputDeviceType::GAINPUT_TOUCH, gainput::Touch0Y, 1.0f },
	{ CAMERA_INPUT_MOVE, INPUT_X_AXIS, GainputDeviceType::GAINPUT_TOUCH, gainput::Touch1X, 1.0f },
	{ CAMERA_INPUT_MOVE, INPUT_Y_AXIS, GainputDeviceType::GAINPUT_TOUCH, gainput::Touch1Y, 1.0f },

	{ CAMERA_INPUT_ROTATE, INPUT_Y_AXIS, GainputDeviceType::GAINPUT_RAW_MOUSE, DEFINE_DEVICE_ACTION(gainput::MouseAxisX, ACTION_DELTA_VALUE), k_mouseRotationSpeed },
	{ CAMERA_INPUT_ROTATE, INPUT_X_AXIS, GainputDeviceType::GAINPUT_RAW_MOUSE, DEFINE_DEVICE_ACTION(gainput::MouseAxisY, ACTION_DELTA_VALUE), k_mouseRotationSpeed },
	{ CAMERA_INPUT_ROTATE, INPUT_X_AXIS, GainputDeviceType::GAINPUT_TOUCH, gainput::Touch1X, 1.0f },
	{ CAMERA_INPUT_ROTATE, INPUT_Y_AXIS, GainputDeviceType::GAINPUT_TOUCH, gainput::Touch1Y, 1.0f },
	{ CAMERA_INPUT_ROTATE, INPUT_X_AXIS, GainputDeviceType::GAINPUT_TOUCH, gainput::Touch0X, 1.0f },
	{ CAMERA_INPUT_ROTATE, INPUT_Y_AXIS, GainputDeviceType::GAINPUT_TOUCH, gainput::Touch0Y, 1.0f },

	// Misc unit tests bindings
	{ KEY_BUTTON_X_TRIGGERED, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, DEFINE_DEVICE_ACTION(gainput::KeyQ, ACTION_TRIGGERED), 1.0f },
	{ KEY_BUTTON_Y_TRIGGERED, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, DEFINE_DEVICE_ACTION(gainput::KeyF, ACTION_TRIGGERED), 1.0f },
	{ KEY_LEFT_TRIGGER_TRIGGERED, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, DEFINE_DEVICE_ACTION(gainput::KeySpace, ACTION_TRIGGERED), 1.0f },
	{ KEY_RIGHT_TRIGGER_TRIGGERED, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, DEFINE_DEVICE_ACTION(gainput::KeyE, ACTION_TRIGGERED), 1.0f },
	{ KEY_LEFT_STICK_BUTTON_TRIGGERED, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, DEFINE_DEVICE_ACTION(gainput::KeyF1, ACTION_TRIGGERED), 1.0f },
	{ KEY_RIGHT_STICK_BUTTON_TRIGGERED, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, DEFINE_DEVICE_ACTION(gainput::KeyBackSpace, ACTION_TRIGGERED), 1.0f },
	{ KEY_CONFIRM_TRIGGERED, INPUT_X_AXIS, GainputDeviceType::GAINPUT_MOUSE, DEFINE_DEVICE_ACTION(gainput::MouseButton0, ACTION_TRIGGERED), 1.0f },
	{ KEY_CONFIRM_TRIGGERED, INPUT_X_AXIS, GainputDeviceType::GAINPUT_TOUCH, DEFINE_DEVICE_ACTION(gainput::Touch0Down, ACTION_TRIGGERED), 1.0f },
	{ KEY_CANCEL_TRIGGERED, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, DEFINE_DEVICE_ACTION(gainput::KeyEscape, ACTION_TRIGGERED), 1.0f },
	{ KEY_MENU_TRIGGERED, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, DEFINE_DEVICE_ACTION(gainput::KeyReturn, ACTION_TRIGGERED), 1.0f },
	{ KEY_CONFIRM_PRESSED, INPUT_X_AXIS, GainputDeviceType::GAINPUT_MOUSE, DEFINE_DEVICE_ACTION(gainput::MouseButton0, ACTION_PRESSED), 1.0f },
	{ KEY_CONFIRM_PRESSED, INPUT_X_AXIS, GainputDeviceType::GAINPUT_TOUCH, DEFINE_DEVICE_ACTION(gainput::Touch0Down, ACTION_PRESSED), 1.0f },
	{ KEY_PAD_UP_PRESSED, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, DEFINE_DEVICE_ACTION(gainput::KeyUp, ACTION_PRESSED), 1.0f },
	{ KEY_PAD_DOWN_PRESSED, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, DEFINE_DEVICE_ACTION(gainput::KeyDown, ACTION_PRESSED), -1.0f },
	{ KEY_PAD_LEFT_PRESSED, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, DEFINE_DEVICE_ACTION(gainput::KeyLeft, ACTION_PRESSED), -1.0f },
	{ KEY_PAD_RIGHT_PRESSED, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, DEFINE_DEVICE_ACTION(gainput::KeyRight, ACTION_PRESSED), 1.0f },
	{ KEY_LEFT_SHIFT_PRESSED, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, DEFINE_DEVICE_ACTION(gainput::KeyShiftL, ACTION_PRESSED), 1.0f },
	{ KEY_RIGHT_SHIFT_PRESSED, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, DEFINE_DEVICE_ACTION(gainput::KeyShiftR, ACTION_PRESSED), 1.0f },
	{ KEY_RIGHT_CTRL_PRESSED, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, DEFINE_DEVICE_ACTION(gainput::KeyCtrlR, ACTION_PRESSED), 1.0f },
	{ KEY_LEFT_CTRL_PRESSED, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, DEFINE_DEVICE_ACTION(gainput::KeyCtrlL, ACTION_PRESSED), 1.0f },
	{ KEY_RIGHT_ALT_PRESSED, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, DEFINE_DEVICE_ACTION(gainput::KeyAltR, ACTION_PRESSED), 1.0f },
	{ KEY_LEFT_ALT_PRESSED, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, DEFINE_DEVICE_ACTION(gainput::KeyAltL, ACTION_PRESSED), 1.0f },
	{ KEY_LEFT_STICK_PRESSED, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, DEFINE_DEVICE_ACTION(gainput::KeyD, ACTION_PRESSED), 1.0f },
	{ KEY_LEFT_STICK_PRESSED, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, DEFINE_DEVICE_ACTION(gainput::KeyA, ACTION_PRESSED), -1.0f },
	{ KEY_LEFT_STICK_PRESSED, INPUT_Y_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, DEFINE_DEVICE_ACTION(gainput::KeyW, ACTION_PRESSED), 1.0f },
	{ KEY_LEFT_STICK_PRESSED, INPUT_Y_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, DEFINE_DEVICE_ACTION(gainput::KeyS, ACTION_PRESSED), -1.0f },
	{ KEY_LEFT_BUMPER_PRESSED, INPUT_X_AXIS, GainputDeviceType::GAINPUT_MOUSE, DEFINE_DEVICE_ACTION(gainput::KeyShiftL, ACTION_PRESSED), 1.0f },
	{ KEY_RIGHT_BUMPER_PRESSED, INPUT_X_AXIS, GainputDeviceType::GAINPUT_MOUSE, DEFINE_DEVICE_ACTION(gainput::MouseButton2, ACTION_PRESSED), 1.0f },
	{ KEY_MOUSE_WHEEL_BUTTON_PRESSED, INPUT_X_AXIS, GainputDeviceType::GAINPUT_MOUSE, DEFINE_DEVICE_ACTION(gainput::MouseButtonMiddle, ACTION_PRESSED), 1.0f },

	//Triggers
	//Keyboard
	{ KEY_LEFT_TRIGGER, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, gainput::KeySpace, 1.0f },
	{ KEY_RIGHT_TRIGGER, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, gainput::KeyE, 1.0f },

	//Bumbers
	//Keyboard/Mouse
	{ KEY_LEFT_BUMPER, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, gainput::KeyShiftL, 1.0f },
	{ KEY_RIGHT_BUMPER, INPUT_X_AXIS, GainputDeviceType::GAINPUT_MOUSE, gainput::MouseButton2, 1.0f },

	//STICKS
	//Keyboard/Mouse
	{ KEY_LEFT_STICK, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, gainput::KeyD, 1.0f },
	{ KEY_LEFT_STICK, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, gainput::KeyA, -1.0f },
	{ KEY_LEFT_STICK, INPUT_Y_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, gainput::KeyW, 1.0f },
	{ KEY_LEFT_STICK, INPUT_Y_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, gainput::KeyS, -1.0f },

	{ KEY_UI_MOVE, INPUT_X_AXIS, GainputDeviceType::GAINPUT_MOUSE, gainput::MouseAxisX, 1.0f },
	{ KEY_UI_MOVE, INPUT_Y_AXIS, GainputDeviceType::GAINPUT_MOUSE, gainput::MouseAxisY, 1.0f },
	{ KEY_RIGHT_STICK, INPUT_X_AXIS, GainputDeviceType::GAINPUT_RAW_MOUSE, gainput::MouseAxisX, 1.0f },
	{ KEY_RIGHT_STICK, INPUT_Y_AXIS, GainputDeviceType::GAINPUT_RAW_MOUSE, gainput::MouseAxisY, 1.0f },

	//TOUCH
	{ KEY_UI_MOVE, INPUT_X_AXIS, GainputDeviceType::GAINPUT_TOUCH, gainput::Touch0X, 1.0f },
	{ KEY_UI_MOVE, INPUT_Y_AXIS, GainputDeviceType::GAINPUT_TOUCH, gainput::Touch0Y, 1.0f },

	{ VIRTUAL_JOYSTICK_TOUCH0, INPUT_X_AXIS, GainputDeviceType::GAINPUT_TOUCH, gainput::Touch0X, 1.0f },
	{ VIRTUAL_JOYSTICK_TOUCH0, INPUT_Y_AXIS, GainputDeviceType::GAINPUT_TOUCH, gainput::Touch0Y, 1.0f },
	{ VIRTUAL_JOYSTICK_TOUCH1, INPUT_X_AXIS, GainputDeviceType::GAINPUT_TOUCH, gainput::Touch1X, 1.0f },
	{ VIRTUAL_JOYSTICK_TOUCH1, INPUT_Y_AXIS, GainputDeviceType::GAINPUT_TOUCH, gainput::Touch1Y, 1.0f },

	//PAD
	//KEYBOARD
	{ KEY_PAD_UP, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, gainput::KeyUp, 1.0f },
	{ KEY_PAD_DOWN, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, gainput::KeyDown, -1.0f },
	{ KEY_PAD_LEFT, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, gainput::KeyLeft, -1.0f },
	{ KEY_PAD_RIGHT, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, gainput::KeyRight, 1.0f },

	//this will map to x y buttons
	{ KEY_BUTTON_X, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, gainput::KeyQ, 1.0f },
	{ KEY_BUTTON_Y, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, gainput::KeyF, 1.0f },

	//this will map to L3 R3
	{ KEY_LEFT_STICK_BUTTON, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, gainput::KeyF1, 1.0f },
	{ KEY_RIGHT_STICK_BUTTON, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, gainput::KeyBackSpace, 1.0f },

	//CONFIRM
	//MOUSE
	{ KEY_CONFIRM, INPUT_X_AXIS, GainputDeviceType::GAINPUT_MOUSE, gainput::MouseButton0, 1.0f },
	{ KEY_CONFIRM, INPUT_X_AXIS, GainputDeviceType::GAINPUT_TOUCH, gainput::Touch0Down, 1.0f },

	//CANCEL
	//Keyboard
	{ KEY_CANCEL, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, gainput::KeyEscape, 1.0f },

	//MENU
	//Keyboard
	{ KEY_MENU, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, gainput::KeyReturn, 1.0f },

	//Mouse Wheel
	{ KEY_MOUSE_WHEEL, INPUT_X_AXIS, GainputDeviceType::GAINPUT_MOUSE, gainput::MouseButtonWheelUp, 1.0f },
	{ KEY_MOUSE_WHEEL, INPUT_X_AXIS, GainputDeviceType::GAINPUT_MOUSE, gainput::MouseButtonWheelDown, -1.0f },
	{ KEY_MOUSE_WHEEL_BUTTON, INPUT_X_AXIS, GainputDeviceType::GAINPUT_MOUSE, gainput::MouseButtonMiddle, 1.0f },

	{ KEY_LEFT_SHIFT, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, gainput::KeyShiftL, 1.0f },
	{ KEY_RIGHT_SHIFT, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, gainput::KeyShiftR, 1.0f },
	{ KEY_RIGHT_CTRL, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, gainput::KeyCtrlR, 1.0f },
	{ KEY_LEFT_CTRL, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, gainput::KeyCtrlL, 1.0f },
	{ KEY_RIGHT_ALT, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, gainput::KeyAltR, 1.0f },
	{ KEY_LEFT_ALT, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, gainput::KeyAltL, 1.0f },
	{ KEY_RIGHT_SUPER, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, gainput::KeySuperR, 1.0f },
	{ KEY_LEFT_SUPER, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, gainput::KeySuperL, 1.0f },
	{ KEY_DELETE, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, gainput::KeyDelete, 1.0f },

	//Keyboard chars
	{ KEY_CHAR, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, gainput::KeyA, 1.0f },
	{ KEY_CHAR, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, gainput::KeyB, 1.0f },
	{ KEY_CHAR, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, gainput::KeyC, 1.0f },
	{ KEY_CHAR, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, gainput::KeyD, 1.0f },
	{ KEY_CHAR, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, gainput::KeyE, 1.0f },
	{ KEY_CHAR, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, gainput::KeyF, 1.0f },
	{ KEY_CHAR, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, gainput::KeyG, 1.0f },
	{ KEY_CHAR, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, gainput::KeyH, 1.0f },
	{ KEY_CHAR, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, gainput::KeyI, 1.0f },
	{ KEY_CHAR, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, gainput::KeyJ, 1.0f },
	{ KEY_CHAR, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, gainput::KeyK, 1.0f },
	{ KEY_CHAR, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, gainput::KeyL, 1.0f },
	{ KEY_CHAR, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, gainput::KeyM, 1.0f },
	{ KEY_CHAR, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, gainput::KeyN, 1.0f },
	{ KEY_CHAR, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, gainput::KeyO, 1.0f },
	{ KEY_CHAR, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, gainput::KeyP, 1.0f },
	{ KEY_CHAR, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, gainput::KeyQ, 1.0f },
	{ KEY_CHAR, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, gainput::KeyR, 1.0f },
	{ KEY_CHAR, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, gainput::KeyS, 1.0f },
	{ KEY_CHAR, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, gainput::KeyT, 1.0f },
	{ KEY_CHAR, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, gainput::KeyU, 1.0f },
	{ KEY_CHAR, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, gainput::KeyV, 1.0f },
	{ KEY_CHAR, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, gainput::KeyW, 1.0f },
	{ KEY_CHAR, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, gainput::KeyX, 1.0f },
	{ KEY_CHAR, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, gainput::KeyY, 1.0f },
	{ KEY_CHAR, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, gainput::KeyZ, 1.0f },
	{ KEY_CHAR, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, gainput::KeySpace, 1.0f },
	{ KEY_CHAR, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, gainput::KeyComma, 1.0f },
	{ KEY_CHAR, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, gainput::KeyPeriod, 1.0f },
	{ KEY_CHAR, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, gainput::KeyApostrophe, 1.0f },
	{ KEY_CHAR, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, gainput::KeySlash, 1.0f },
	{ KEY_CHAR, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, gainput::KeyBackslash, 1.0f },

	{ KEY_CHAR, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, gainput::KeyKpSubtract, 1.0f },
	{ KEY_CHAR, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, gainput::KeyKpInsert, 1.0f },
	{ KEY_CHAR, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, gainput::KeyKpEnd, 1.0f },
	{ KEY_CHAR, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, gainput::KeyKpDown, 1.0f },
	{ KEY_CHAR, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, gainput::KeyKpPageDown, 1.0f },
	{ KEY_CHAR, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, gainput::KeyKpBegin, 1.0f },
	{ KEY_CHAR, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, gainput::KeyKpLeft, 1.0f },
	{ KEY_CHAR, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, gainput::KeyKpRight, 1.0f },
	{ KEY_CHAR, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, gainput::KeyKpHome, 1.0f },
	{ KEY_CHAR, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, gainput::KeyKpUp, 1.0f },
	{ KEY_CHAR, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, gainput::KeyKpPageUp, 1.0f },

	{ KEY_CHAR, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, gainput::KeyMinus, 1.0f },
	{ KEY_CHAR, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, gainput::Key0, 1.0f },
	{ KEY_CHAR, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, gainput::Key1, 1.0f },
	{ KEY_CHAR, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, gainput::Key2, 1.0f },
	{ KEY_CHAR, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, gainput::Key3, 1.0f },
	{ KEY_CHAR, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, gainput::Key4, 1.0f },
	{ KEY_CHAR, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, gainput::Key5, 1.0f },
	{ KEY_CHAR, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, gainput::Key6, 1.0f },
	{ KEY_CHAR, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, gainput::Key7, 1.0f },
	{ KEY_CHAR, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, gainput::Key8, 1.0f },
	{ KEY_CHAR, INPUT_X_AXIS, GainputDeviceType::GAINPUT_KEYBOARD, gainput::Key9, 1.0f },
};

static GestureMappingDescription gGestureMappings[] = {
	{ GESTURE_SWIPE_2, gainput::GesturePan, { 2, 2 } },
};

//TODO: Add callbacks to mappings as Actions
//this will unify iOS as joysticks are virtual and can be mapped to any finger.
//TODO: Separate per device for simpler GetButtonData
static KeyMappingDescription gXboxMappings[] = {
	// IMGUI mappings
	{ IMGUI_NAVINPUT_ACTIVATE, INPUT_X_AXIS, GainputDeviceType::GAINPUT_GAMEPAD, DEFINE_DEVICE_ACTION(gainput::PadButtonA, ACTION_PRESSED), 1.0f },
	{ IMGUI_NAVINPUT_CANCEL, INPUT_X_AXIS, GainputDeviceType::GAINPUT_GAMEPAD, DEFINE_DEVICE_ACTION(gainput::PadButtonB, ACTION_PRESSED), 1.0f },
	{ IMGUI_NAVINPUT_MENU, INPUT_X_AXIS, GainputDeviceType::GAINPUT_GAMEPAD, DEFINE_DEVICE_ACTION(gainput::PadButtonX, ACTION_PRESSED), 1.0f },
	{ IMGUI_NAVINPUT_INPUT, INPUT_X_AXIS, GainputDeviceType::GAINPUT_GAMEPAD, DEFINE_DEVICE_ACTION(gainput::PadButtonY, ACTION_PRESSED), 1.0f },
	{ IMGUI_NAVINPUT_DPADUP, INPUT_X_AXIS, GainputDeviceType::GAINPUT_GAMEPAD, DEFINE_DEVICE_ACTION(gainput::PadButtonUp, ACTION_PRESSED), 1.0f },
	{ IMGUI_NAVINPUT_DPADDOWN, INPUT_X_AXIS, GainputDeviceType::GAINPUT_GAMEPAD, DEFINE_DEVICE_ACTION(gainput::PadButtonDown, ACTION_PRESSED), 1.0f },
	{ IMGUI_NAVINPUT_DPADLEFT, INPUT_X_AXIS, GainputDeviceType::GAINPUT_GAMEPAD, DEFINE_DEVICE_ACTION(gainput::PadButtonLeft, ACTION_PRESSED), 1.0f },
	{ IMGUI_NAVINPUT_DPADRIGHT, INPUT_X_AXIS, GainputDeviceType::GAINPUT_GAMEPAD, DEFINE_DEVICE_ACTION(gainput::PadButtonRight, ACTION_PRESSED), 1.0f },
	{ IMGUI_NAVINPUT_FOCUSNEXT, INPUT_X_AXIS, GainputDeviceType::GAINPUT_GAMEPAD, DEFINE_DEVICE_ACTION(gainput::PadButtonL1, ACTION_PRESSED), 1.0f },
	{ IMGUI_NAVINPUT_FOCUSPREV, INPUT_X_AXIS, GainputDeviceType::GAINPUT_GAMEPAD, DEFINE_DEVICE_ACTION(gainput::PadButtonR1, ACTION_PRESSED), 1.0f },
	{ IMGUI_NAVINPUT_TWEAKFAST, INPUT_X_AXIS, GainputDeviceType::GAINPUT_GAMEPAD, DEFINE_DEVICE_ACTION(gainput::PadButtonL1, ACTION_PRESSED), 1.0f },
	{ IMGUI_NAVINPUT_TWEAKSLOW, INPUT_X_AXIS, GainputDeviceType::GAINPUT_GAMEPAD, DEFINE_DEVICE_ACTION(gainput::PadButtonR1, ACTION_PRESSED), 1.0f },

	// Camera bindings
	{ CAMERA_INPUT_MOVE, INPUT_X_AXIS, GainputDeviceType::GAINPUT_GAMEPAD, gainput::PadButtonLeftStickX, 1.0f },
	{ CAMERA_INPUT_MOVE, INPUT_Y_AXIS, GainputDeviceType::GAINPUT_GAMEPAD, gainput::PadButtonLeftStickY, 1.0f },
	{ CAMERA_INPUT_ROTATE, INPUT_Y_AXIS, GainputDeviceType::GAINPUT_GAMEPAD, gainput::PadButtonRightStickX, k_joystickRotationSpeed },
	{ CAMERA_INPUT_ROTATE, INPUT_X_AXIS, GainputDeviceType::GAINPUT_GAMEPAD, gainput::PadButtonRightStickY, -k_joystickRotationSpeed},

	// Misc unit tests bindings
	{ KEY_BUTTON_X_TRIGGERED, INPUT_X_AXIS, GainputDeviceType::GAINPUT_GAMEPAD, DEFINE_DEVICE_ACTION(gainput::PadButtonX, ACTION_TRIGGERED), 1.0f },
	{ KEY_BUTTON_Y_TRIGGERED, INPUT_X_AXIS, GainputDeviceType::GAINPUT_GAMEPAD, DEFINE_DEVICE_ACTION(gainput::PadButtonY, ACTION_TRIGGERED), 1.0f },
	{ KEY_LEFT_TRIGGER_TRIGGERED, INPUT_X_AXIS, GainputDeviceType::GAINPUT_GAMEPAD, DEFINE_DEVICE_ACTION(gainput::PadButtonAxis4, ACTION_TRIGGERED), 1.0f },
	{ KEY_RIGHT_TRIGGER_TRIGGERED, INPUT_X_AXIS, GainputDeviceType::GAINPUT_GAMEPAD, DEFINE_DEVICE_ACTION(gainput::PadButtonAxis5, ACTION_TRIGGERED), 1.0f },
	{ KEY_LEFT_STICK_BUTTON_TRIGGERED, INPUT_X_AXIS, GainputDeviceType::GAINPUT_GAMEPAD, DEFINE_DEVICE_ACTION(gainput::PadButtonL3, ACTION_TRIGGERED), 1.0f },
	{ KEY_RIGHT_STICK_BUTTON_TRIGGERED, INPUT_X_AXIS, GainputDeviceType::GAINPUT_GAMEPAD, DEFINE_DEVICE_ACTION(gainput::PadButtonR3, ACTION_TRIGGERED), 1.0f },
	{ KEY_CONFIRM_TRIGGERED, INPUT_X_AXIS, GainputDeviceType::GAINPUT_GAMEPAD, DEFINE_DEVICE_ACTION(gainput::PadButtonA, ACTION_TRIGGERED), 1.0f },
	{ KEY_CONFIRM_PRESSED, INPUT_X_AXIS, GainputDeviceType::GAINPUT_GAMEPAD, DEFINE_DEVICE_ACTION(gainput::PadButtonA, ACTION_PRESSED), 1.0f },
	{ KEY_CANCEL_TRIGGERED, INPUT_X_AXIS, GainputDeviceType::GAINPUT_GAMEPAD, DEFINE_DEVICE_ACTION(gainput::PadButtonB, ACTION_TRIGGERED), 1.0f },
	{ KEY_MENU_TRIGGERED, INPUT_X_AXIS, GainputDeviceType::GAINPUT_GAMEPAD, DEFINE_DEVICE_ACTION(gainput::PadButtonStart, ACTION_TRIGGERED), 1.0f },
	{ KEY_PAD_UP_PRESSED, INPUT_Y_AXIS, GainputDeviceType::GAINPUT_GAMEPAD, DEFINE_DEVICE_ACTION(gainput::PadButtonUp, ACTION_PRESSED), 1.0f },
	{ KEY_PAD_DOWN_PRESSED, INPUT_Y_AXIS, GainputDeviceType::GAINPUT_GAMEPAD, DEFINE_DEVICE_ACTION(gainput::PadButtonDown, ACTION_PRESSED), -1.0f },
	{ KEY_PAD_LEFT_PRESSED, INPUT_X_AXIS, GainputDeviceType::GAINPUT_GAMEPAD, DEFINE_DEVICE_ACTION(gainput::PadButtonLeft, ACTION_PRESSED), -1.0f },
	{ KEY_PAD_RIGHT_PRESSED, INPUT_X_AXIS, GainputDeviceType::GAINPUT_GAMEPAD, DEFINE_DEVICE_ACTION(gainput::PadButtonRight, ACTION_PRESSED), 1.0f },
	{ KEY_LEFT_BUMPER_PRESSED, INPUT_X_AXIS, GainputDeviceType::GAINPUT_GAMEPAD, DEFINE_DEVICE_ACTION(gainput::PadButtonL1, ACTION_PRESSED), 1.0f },
	{ KEY_RIGHT_BUMPER_PRESSED, INPUT_X_AXIS, GainputDeviceType::GAINPUT_GAMEPAD, DEFINE_DEVICE_ACTION(gainput::PadButtonR1, ACTION_PRESSED), 1.0f },
	{ KEY_LEFT_STICK_PRESSED, INPUT_X_AXIS, GainputDeviceType::GAINPUT_GAMEPAD, gainput::PadButtonLeftStickX, 1.0f },
	{ KEY_LEFT_STICK_PRESSED, INPUT_Y_AXIS, GainputDeviceType::GAINPUT_GAMEPAD, gainput::PadButtonLeftStickY, -1.0f },


	//Triggers
	//Keyboard
	{ KEY_LEFT_TRIGGER, INPUT_X_AXIS, GainputDeviceType::GAINPUT_GAMEPAD, gainput::PadButtonAxis4, 1.0f },
	{ KEY_RIGHT_TRIGGER, INPUT_X_AXIS, GainputDeviceType::GAINPUT_GAMEPAD, gainput::PadButtonAxis5, 1.0f },

	//Bumbers
	//Keyboard/Mouse
	{ KEY_LEFT_BUMPER, INPUT_X_AXIS, GainputDeviceType::GAINPUT_GAMEPAD, gainput::PadButtonL1, 1.0f },
	{ KEY_RIGHT_BUMPER, INPUT_X_AXIS, GainputDeviceType::GAINPUT_GAMEPAD, gainput::PadButtonR1, 1.0f },

	//STICKS
	//Keyboard/Mouse
	{ KEY_LEFT_STICK, INPUT_X_AXIS, GainputDeviceType::GAINPUT_GAMEPAD, gainput::PadButtonLeftStickX, 1.0f },
	{ KEY_LEFT_STICK, INPUT_Y_AXIS, GainputDeviceType::GAINPUT_GAMEPAD, gainput::PadButtonLeftStickY, -1.0f },
	{ KEY_RIGHT_STICK, INPUT_X_AXIS, GainputDeviceType::GAINPUT_GAMEPAD, gainput::PadButtonRightStickX, 1.0f },
	{ KEY_RIGHT_STICK, INPUT_Y_AXIS, GainputDeviceType::GAINPUT_GAMEPAD, gainput::PadButtonRightStickY, -1.0f },

	//DPAD
	{ KEY_PAD_UP, INPUT_Y_AXIS, GainputDeviceType::GAINPUT_GAMEPAD, gainput::PadButtonUp, 1.0f },
	{ KEY_PAD_DOWN, INPUT_Y_AXIS, GainputDeviceType::GAINPUT_GAMEPAD, gainput::PadButtonDown, -1.0f },
	{ KEY_PAD_LEFT, INPUT_X_AXIS, GainputDeviceType::GAINPUT_GAMEPAD, gainput::PadButtonLeft, -1.0f },
	{ KEY_PAD_RIGHT, INPUT_X_AXIS, GainputDeviceType::GAINPUT_GAMEPAD, gainput::PadButtonRight, 1.0f },

	//this will map to x y buttons
	{ KEY_BUTTON_X, INPUT_X_AXIS, GainputDeviceType::GAINPUT_GAMEPAD, gainput::PadButtonX, 1.0f },
	{ KEY_BUTTON_Y, INPUT_X_AXIS, GainputDeviceType::GAINPUT_GAMEPAD, gainput::PadButtonY, 1.0f },

	////this will map to L3 R3
	{ KEY_LEFT_STICK_BUTTON, INPUT_X_AXIS, GainputDeviceType::GAINPUT_GAMEPAD, gainput::PadButtonL3, 1.0f },
	{ KEY_RIGHT_STICK_BUTTON, INPUT_X_AXIS, GainputDeviceType::GAINPUT_GAMEPAD, gainput::PadButtonR3, 1.0f },

	//CONFIRM
	//MOUSE
	{ KEY_CONFIRM, INPUT_X_AXIS, GainputDeviceType::GAINPUT_GAMEPAD, gainput::PadButtonA, 1.0f },

	//CANCEL
	//Keyboard
	{ KEY_CANCEL, INPUT_X_AXIS, GainputDeviceType::GAINPUT_GAMEPAD, gainput::PadButtonB, 1.0f },

	//MENU
	//Keyboard
	{ KEY_MENU, INPUT_X_AXIS, GainputDeviceType::GAINPUT_GAMEPAD, gainput::PadButtonStart, 1.0f },
};

#endif
