#ifndef INPUT_MAPPING_H
#define INPUT_MAPPING_H
#pragma once


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
	//Maps to Q on keyobard
	KEY_BUTTON_X,
	//Maps to Y on controller
	//Maps to F on keyboard
	KEY_BUTTON_Y,

	//Maps to Mouse on Windows
	//Maps to DPAD on XBox
	KEY_UI_MOVE,

	//Maps to Menu button on controller
	//Maps to Return on keyboard
	KEY_MENU,

	KEY_MAIN_CONTROLS,

	KEY_NUMBER_0,
	KEY_NUMBER_1,
	KEY_NUMBER_2,
	KEY_NUMBER_3,
	KEY_NUMBER_4,
	KEY_NUMBER_5,
	KEY_NUMBER_6,
	KEY_NUMBER_7,
	KEY_NUMBER_8,
	KEY_NUMBER_9,
	KEY_NUMBERS,

	//Virtual keyboads or actual keyboards will need these.
	KEY_CHAR_A,
	KEY_CHAR_B,
	KEY_CHAR_C,
	KEY_CHAR_D,
	KEY_CHAR_E,
	KEY_CHAR_F,
	KEY_CHAR_G,
	KEY_CHAR_H,
	KEY_CHAR_I,
	KEY_CHAR_J,
	KEY_CHAR_K,
	KEY_CHAR_L,
	KEY_CHAR_M,
	KEY_CHAR_N,
	KEY_CHAR_O,
	KEY_CHAR_P,
	KEY_CHAR_Q,
	KEY_CHAR_R,
	KEY_CHAR_S,
	KEY_CHAR_T,
	KEY_CHAR_U,
	KEY_CHAR_V,
	KEY_CHAR_W,
	KEY_CHAR_X,
	KEY_CHAR_Y,
	KEY_CHAR_Z,
	KEY_LETTERS
};

//TODO: Add callbacks to mappings as Actions
//this will unify iOS as joysticks are virtual and can be mapped to any finger.
//TODO: Separate per device for simpler GetButtonData
static KeyMappingDescription gUserKeys[] = {

	//Triggers
	//Keyboard
	{ KEY_LEFT_TRIGGER, GainputDeviceType::GAINPUT_KEYBOARD, 1,{ { INPUT_X_AXIS , 1, gainput::KeySpace },{ },{ },{ } } },
	{ KEY_RIGHT_TRIGGER,  GainputDeviceType::GAINPUT_KEYBOARD, 1,{ { INPUT_X_AXIS  , 1, gainput::KeyE },{},{},{} }  },

	//Bumbers
	//Keyboard/Mouse
	{ KEY_LEFT_BUMPER, GainputDeviceType::GAINPUT_KEYBOARD, 1,{ { INPUT_X_AXIS , 1, gainput::KeyShiftL },{},{},{} }  },
	{ KEY_RIGHT_BUMPER,  GainputDeviceType::GAINPUT_MOUSE, 1,{ { INPUT_X_AXIS  , 1, gainput::MouseButton1 },{},{},{} } },
	
	//STICKS
	//Keyboard/Mouse
	{ KEY_LEFT_STICK, GainputDeviceType::GAINPUT_KEYBOARD, 4,{ { INPUT_X_AXIS , 1, gainput::KeyD },{ INPUT_X_AXIS , -1, gainput::KeyA },{ INPUT_Y_AXIS , 1, gainput::KeyW },{ INPUT_Y_AXIS , -1, gainput::KeyS } } },
	
	{ KEY_UI_MOVE,  GainputDeviceType::GAINPUT_MOUSE, 2,{ { INPUT_X_AXIS , 1, gainput::MouseAxisX },{ INPUT_Y_AXIS , 1, gainput::MouseAxisY },{},{} } },
	{ KEY_RIGHT_STICK,  GainputDeviceType::GAINPUT_RAW_MOUSE, 2,{ { INPUT_X_AXIS , 1, gainput::MouseAxisX },{ INPUT_Y_AXIS , 1, gainput::MouseAxisY },{},{} }},
	
	//TOUCH
	{ KEY_RIGHT_STICK,  GainputDeviceType::GAINPUT_TOUCH, 2,{ { INPUT_X_AXIS , 1, gainput::Touch1X },{ INPUT_Y_AXIS , 1, gainput::Touch1Y },{},{} } },
	{ KEY_RIGHT_STICK,  GainputDeviceType::GAINPUT_TOUCH, 2,{ { INPUT_X_AXIS , 1, gainput::Touch0X },{ INPUT_Y_AXIS , 1, gainput::Touch0Y },{},{} } },
	
	{ KEY_LEFT_STICK,  GainputDeviceType::GAINPUT_TOUCH, 2,{ { INPUT_X_AXIS , 1, gainput::Touch0X },{ INPUT_Y_AXIS , 1, gainput::Touch0Y },{},{} } },
	{ KEY_LEFT_STICK,  GainputDeviceType::GAINPUT_TOUCH, 2,{ { INPUT_X_AXIS , 1, gainput::Touch1X },{ INPUT_Y_AXIS , 1, gainput::Touch1Y },{},{} } },


	//PAD
	//KEYBOARD
	{ KEY_PAD_UP, GainputDeviceType::GAINPUT_KEYBOARD, 1,{ { INPUT_X_AXIS , 1, gainput::KeyUp },{},{},{} } },
	{ KEY_PAD_DOWN,  GainputDeviceType::GAINPUT_KEYBOARD, 1,{ { INPUT_X_AXIS  ,-1, gainput::KeyDown },{},{},{} } },
	{ KEY_PAD_LEFT, GainputDeviceType::GAINPUT_KEYBOARD, 1,{ { INPUT_X_AXIS , -1, gainput::KeyLeft },{},{},{} } },
	{ KEY_PAD_RIGHT,  GainputDeviceType::GAINPUT_KEYBOARD, 1,{ { INPUT_X_AXIS  , 1, gainput::KeyRight },{},{},{} } },

	//this will map to x y buttons
	{ KEY_BUTTON_X, GainputDeviceType::GAINPUT_KEYBOARD, 1,{ { INPUT_X_AXIS , 1, gainput::KeyQ },{},{},{} } },
	{ KEY_BUTTON_Y, GainputDeviceType::GAINPUT_KEYBOARD, 1,{ { INPUT_X_AXIS , 1, gainput::KeyF },{},{},{} } },

	//this will map to L3 R3
	{ KEY_LEFT_STICK_BUTTON, GainputDeviceType::GAINPUT_KEYBOARD, 1,{ { INPUT_X_AXIS , 1, gainput::KeyF1 },{},{},{} } },
	{ KEY_RIGHT_STICK_BUTTON, GainputDeviceType::GAINPUT_KEYBOARD, 1,{ { INPUT_X_AXIS , 1, gainput::KeyBackSpace },{},{},{} } },

	//CONFIRM
	//MOUSE
	{ KEY_CONFIRM, GainputDeviceType::GAINPUT_MOUSE, 1,{ { INPUT_X_AXIS , 1, gainput::MouseButton0 },{},{},{} } },

	//CANCEL
	//Keyboard
	{ KEY_CANCEL,  GainputDeviceType::GAINPUT_KEYBOARD, 1,{ { INPUT_X_AXIS  , 1, gainput::KeyEscape },{},{},{} } },

	//MENU
	//Keyboard
	{ KEY_MENU,  GainputDeviceType::GAINPUT_KEYBOARD, 1,{ { INPUT_X_AXIS  , 1, gainput::KeyReturn },{},{},{} } },


	//Keyboard chars
	{ KEY_CHAR_A,  GainputDeviceType::GAINPUT_KEYBOARD, 1,{ { INPUT_X_AXIS, 1, gainput::KeyA },{},{},{} } },
	{ KEY_CHAR_B,  GainputDeviceType::GAINPUT_KEYBOARD, 1,{ { INPUT_X_AXIS, 1, gainput::KeyB },{},{},{} } },
	{ KEY_CHAR_C,  GainputDeviceType::GAINPUT_KEYBOARD, 1,{ { INPUT_X_AXIS, 1, gainput::KeyC },{},{},{} } },
	{ KEY_CHAR_D,  GainputDeviceType::GAINPUT_KEYBOARD, 1,{ { INPUT_X_AXIS, 1, gainput::KeyD },{},{},{} } },
	{ KEY_CHAR_E,  GainputDeviceType::GAINPUT_KEYBOARD, 1,{ { INPUT_X_AXIS, 1, gainput::KeyE },{},{},{} } },
	{ KEY_CHAR_F,  GainputDeviceType::GAINPUT_KEYBOARD, 1,{ { INPUT_X_AXIS, 1, gainput::KeyF },{},{},{} } },
	{ KEY_CHAR_G,  GainputDeviceType::GAINPUT_KEYBOARD, 1,{ { INPUT_X_AXIS, 1, gainput::KeyG },{},{},{} } },
	{ KEY_CHAR_H,  GainputDeviceType::GAINPUT_KEYBOARD, 1,{ { INPUT_X_AXIS, 1, gainput::KeyH },{},{},{} } },
	{ KEY_CHAR_I,  GainputDeviceType::GAINPUT_KEYBOARD, 1,{ { INPUT_X_AXIS, 1, gainput::KeyI },{},{},{} } },
	{ KEY_CHAR_J,  GainputDeviceType::GAINPUT_KEYBOARD, 1,{ { INPUT_X_AXIS, 1, gainput::KeyJ },{},{},{} } },
	{ KEY_CHAR_K,  GainputDeviceType::GAINPUT_KEYBOARD, 1,{ { INPUT_X_AXIS, 1, gainput::KeyK },{},{},{} } },
	{ KEY_CHAR_L,  GainputDeviceType::GAINPUT_KEYBOARD, 1,{ { INPUT_X_AXIS, 1, gainput::KeyL },{},{},{} } },
	{ KEY_CHAR_M,  GainputDeviceType::GAINPUT_KEYBOARD, 1,{ { INPUT_X_AXIS, 1, gainput::KeyM },{},{},{} } },
	{ KEY_CHAR_N,  GainputDeviceType::GAINPUT_KEYBOARD, 1,{ { INPUT_X_AXIS, 1, gainput::KeyN },{},{},{} } },
	{ KEY_CHAR_O,  GainputDeviceType::GAINPUT_KEYBOARD, 1,{ { INPUT_X_AXIS, 1, gainput::KeyO },{},{},{} } },
	{ KEY_CHAR_P,  GainputDeviceType::GAINPUT_KEYBOARD, 1,{ { INPUT_X_AXIS, 1, gainput::KeyP },{},{},{} } },
	{ KEY_CHAR_Q,  GainputDeviceType::GAINPUT_KEYBOARD, 1,{ { INPUT_X_AXIS, 1, gainput::KeyQ },{},{},{} } },
	{ KEY_CHAR_R,  GainputDeviceType::GAINPUT_KEYBOARD, 1,{ { INPUT_X_AXIS, 1, gainput::KeyR },{},{},{} } },
	{ KEY_CHAR_S,  GainputDeviceType::GAINPUT_KEYBOARD, 1,{ { INPUT_X_AXIS, 1, gainput::KeyS },{},{},{} } },
	{ KEY_CHAR_T,  GainputDeviceType::GAINPUT_KEYBOARD, 1,{ { INPUT_X_AXIS, 1, gainput::KeyT },{},{},{} } },
	{ KEY_CHAR_U,  GainputDeviceType::GAINPUT_KEYBOARD, 1,{ { INPUT_X_AXIS, 1, gainput::KeyU },{},{},{} } },
	{ KEY_CHAR_V,  GainputDeviceType::GAINPUT_KEYBOARD, 1,{ { INPUT_X_AXIS, 1, gainput::KeyV },{},{},{} } },
	{ KEY_CHAR_W,  GainputDeviceType::GAINPUT_KEYBOARD, 1,{ { INPUT_X_AXIS, 1, gainput::KeyW },{},{},{} } },
	{ KEY_CHAR_X,  GainputDeviceType::GAINPUT_KEYBOARD, 1,{ { INPUT_X_AXIS, 1, gainput::KeyX },{},{},{} } },
	{ KEY_CHAR_Y,  GainputDeviceType::GAINPUT_KEYBOARD, 1,{ { INPUT_X_AXIS, 1, gainput::KeyY },{},{},{} } },
	{ KEY_CHAR_Z,  GainputDeviceType::GAINPUT_KEYBOARD, 1,{ { INPUT_X_AXIS, 1, gainput::KeyZ },{},{},{} } }
};


//TODO: Add callbacks to mappings as Actions
//this will unify iOS as joysticks are virtual and can be mapped to any finger.
//TODO: Separate per device for simpler GetButtonData
static KeyMappingDescription gXboxMappings[] = {

	//Triggers
	//Keyboard
	{ KEY_LEFT_TRIGGER, GainputDeviceType::GAINPUT_GAMEPAD, 1,{ { INPUT_X_AXIS , 1, gainput::PadButtonAxis4 },{},{},{} } },
	{ KEY_RIGHT_TRIGGER,  GainputDeviceType::GAINPUT_GAMEPAD, 1,{ { INPUT_X_AXIS  , 1, gainput::PadButtonAxis5 },{},{},{} } },

	//Bumbers
	//Keyboard/Mouse
	{ KEY_LEFT_BUMPER, GainputDeviceType::GAINPUT_GAMEPAD, 1,{ { INPUT_X_AXIS , 1, gainput::PadButtonL1 },{},{},{} } },
	{ KEY_RIGHT_BUMPER,  GainputDeviceType::GAINPUT_GAMEPAD, 1,{ { INPUT_X_AXIS  , 1, gainput::PadButtonR1 },{},{},{} } },

	//STICKS
	//Keyboard/Mouse
	{ KEY_LEFT_STICK, GainputDeviceType::GAINPUT_GAMEPAD, 2,{ { INPUT_X_AXIS , 1, gainput::PadButtonLeftStickX },{ INPUT_Y_AXIS , 1, gainput::PadButtonLeftStickY },{},{} } },
	{ KEY_RIGHT_STICK, GainputDeviceType::GAINPUT_GAMEPAD, 2,{ { INPUT_X_AXIS , 1, gainput::PadButtonRightStickX },{ INPUT_Y_AXIS , -1, gainput::PadButtonRightStickY },{},{} } },

	//DPAD
	{ KEY_UI_MOVE,  GainputDeviceType::GAINPUT_GAMEPAD, 4,{ { INPUT_X_AXIS , 1, gainput::PadButtonRight },{ INPUT_X_AXIS , -1, gainput::PadButtonLeft },{ INPUT_Y_AXIS , 1, gainput::PadButtonUp },{ INPUT_Y_AXIS , -1, gainput::PadButtonDown } } },
	
	////PAD
	////KEYBOARD
	//{ KEY_PAD_UP, GainputDeviceType::GAINPUT_GAMEPAD, 1,{ { INPUT_X_AXIS , 1, gainput::PadButtonUp },{},{},{} }  },
	//{ KEY_PAD_DOWN,  GainputDeviceType::GAINPUT_GAMEPAD, 1,{ { INPUT_X_AXIS  ,-1, gainput::PadButtonDown },{},{},{} } },
	//{ KEY_PAD_LEFT, GainputDeviceType::GAINPUT_GAMEPAD, 1,{ { INPUT_X_AXIS , -1, gainput::PadButtonLeft },{},{},{} }  },
	//{ KEY_PAD_RIGHT,  GainputDeviceType::GAINPUT_GAMEPAD, 1,{ { INPUT_X_AXIS  , 1, gainput::PadButtonRight },{},{},{} } },

	//this will map to x y buttons
	{ KEY_BUTTON_X, GainputDeviceType::GAINPUT_GAMEPAD, 1,{ { INPUT_X_AXIS , 1, gainput::PadButtonX },{},{},{} } },
	{ KEY_BUTTON_Y, GainputDeviceType::GAINPUT_GAMEPAD, 1,{ { INPUT_X_AXIS , 1, gainput::PadButtonY },{},{},{} } },

	////this will map to L3 R3
	{ KEY_LEFT_STICK_BUTTON, GainputDeviceType::GAINPUT_GAMEPAD, 1,{ { INPUT_X_AXIS , 1, gainput::PadButtonL3 },{},{},{} } },
	{ KEY_RIGHT_STICK_BUTTON, GainputDeviceType::GAINPUT_GAMEPAD, 1,{ { INPUT_X_AXIS , 1, gainput::PadButtonR3},{},{},{} } },

	//CONFIRM
	//MOUSE
	{ KEY_CONFIRM, GainputDeviceType::GAINPUT_GAMEPAD, 1,{ { INPUT_X_AXIS , 1, gainput::PadButtonA },{},{},{} } },

	//CANCEL
	//Keyboard
	{ KEY_CANCEL,  GainputDeviceType::GAINPUT_GAMEPAD, 1,{ { INPUT_X_AXIS  , 1, gainput::PadButtonB },{},{},{} }  },

	//MENU
	//Keyboard
	{ KEY_MENU,  GainputDeviceType::GAINPUT_GAMEPAD, 1,{ { INPUT_X_AXIS  , 1, gainput::PadButtonStart },{},{},{} } }
};

#endif