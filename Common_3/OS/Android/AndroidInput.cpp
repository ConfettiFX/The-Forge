/*
 * Copyright (c) 2017-2024 The Forge Interactive Inc.
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

#include "../Interfaces/IOperatingSystem.h"
#include "../../Utilities/Interfaces/ILog.h"
#include "../../Utilities/Math/MathTypes.h"
#include "../Interfaces/IInput.h"

#include <android_native_app_glue.h>
#include <android/input.h>

#if !defined(QUEST_VR)
#define ENABLE_PADDLEBOAT
#endif

#if defined(ENABLE_PADDLEBOAT)
#include <paddleboat/paddleboat.h>
#endif

#include "../Input/InputCommon.h"

#include "../../Utilities/Interfaces/IMemory.h"

static constexpr uint32_t AKEYCODE_COUNT = AKEYCODE_PROFILE_SWITCH + 1;
static InputEnum          gKeyCodeMap[AKEYCODE_COUNT] = {};

static void MapKeyCodes()
{
    for (uint8_t i = 0; i < 26; ++i)
    {
        gKeyCodeMap[AKEYCODE_A + i] = (InputEnum)(K_A + i);
    }

    for (uint8_t i = 0; i < 10; ++i)
    {
        gKeyCodeMap[AKEYCODE_0 + i] = (InputEnum)(K_0 + i);
    }

    gKeyCodeMap[AKEYCODE_ENTER] = K_ENTER;
    gKeyCodeMap[AKEYCODE_ESCAPE] = K_ESCAPE;
    gKeyCodeMap[AKEYCODE_DEL] = K_BACKSPACE;
    gKeyCodeMap[AKEYCODE_TAB] = K_TAB;
    gKeyCodeMap[AKEYCODE_SPACE] = K_SPACE;
    gKeyCodeMap[AKEYCODE_MINUS] = K_MINUS;
    gKeyCodeMap[AKEYCODE_EQUALS] = K_EQUAL;
    gKeyCodeMap[AKEYCODE_LEFT_BRACKET] = K_LEFTBRACKET;
    gKeyCodeMap[AKEYCODE_RIGHT_BRACKET] = K_RIGHTBRACKET;
    gKeyCodeMap[AKEYCODE_BACKSLASH] = K_BACKSLASH;
    gKeyCodeMap[AKEYCODE_SEMICOLON] = K_SEMICOLON;
    gKeyCodeMap[AKEYCODE_APOSTROPHE] = K_APOSTROPHE;
    gKeyCodeMap[AKEYCODE_GRAVE] = K_GRAVE;
    gKeyCodeMap[AKEYCODE_COMMA] = K_COMMA;
    gKeyCodeMap[AKEYCODE_PERIOD] = K_PERIOD;
    gKeyCodeMap[AKEYCODE_SLASH] = K_SLASH;
    gKeyCodeMap[AKEYCODE_POUND] = INPUT_NONE;
    gKeyCodeMap[AKEYCODE_STAR] = INPUT_NONE;
    gKeyCodeMap[AKEYCODE_CAPS_LOCK] = K_CAPSLOCK;
    gKeyCodeMap[AKEYCODE_F1] = K_F1;
    gKeyCodeMap[AKEYCODE_F2] = K_F2;
    gKeyCodeMap[AKEYCODE_F3] = K_F3;
    gKeyCodeMap[AKEYCODE_F4] = K_F4;
    gKeyCodeMap[AKEYCODE_F5] = K_F5;
    gKeyCodeMap[AKEYCODE_F6] = K_F6;
    gKeyCodeMap[AKEYCODE_F7] = K_F7;
    gKeyCodeMap[AKEYCODE_F8] = K_F8;
    gKeyCodeMap[AKEYCODE_F9] = K_F9;
    gKeyCodeMap[AKEYCODE_F10] = K_F10;
    gKeyCodeMap[AKEYCODE_F11] = K_F11;
    gKeyCodeMap[AKEYCODE_F12] = K_F12;
    gKeyCodeMap[AKEYCODE_INSERT] = K_INS;
    gKeyCodeMap[AKEYCODE_HOME] = K_HOME;
    gKeyCodeMap[AKEYCODE_PAGE_UP] = K_PGUP;
    gKeyCodeMap[AKEYCODE_FORWARD_DEL] = K_DEL;
    gKeyCodeMap[AKEYCODE_PAGE_DOWN] = K_PGDN;
    gKeyCodeMap[AKEYCODE_DPAD_RIGHT] = K_RIGHTARROW;
    gKeyCodeMap[AKEYCODE_DPAD_LEFT] = K_LEFTARROW;
    gKeyCodeMap[AKEYCODE_DPAD_DOWN] = K_DOWNARROW;
    gKeyCodeMap[AKEYCODE_DPAD_UP] = K_UPARROW;
    gKeyCodeMap[AKEYCODE_NUM_LOCK] = K_KP_NUMLOCK;
    gKeyCodeMap[AKEYCODE_NUMPAD_DIVIDE] = K_KP_SLASH;
    gKeyCodeMap[AKEYCODE_NUMPAD_MULTIPLY] = K_KP_STAR;
    gKeyCodeMap[AKEYCODE_NUMPAD_SUBTRACT] = K_KP_MINUS;
    gKeyCodeMap[AKEYCODE_NUMPAD_ADD] = K_KP_PLUS;
    gKeyCodeMap[AKEYCODE_NUMPAD_ENTER] = K_KP_ENTER;
    gKeyCodeMap[AKEYCODE_NUMPAD_1] = K_KP_END;
    gKeyCodeMap[AKEYCODE_NUMPAD_2] = K_KP_DOWNARROW;
    gKeyCodeMap[AKEYCODE_NUMPAD_3] = K_KP_PGDN;
    gKeyCodeMap[AKEYCODE_NUMPAD_4] = K_KP_LEFTARROW;
    gKeyCodeMap[AKEYCODE_NUMPAD_5] = K_KP_NUMPAD_5;
    gKeyCodeMap[AKEYCODE_NUMPAD_6] = K_KP_RIGHTARROW;
    gKeyCodeMap[AKEYCODE_NUMPAD_7] = K_KP_HOME;
    gKeyCodeMap[AKEYCODE_NUMPAD_8] = K_KP_UPARROW;
    gKeyCodeMap[AKEYCODE_NUMPAD_9] = K_KP_PGUP;
    gKeyCodeMap[AKEYCODE_NUMPAD_0] = K_KP_INS;
    gKeyCodeMap[AKEYCODE_NUMPAD_DOT] = K_KP_DEL;
    gKeyCodeMap[AKEYCODE_NUMPAD_EQUALS] = K_KP_EQUALS;
    gKeyCodeMap[AKEYCODE_NUMPAD_LEFT_PAREN] = INPUT_NONE;
    gKeyCodeMap[AKEYCODE_NUMPAD_RIGHT_PAREN] = INPUT_NONE;
    gKeyCodeMap[AKEYCODE_CTRL_LEFT] = K_LCTRL;
    gKeyCodeMap[AKEYCODE_SHIFT_LEFT] = K_LSHIFT;
    gKeyCodeMap[AKEYCODE_ALT_LEFT] = K_LALT;
    gKeyCodeMap[AKEYCODE_CTRL_RIGHT] = K_RCTRL;
    gKeyCodeMap[AKEYCODE_SHIFT_RIGHT] = K_RSHIFT;
    gKeyCodeMap[AKEYCODE_ALT_RIGHT] = K_RALT;
    gKeyCodeMap[AKEYCODE_MENU] = K_MENU;
    gKeyCodeMap[AKEYCODE_SCROLL_LOCK] = K_SCROLLLOCK;

    gKeyCodeMap[AKEYCODE_BREAK] = K_PAUSE;
    gKeyCodeMap[AKEYCODE_SYSRQ] = K_PRINTSCREEN;
}
/************************************************************************/
// Touch
/************************************************************************/
#if defined(ENABLE_FORGE_TOUCH_INPUT)
#include "../Input/TouchInput.h"
#endif
/************************************************************************/
// Gamepad
/************************************************************************/
#if defined(ENABLE_PADDLEBOAT)
struct PBGamepad
{
    Paddleboat_Controller_Info mInfo;
    int32_t                    mControllerIndex;
};

PBGamepad gPBGamepads[MAX_GAMEPADS] = {};

static void GamepadUpdateState(JNIEnv* env, InputPortIndex portIndex)
{
    PBGamepad& pgpad = gPBGamepads[portIndex];
    Gamepad&   gpad = gGamepads[portIndex];
    GamepadUpdateLastState(portIndex);

    if (!inputGamepadIsActive(portIndex))
    {
        return;
    }

    Paddleboat_Controller_Data data = {};
    Paddleboat_ErrorCode       paddleboatError = Paddleboat_getControllerData(pgpad.mControllerIndex, &data);
    if (PADDLEBOAT_NO_ERROR != paddleboatError)
    {
        return;
    }

    uint32_t buttons = data.buttonsDown;
    gpad.mButtons[GPAD_UP - GPAD_BTN_FIRST] = ((buttons & PADDLEBOAT_BUTTON_DPAD_UP) != 0);
    gpad.mButtons[GPAD_DOWN - GPAD_BTN_FIRST] = ((buttons & PADDLEBOAT_BUTTON_DPAD_DOWN) != 0);
    gpad.mButtons[GPAD_LEFT - GPAD_BTN_FIRST] = ((buttons & PADDLEBOAT_BUTTON_DPAD_LEFT) != 0);
    gpad.mButtons[GPAD_RIGHT - GPAD_BTN_FIRST] = ((buttons & PADDLEBOAT_BUTTON_DPAD_RIGHT) != 0);
    gpad.mButtons[GPAD_A - GPAD_BTN_FIRST] = ((buttons & PADDLEBOAT_BUTTON_A) != 0);
    gpad.mButtons[GPAD_B - GPAD_BTN_FIRST] = ((buttons & PADDLEBOAT_BUTTON_B) != 0);
    gpad.mButtons[GPAD_X - GPAD_BTN_FIRST] = ((buttons & PADDLEBOAT_BUTTON_X) != 0);
    gpad.mButtons[GPAD_Y - GPAD_BTN_FIRST] = ((buttons & PADDLEBOAT_BUTTON_Y) != 0);
    gpad.mButtons[GPAD_START - GPAD_BTN_FIRST] = ((buttons & PADDLEBOAT_BUTTON_START) != 0);
    gpad.mButtons[GPAD_BACK - GPAD_BTN_FIRST] = ((buttons & PADDLEBOAT_BUTTON_SELECT) != 0);
    gpad.mButtons[GPAD_L3 - GPAD_BTN_FIRST] = ((buttons & PADDLEBOAT_BUTTON_L3) != 0);
    gpad.mButtons[GPAD_R3 - GPAD_BTN_FIRST] = ((buttons & PADDLEBOAT_BUTTON_R3) != 0);
    gpad.mButtons[GPAD_L1 - GPAD_BTN_FIRST] = ((buttons & PADDLEBOAT_BUTTON_L1) != 0);
    gpad.mButtons[GPAD_R1 - GPAD_BTN_FIRST] = ((buttons & PADDLEBOAT_BUTTON_R1) != 0);

    gpad.mAxis[GPAD_LX - GPAD_AXIS_FIRST] = data.leftStick.stickX;
    // Invert Y to match other platform convention
    gpad.mAxis[GPAD_LY - GPAD_AXIS_FIRST] = -data.leftStick.stickY;
    gpad.mAxis[GPAD_RX - GPAD_AXIS_FIRST] = data.rightStick.stickX;
    // Invert Y to match other platform convention
    gpad.mAxis[GPAD_RY - GPAD_AXIS_FIRST] = -data.rightStick.stickY;
    gpad.mAxis[GPAD_L2 - GPAD_AXIS_FIRST] = data.triggerL2;
    gpad.mAxis[GPAD_R2 - GPAD_AXIS_FIRST] = data.triggerR2;
    GamepadProcessStick(portIndex, GPAD_LX);
    GamepadProcessStick(portIndex, GPAD_RX);
    GamepadProcessTrigger(portIndex, GPAD_L2);
    GamepadProcessTrigger(portIndex, GPAD_R2);

    // Rumble
    bool stopRumble = false;
    if (gpad.mRumbleHigh == 0.0f && gpad.mRumbleLow == 0.0f)
    {
        stopRumble = true;
    }
    // Dont keep setting zero rumble
    if (!gpad.mRumbleStopped && (pgpad.mInfo.controllerFlags & PADDLEBOAT_CONTROLLER_FLAG_VIBRATION))
    {
        const int32_t             vibrationTimeMs = 250;
        Paddleboat_Vibration_Data vibration = {};
        vibration.durationLeft = vibrationTimeMs;
        vibration.durationRight = vibrationTimeMs;
        vibration.intensityLeft = gpad.mRumbleHigh;
        vibration.intensityRight = gpad.mRumbleLow;
        Paddleboat_setControllerVibrationData(pgpad.mControllerIndex, &vibration, env);
    }
    gpad.mRumbleStopped = stopRumble;

    // Lighting
    if (pgpad.mInfo.controllerFlags & PADDLEBOAT_LIGHT_RGB)
    {
        if (gpad.mLightReset)
        {
            Paddleboat_setControllerLight(pgpad.mControllerIndex, PADDLEBOAT_LIGHT_RGB, 0, env);
            gpad.mLightReset = false;
        }
        else if (gpad.mLightUpdate)
        {
            uint32_t light = packA8B8G8R8(float4(gpad.mLight, 1.0f));
            Paddleboat_setControllerLight(pgpad.mControllerIndex, PADDLEBOAT_LIGHT_RGB, light, env);
            gpad.mLightUpdate = false;
        }
    }
}

static InputPortIndex GamepadFindEmptySlot()
{
    for (InputPortIndex portIndex = 0; portIndex < MAX_GAMEPADS; ++portIndex)
    {
        if (!inputGamepadIsActive(portIndex))
        {
            return portIndex;
        }
    }

    return PORT_INDEX_INVALID;
}

static InputPortIndex GamepadAddController(const int32_t controllerIndex)
{
    InputPortIndex portIndex = GamepadFindEmptySlot();
    if (PORT_INDEX_INVALID == portIndex)
    {
        return portIndex;
    }

    Paddleboat_Controller_Info info = {};
    Paddleboat_ErrorCode       paddleboatError = Paddleboat_getControllerInfo(controllerIndex, &info);
    PBGamepad&                 pgpad = gPBGamepads[portIndex];
    pgpad.mControllerIndex = controllerIndex;
    if (PADDLEBOAT_NO_ERROR == paddleboatError)
    {
        pgpad.mInfo = info;
    }

    static char controllerNames[MAX_GAMEPADS][FS_MAX_PATH] = {};
    Paddleboat_getControllerName(controllerIndex, TF_ARRAY_COUNT(controllerNames[0]), controllerNames[portIndex]);

    GamepadResetState(portIndex);
    gGamepads[portIndex].mActive = true;
    gGamepads[portIndex].pName = controllerNames[portIndex];
    if (gGamepadAddedCb)
    {
        gGamepadAddedCb(portIndex);
    }

    return portIndex;
}

void GamepadRemoveController(int32_t controllerIndex)
{
    InputPortIndex portIndex = PORT_INDEX_INVALID;
    for (uint32_t p = 0; p < MAX_GAMEPADS; ++p)
    {
        if (gPBGamepads[p].mControllerIndex == controllerIndex)
        {
            portIndex = p;
            break;
        }
    }

    if (PORT_INDEX_INVALID == portIndex)
    {
        return;
    }

    if (!inputGamepadIsActive(portIndex))
    {
        return;
    }

    PBGamepad& pgpad = gPBGamepads[portIndex];
    pgpad = {};
    pgpad.mControllerIndex = -1;

    GamepadResetState(portIndex);
    gGamepads[portIndex].mActive = false;
    gGamepads[portIndex].pName = gGamepadDisconnectedName;

    if (gGamepadRemovedCb)
    {
        gGamepadRemovedCb(portIndex);
    }
}

static void GameControllerStatusCallback(const int32_t controllerIndex, const Paddleboat_ControllerStatus status, void*)
{
    if (PADDLEBOAT_CONTROLLER_JUST_CONNECTED == status)
    {
        GamepadAddController(controllerIndex);
    }
    else if (PADDLEBOAT_CONTROLLER_JUST_DISCONNECTED == status)
    {
        GamepadRemoveController(controllerIndex);
    }
}
#endif
/************************************************************************/
// Keyboard
/************************************************************************/
static void ProcessKey(AInputEvent* event)
{
    int32_t         keyCode = AKeyEvent_getKeyCode(event);
    int32_t         action = AKeyEvent_getAction(event);
    const bool      pressed = AKEY_EVENT_ACTION_DOWN == action;
    const InputEnum key = (keyCode >= 0 && keyCode < TF_ARRAY_COUNT(gKeyCodeMap)) ? gKeyCodeMap[keyCode] : INPUT_NONE;
    if (key != INPUT_NONE)
    {
        gInputValues[key] = pressed;
    }
}
/************************************************************************/
// Platform implementation
/************************************************************************/
void platformInitInput(JNIEnv* env, jobject activity)
{
#if defined(ENABLE_PADDLEBOAT)
    // Setup Paddleboat game controller lib
    extern jobject       AndroidGetActivity();
    Paddleboat_ErrorCode paddleboatError = Paddleboat_init(env, activity);
    if (PADDLEBOAT_NO_ERROR == paddleboatError)
    {
        Paddleboat_setControllerStatusCallback(GameControllerStatusCallback, NULL);
        for (uint32_t p = 0; p < MAX_GAMEPADS; ++p)
        {
            gPBGamepads[p].mControllerIndex = -1;
        }
    }
    else
    {
        LOGF(eERROR, "Failed to initialize Paddleboat with error %u", paddleboatError);
    }
#endif

    MapKeyCodes();

#if defined(ENABLE_FORGE_TOUCH_INPUT)
    ResetTouchEvents();
    gVirtualJoystickLeft.mActive = true;
    gVirtualJoystickRight.mActive = true;
#endif

    InputInitCommon();
}

void platformExitInput(JNIEnv* env)
{
#if defined(ENABLE_PADDLEBOAT)
    if (Paddleboat_isInitialized())
    {
        Paddleboat_destroy(env);
    }
#endif
}

void platformUpdateLastInputState(android_app* app)
{
    memcpy(gLastInputValues, gInputValues, sizeof(gInputValues));

    gInputValues[MOUSE_WHEEL_UP] = false;
    gInputValues[MOUSE_WHEEL_DOWN] = false;
    gInputValues[MOUSE_DX] = 0;
    gInputValues[MOUSE_DY] = 0;
}

void platformUpdateInput(android_app* app, JNIEnv* env, uint32_t width, uint32_t height, float dt)
{
#if defined(ENABLE_PADDLEBOAT)
    if (Paddleboat_isInitialized())
    {
        Paddleboat_update(env);
        for (InputPortIndex portIndex = 0; portIndex < MAX_GAMEPADS; ++portIndex)
        {
            GamepadUpdateState(env, portIndex);
        }
    }
#endif

#if defined(ENABLE_FORGE_TOUCH_INPUT)

    // Touch events
    ProcessTouchEvents(width, height, dt);

    // Update mouse
    gInputValues[MOUSE_X] = gCursorPos[0];
    gInputValues[MOUSE_Y] = gCursorPos[1];
#endif

    gDeltaTime = dt;
}

void platformInputOnStart(JNIEnv* env)
{
#if defined(ENABLE_PADDLEBOAT)
    if (Paddleboat_isInitialized())
    {
        Paddleboat_onStart(env);
    }
#endif
}

void platformInputOnStop(JNIEnv* env)
{
#if defined(ENABLE_PADDLEBOAT)
    if (Paddleboat_isInitialized())
    {
        Paddleboat_onStop(env);
    }
#endif
}

int32_t platformInputEvent(struct android_app* app, AInputEvent* event)
{
#if defined(ENABLE_PADDLEBOAT)
    if (Paddleboat_isInitialized())
    {
        if (Paddleboat_processInputEvent(event))
        {
            return 1;
        }
    }
#endif

    const int32_t type = AInputEvent_getType(event);
    const int32_t source = AInputEvent_getSource(event);
    const bool    joystick = (source & AINPUT_SOURCE_JOYSTICK);
    const bool    gamepad = (source & AINPUT_SOURCE_GAMEPAD);
    const bool    touchscreen = (source & (AINPUT_SOURCE_TOUCHSCREEN | AINPUT_SOURCE_STYLUS));
    const bool    mouse = (source & AINPUT_SOURCE_MOUSE);
    const bool    keyboard = (source & AINPUT_SOURCE_KEYBOARD);

    if (!joystick && !gamepad && !touchscreen && !keyboard && !mouse)
    {
        return 0;
    }

    if (AINPUT_EVENT_TYPE_KEY == type)
    {
        if (joystick || gamepad || keyboard)
        {
            ProcessKey(event);
        }
    }
    else if (AINPUT_EVENT_TYPE_MOTION == type)
    {
        int32_t action = AMotionEvent_getAction(event);
        int32_t motionAction = action & AMOTION_EVENT_ACTION_MASK;
        // Treat tap as click
        if (touchscreen || mouse)
        {
            int32_t pointerIndex = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;

            if (AMOTION_EVENT_ACTION_DOWN == motionAction || AMOTION_EVENT_ACTION_UP == motionAction)
            {
                static int32_t previousButtonState = 0;
                int32_t        buttonState = AMotionEvent_getButtonState(event);

                const bool pressed = (AMOTION_EVENT_ACTION_DOWN == motionAction);
                if (!pressed)
                {
                    buttonState = previousButtonState;
                }
                if (!buttonState || (buttonState & AMOTION_EVENT_BUTTON_PRIMARY))
                {
                    gInputValues[MOUSE_1] = pressed;
                }
                if (buttonState & AMOTION_EVENT_BUTTON_SECONDARY)
                {
                    gInputValues[MOUSE_2] = pressed;
                }
                if (buttonState & AMOTION_EVENT_BUTTON_TERTIARY)
                {
                    gInputValues[MOUSE_3] = pressed;
                }

                previousButtonState = buttonState;
            }
            if (AMOTION_EVENT_ACTION_SCROLL == motionAction)
            {
                float scroll = AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_VSCROLL, pointerIndex);
                if (scroll > 0.0f)
                {
                    gInputValues[MOUSE_WHEEL_UP] = true;
                }
                else
                {
                    gInputValues[MOUSE_WHEEL_DOWN] = true;
                }
            }
        }

#if defined(ENABLE_FORGE_TOUCH_INPUT)
        if (touchscreen)
        {
            const uint32_t maxPointerCount = 8;

            int32_t    pointerIndex = INT32_MAX;
            TouchPhase phase;

            switch (motionAction)
            {
            case AMOTION_EVENT_ACTION_DOWN:
            case AMOTION_EVENT_ACTION_POINTER_DOWN:
            {
                pointerIndex = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
                phase = TOUCH_BEGAN;
                break;
            }
            case AMOTION_EVENT_ACTION_MOVE:
            {
                const uint32_t pointerCount = AMotionEvent_getPointerCount(event);
                for (uint32_t i = 0; (i < pointerCount) && (i < maxPointerCount); ++i)
                {
                    int32_t    pointerId = AMotionEvent_getPointerId(event, i);
                    int32_t    x = (int32_t)(AMotionEvent_getX(event, i));
                    int32_t    y = (int32_t)(AMotionEvent_getY(event, i));
                    TouchEvent touch = {};
                    touch.mId = pointerId;
                    touch.mPhase = TOUCH_MOVED;
                    touch.mPos[0] = x;
                    touch.mPos[1] = y;
                    AddTouchEvent(touch);
                }
                break;
            }
            case AMOTION_EVENT_ACTION_CANCEL:
            case AMOTION_EVENT_ACTION_UP:
            case AMOTION_EVENT_ACTION_POINTER_UP:
            {
                pointerIndex = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
                phase = TOUCH_ENDED;
                break;
            }
            }

            if (pointerIndex != INT32_MAX)
            {
                int32_t    pointerId = AMotionEvent_getPointerId(event, pointerIndex);
                int32_t    x = (int32_t)(AMotionEvent_getX(event, pointerIndex));
                int32_t    y = (int32_t)(AMotionEvent_getY(event, pointerIndex));
                TouchEvent touch = {};
                touch.mId = pointerId;
                touch.mPhase = phase;
                touch.mPos[0] = x;
                touch.mPos[1] = y;
                AddTouchEvent(touch);
            }
        }
#endif
    }

    return 0;
}
