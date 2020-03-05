#pragma once

#include "IOperatingSystem.h"

extern uint32_t MAX_INPUT_MULTI_TOUCHES;
extern uint32_t MAX_INPUT_GAMEPADS;
extern uint32_t MAX_INPUT_ACTIONS;

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
		BUTTON_SOUTH, // A/CROSS
		BUTTON_EAST, // B/CIRCLE
		BUTTON_WEST, // X/SQUARE
		BUTTON_NORTH, // Y/TRIANGLE
		BUTTON_L1,
		BUTTON_R1,
		BUTTON_L2,
		BUTTON_R2,
		BUTTON_L3, // LEFT THUMB
		BUTTON_R3, // RIGHT THUMB
		BUTTON_HOME, // PS BUTTON
		BUTTON_BACK,
        BUTTON_FULLSCREEN,
        BUTTON_EXIT,
		BUTTON_DUMP,
		BUTTON_ANY,
		BUTTON_BINDINGS_END = BUTTON_ANY,
		/**********************************************/
        // Gesture bindings
		/**********************************************/
        GESTURE_BINDINGS_BEGIN,
        GESTURE_TAP = GESTURE_BINDINGS_BEGIN,
        GESTURE_PAN,
        GESTURE_PINCH,
        GESTURE_ROTATE,
        GESTURE_LONG_PRESS,
        GESTURE_BINDINGS_END = GESTURE_LONG_PRESS,
        /**********************************************/
        /**********************************************/
		TEXT,
	} Binding;
        
    typedef struct GestureDesc
    {
        /// Configuring Pan gesture
        uint32_t    mMinNumberOfTouches;
        uint32_t    mMaxNumberOfTouches;
        /// Configuring Tap gesture (single tap, double tap, ...)
        uint32_t    mNumberOfTapsRequired;
        /// Configuring Long press gesture
        float       mMinimumPressDuration;
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
	INPUT_ACTION_PHASE_PERFORMED,
	/// Example: left mouse button was pressed and now released, gesture was started but got canceled
	INPUT_ACTION_PHASE_CANCELED,
} InputActionPhase;

typedef struct InputActionContext
{    
	void*            pUserData;
	union
	{
        /// Gesture input
        float4       mFloat4;
		/// 3D input (gyroscope, ...)
		float3       mFloat3;
		/// 2D input (mouse position, delta, composite input (wasd), gamepad stick, joystick, ...)
		float2       mFloat2;
		/// 1D input (composite input (ws), gamepad left trigger, ...)
		float        mFloat;
		/// Button input (mouse left button, keyboard keys, ...)
		bool         mBool;
		/// Text input
		wchar_t*     pText;
	};

    float2*          pPosition;
	const bool*      pCaptured;
	uint32_t         mBinding;
	/// What phase is the action currently in
	uint8_t          mPhase;
	uint8_t          mDeviceType;
} InputActionContext;

typedef bool (*InputActionCallback)(InputActionContext* pContext);

typedef struct InputActionDesc
{
    /// Value from InputBindings::Binding enum
	uint32_t                    mBinding;
	/// Callback when an action is initiated, performed or canceled
	InputActionCallback         pFunction;
	/// User data which will be assigned to InputActionContext::pUserData when calling pFunction
	void*                       pUserData;
    /// Virtual joystick
    float                       mDeadzone;
    float                       mOutsideRadius;
    float                       mScale;
	/// User management (which user does this action apply to)
	uint8_t                     mUserId;
    /// Gesture desc
    InputBindings::GestureDesc* pGesture;
} InputActionDesc;

bool          initInputSystem(struct WindowsDesc* pWindow);
void          exitInputSystem();
void          updateInputSystem(uint32_t width, uint32_t height);
InputAction*  addInputAction(const InputActionDesc* pDesc);
void          removeInputAction(InputAction* pAction);
bool          setEnableCaptureInput(bool enable);
/// Used to enable/disable text input for non-keyboard setups (virtual keyboards for console/mobile, ...)
void          setVirtualKeyboard(uint32_t type);
