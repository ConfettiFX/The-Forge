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

#pragma once

#if defined(__INTELLISENSE__)
#include "../Interfaces/IOperatingSystem.h"
#include "../../Utilities/Interfaces/ILog.h"
#include "../../Utilities/Interfaces/IThread.h"
#include "../../Utilities/Threading/Atomics.h"
#include "../../Utilities/Math/MathTypes.h"
#include "../Interfaces/IInput.h"
#endif

// clang-format off
#define DECL_INPUTS(action)      \
action(INPUT_NONE)               \
action(MOUSE_1)                  \
action(MOUSE_2)                  \
action(MOUSE_3)                  \
action(MOUSE_4)                  \
action(MOUSE_WHEEL_UP)           \
action(MOUSE_WHEEL_DOWN)         \
action(MOUSE_X)                  \
action(MOUSE_Y)                  \
action(MOUSE_DX)                 \
action(MOUSE_DY)                 \
action(VGPAD_LX)                 \
action(VGPAD_LY)                 \
action(VGPAD_RX)                 \
action(VGPAD_RY)                 \
action(K_ESCAPE)                 \
action(K_0)                      \
action(K_1)                      \
action(K_2)                      \
action(K_3)                      \
action(K_4)                      \
action(K_5)                      \
action(K_6)                      \
action(K_7)                      \
action(K_8)                      \
action(K_9)                      \
action(K_MINUS)                  \
action(K_EQUAL)                  \
action(K_KP_EQUALS)              \
action(K_BACKSPACE)              \
action(K_TAB)                    \
action(K_A)                      \
action(K_B)                      \
action(K_C)                      \
action(K_D)                      \
action(K_E)                      \
action(K_F)                      \
action(K_G)                      \
action(K_H)                      \
action(K_I)                      \
action(K_J)                      \
action(K_K)                      \
action(K_L)                      \
action(K_M)                      \
action(K_N)                      \
action(K_O)                      \
action(K_P)                      \
action(K_Q)                      \
action(K_R)                      \
action(K_S)                      \
action(K_T)                      \
action(K_U)                      \
action(K_V)                      \
action(K_W)                      \
action(K_X)                      \
action(K_Y)                      \
action(K_Z)                      \
action(K_LEFTBRACKET)            \
action(K_RIGHTBRACKET)           \
action(K_ENTER)                  \
action(K_KP_ENTER)               \
action(K_LCTRL)                  \
action(K_RCTRL)                  \
action(K_SEMICOLON)              \
action(K_APOSTROPHE)             \
action(K_GRAVE)                  \
action(K_LSHIFT)                 \
action(K_BACKSLASH)              \
action(K_COMMA)                  \
action(K_PERIOD)                 \
action(K_SLASH)                  \
action(K_KP_SLASH)               \
action(K_RSHIFT)                 \
action(K_KP_STAR)                \
action(K_PRINTSCREEN)            \
action(K_LALT)                   \
action(K_RALT)                   \
action(K_SPACE)                  \
action(K_CAPSLOCK)               \
action(K_F1)                     \
action(K_F2)                     \
action(K_F3)                     \
action(K_F4)                     \
action(K_F5)                     \
action(K_F6)                     \
action(K_F7)                     \
action(K_F8)                     \
action(K_F9)                     \
action(K_F10)                    \
action(K_F11)                    \
action(K_F12)                    \
action(K_PAUSE)                  \
action(K_KP_NUMLOCK)             \
action(K_SCROLLLOCK)             \
action(K_KP_HOME)                \
action(K_HOME)                   \
action(K_KP_UPARROW)             \
action(K_UPARROW)                \
action(K_KP_PGUP)                \
action(K_PGUP)                   \
action(K_KP_MINUS)               \
action(K_KP_LEFTARROW)           \
action(K_LEFTARROW)              \
action(K_KP_NUMPAD_5)            \
action(K_KP_RIGHTARROW)          \
action(K_RIGHTARROW)             \
action(K_KP_PLUS)                \
action(K_KP_END)                 \
action(K_END)                    \
action(K_KP_DOWNARROW)           \
action(K_DOWNARROW)              \
action(K_KP_PGDN)                \
action(K_PGDN)                   \
action(K_KP_INS)                 \
action(K_INS)                    \
action(K_KP_DEL)                 \
action(K_DEL)                    \
action(K_LWIN)                   \
action(K_RWIN)                   \
action(K_MENU)                   \
action(K_F13)                    \
action(K_F14)                    \
action(K_F15)                    \
action(GPAD_UP)                  \
action(GPAD_DOWN)                \
action(GPAD_LEFT)                \
action(GPAD_RIGHT)               \
action(GPAD_START)               \
action(GPAD_BACK)                \
action(GPAD_L3)                  \
action(GPAD_R3)                  \
action(GPAD_A)                   \
action(GPAD_B)                   \
action(GPAD_X)                   \
action(GPAD_Y)                   \
action(GPAD_L1)                  \
action(GPAD_R1)                  \
action(GPAD_LX)                  \
action(GPAD_LY)                  \
action(GPAD_RX)                  \
action(GPAD_RY)                  \
action(GPAD_L2)                  \
action(GPAD_R2)
// clang-format on

#define ENUM_ACTION(x) x,
#define STR_ACTION(x)  #x,

enum InputEnum : uint16_t
{
    DECL_INPUTS(ENUM_ACTION)
};

static const uint32_t CUSTOM_BINDING_FIRST = GPAD_R2 + 1;
// 512 custom key bindings
static const uint32_t CUSTOM_BINDING_LAST = CUSTOM_BINDING_FIRST + 511;
static const uint32_t CUSTOM_BINDING_COUNT = CUSTOM_BINDING_LAST - CUSTOM_BINDING_FIRST + 1;

static const uint32_t MOUSE_BTN_FIRST = MOUSE_1;
static const uint32_t MOUSE_BTN_LAST = MOUSE_WHEEL_DOWN;
static const uint32_t MOUSE_BTN_COUNT = MOUSE_BTN_LAST - MOUSE_BTN_FIRST + 1;

static const uint32_t MOUSE_AXIS_FIRST = MOUSE_X;
static const uint32_t MOUSE_AXIS_LAST = MOUSE_DY;
static const uint32_t MOUSE_AXIS_COUNT = MOUSE_AXIS_LAST - MOUSE_AXIS_FIRST + 1;

static const uint32_t VGPAD_AXIS_FIRST = VGPAD_LX;
static const uint32_t VGPAD_AXIS_LAST = VGPAD_RY;
static const uint32_t VGPAD_AXIS_COUNT = VGPAD_AXIS_LAST - VGPAD_AXIS_FIRST + 1;

static const uint32_t K_FIRST = K_ESCAPE;
static const uint32_t K_LAST = K_F15;
static const uint32_t K_COUNT = K_LAST - K_FIRST + 1;

static const uint32_t GPAD_BTN_FIRST = GPAD_UP;
static const uint32_t GPAD_BTN_LAST = GPAD_R1;
static const uint32_t GPAD_BTN_COUNT = GPAD_BTN_LAST - GPAD_BTN_FIRST + 1;

static const uint32_t GPAD_AXIS_FIRST = GPAD_LX;
static const uint32_t GPAD_AXIS_LAST = GPAD_R2;
static const uint32_t GPAD_AXIS_COUNT = GPAD_AXIS_LAST - GPAD_AXIS_FIRST + 1;

struct Gamepad
{
    const char* pName;

    bool mButtons[GPAD_BTN_COUNT];
    bool mLastButtons[GPAD_BTN_COUNT];

    float mAxis[GPAD_AXIS_COUNT];
    float mLastAxis[GPAD_AXIS_COUNT];
    float mDeadzones[GPAD_AXIS_COUNT][2];

    float mRumbleLow;
    float mRumbleHigh;
    bool  mRumbleStopped;

    float3  mLight;
    uint8_t mLightUpdate : 1;
    uint8_t mLightReset : 1;
    bool    mActive;
};

enum TouchPhase
{
    TOUCH_BEGAN,
    TOUCH_MOVED,
    TOUCH_ENDED,
    TOUCH_CANCELED,
};

struct TouchEvent
{
    int32_t    mId;
    int32_t    mPos[2];
    TouchPhase mPhase;
};
static const int32_t TOUCH_ID_INVALID = -1;
typedef void (*InputTouchEventCallback)(const TouchEvent* event, void* pData);

struct VirtualJoystick
{
    float2  mStartPos;
    float2  mPos;
    float2  mSticks;
    int32_t mId;
    float   mDeadZone;
    float   mRadius;
    bool    mActive;
};

struct InputCustomBindingElement
{
    // Primary button/key/axis/...
    InputEnum mInput;
    // Whether value is for button reset or button press (Exit when K_ESCAPE is released, ...)
    bool      mReleased;
    // Multiplier - Useful for axis control using keys/buttons (non analog inputs) (K_W = 1.0f, K_S = -1.0f, ...)
    float     mMultiplier;
    // Condition (optional)
    InputEnum mCondInput;
    // Whether the condition is press or release (optional)
    bool      mCondReleased : 1;
    bool      mRemoveDt : 1;
};

static const uint32_t INPUT_ENUM_NAME_LENGTH_MAX = 32;
static const uint32_t CUSTOM_BINDING_ELEMENT_MAX = 5;

struct InputCustomBindingDesc
{
    InputCustomBindingElement mBindings[CUSTOM_BINDING_ELEMENT_MAX];
    uint32_t                  mCount;
};

Gamepad                 gGamepads[MAX_GAMEPADS] = {};
float                   gInputValues[MOUSE_BTN_COUNT + MOUSE_AXIS_COUNT + VGPAD_AXIS_COUNT + K_COUNT + 1] = {};
float                   gLastInputValues[MOUSE_BTN_COUNT + MOUSE_AXIS_COUNT + VGPAD_AXIS_COUNT + K_COUNT + 1] = {};
char32_t                gCharacterBuffer[128] = {};
uint32_t                gCharacterBufferCount = 0;
GamepadCallback         gGamepadAddedCb = {};
GamepadCallback         gGamepadRemovedCb = {};
VirtualJoystick         gVirtualJoystickLeft = {};
VirtualJoystick         gVirtualJoystickRight = {};
InputTouchEventCallback pCustomTouchEventFn = {};
void*                   pCustomTouchEventCallbackData = {};
bool                    gVirtualJoystickEnable = true;
static float            gDeltaTime = {};

struct InputCustomBinding
{
    char                      mName[INPUT_ENUM_NAME_LENGTH_MAX];
    InputCustomBindingElement mBindings[CUSTOM_BINDING_ELEMENT_MAX];
    uint32_t                  mCount;
    bool                      mValid;
};

static InputCustomBinding gCustomInputs[CUSTOM_BINDING_COUNT] = {};
/************************************************************************/
// Gamepad Helpers
/************************************************************************/
static const char*        gGamepadDisconnectedName = "N/A";

static void GamepadResetState(InputPortIndex portIndex)
{
    Gamepad& gpad = gGamepads[portIndex];
    memset(gpad.mButtons, 0, sizeof(gpad.mButtons));
    memset(gpad.mAxis, 0, sizeof(gpad.mAxis));
    memset(gpad.mLastButtons, 0, sizeof(gpad.mLastButtons));
    memset(gpad.mLastAxis, 0, sizeof(gpad.mLastAxis));
    gpad.mRumbleHigh = 0.0f;
    gpad.mRumbleLow = 0.0f;
    gpad.mRumbleStopped = false;
    gpad.mLightReset = true;
}

void GamepadUpdateLastState(InputPortIndex portIndex)
{
    Gamepad& gpad = gGamepads[portIndex];
    memcpy(gpad.mLastButtons, gpad.mButtons, sizeof(gpad.mButtons));
    memcpy(gpad.mLastAxis, gpad.mAxis, sizeof(gpad.mAxis));
}

static void GamepadProcessStick(InputPortIndex portIndex, InputEnum axisStart)
{
    Gamepad&    gpad = gGamepads[portIndex];
    float&      x = gpad.mAxis[axisStart - GPAD_AXIS_FIRST];
    float&      y = gpad.mAxis[axisStart + 1 - GPAD_AXIS_FIRST];
    vec2        stickVec = { x, y };
    const float minDeadzone = gpad.mDeadzones[axisStart - GPAD_AXIS_FIRST][0];
    const float maxDeadzone = gpad.mDeadzones[axisStart - GPAD_AXIS_FIRST][1];
    float       deadZoneTotal = (minDeadzone + maxDeadzone);

    float len = length(stickVec);
    if (len == 0.0f)
    {
        len = 1.0f;
    }
    if (len < minDeadzone)
    {
        len = 0.0f;
    }
    else if (len > (1.0f - maxDeadzone))
    {
        len = 1.0f;
    }
    else
    {
        len = (len - minDeadzone) / (1.0f - deadZoneTotal);
    }

    x = stickVec[0] * len;
    y = stickVec[1] * len;
}

static void GamepadProcessTrigger(InputPortIndex portIndex, InputEnum axisStart)
{
    Gamepad&    gpad = gGamepads[portIndex];
    float&      x = gpad.mAxis[axisStart - GPAD_AXIS_FIRST];
    const float deadzone = gpad.mDeadzones[axisStart - GPAD_AXIS_FIRST][0];
    if (x < deadzone)
    {
        x = 0.0f;
    }
    else if (x > 1.0f - deadzone)
    {
        x = 1.0f;
    }
}

// Apply deadzones, ...
void GamepadPostProcess(InputPortIndex portIndex)
{
    if (!inputGamepadIsActive(portIndex))
    {
        return;
    }

    GamepadProcessStick(portIndex, GPAD_LX);
    GamepadProcessStick(portIndex, GPAD_RX);
    GamepadProcessTrigger(portIndex, GPAD_L2);
    GamepadProcessTrigger(portIndex, GPAD_R2);
}

static void GamepadDefault()
{
    for (uint32_t portIndex = 0; portIndex < MAX_GAMEPADS; ++portIndex)
    {
        Gamepad& gpad = gGamepads[portIndex];
        GamepadResetState(portIndex);
        gpad.pName = gGamepadDisconnectedName;
        for (uint32_t a = 0; a < TF_ARRAY_COUNT(gpad.mDeadzones); ++a)
        {
            gpad.mDeadzones[a][0] = GAMEPAD_DEADZONE_DEFAULT_MIN;
            gpad.mDeadzones[a][1] = GAMEPAD_DEADZONE_DEFAULT_MAX;
        }
    }
}
/************************************************************************/
// Other helpers
/************************************************************************/
static void InputInitCommon()
{
    GamepadDefault();
    gVirtualJoystickLeft.mId = TOUCH_ID_INVALID;
    gVirtualJoystickRight.mId = TOUCH_ID_INVALID;

    memset(gCustomInputs, 0, sizeof(gCustomInputs));

    // Custom bindings builtin
    // Keyboard + Mouse
    inputAddCustomBindings(R"(
move_x; buttonanalog; K_D; 1.0f; K_A; -1.0f
move_y; buttonanalog; K_W; 1.0f; K_S; -1.0f
move_up; buttonanalog; K_E; 1.0f; K_Q; -1.0f
look_x; analog; MOUSE_DX; 0.0025f; cond; MOUSE_1; pressed; removedt
look_y; analog; MOUSE_DY; 0.0025f; cond; MOUSE_1; pressed; removedt
look_x; buttonanalog; K_H; 1.0f; K_F; -1.0f
look_y; buttonanalog; K_T; 1.0f; K_G; -1.0f
reset_view; button; K_SPACE; released
toggle_ui; button; K_F1; released
toggle_interface; button; K_F2; released
dump_profile; button; K_F3; released
toggle_fs; button; K_LALT; pressed; cond; K_ENTER; released
reload_shaders; button; K_LCTRL; pressed; cond; K_S; released
exit; button; K_ESCAPE; released
pt_x; analog; MOUSE_X; 1.0f
pt_y; analog; MOUSE_Y; 1.0f
pt_down; button; MOUSE_1; pressed)");

    // Gamepad
    inputAddCustomBindings(R"(
move_x; analog; GPAD_LX; 1.0f
move_y; analog; GPAD_LY; 1.0f
look_x; analog; GPAD_RX; 1.0f
look_y; analog; GPAD_RY; 1.0f
reset_view; button; GPAD_Y; released
toggle_ui; button; GPAD_R3; released
dump_profile; button; GPAD_START; pressed; cond; GPAD_B; released)");

    // Touch
#if defined(ENABLE_FORGE_TOUCH_INPUT)
    inputAddCustomBindings(R"(
move_x; analog; VGPAD_LX; 1.0f
move_y; analog; VGPAD_LY; 1.0f
look_x; analog; VGPAD_RX; 0.005f; removedt
look_y; analog; VGPAD_RY; 0.005f; removedt)");
#endif
}

#if defined(ENABLE_FORGE_UI)
#include "../../Application/ThirdParty/OpenSource/imgui/imgui.h"
void InputFillImguiKeyMap(InputEnum* keyMap)
{
    keyMap[ImGuiKey_Tab - ImGuiKey_NamedKey_BEGIN] = K_TAB;
    keyMap[ImGuiKey_LeftArrow - ImGuiKey_NamedKey_BEGIN] = K_LEFTARROW;
    keyMap[ImGuiKey_RightArrow - ImGuiKey_NamedKey_BEGIN] = K_RIGHTARROW;
    keyMap[ImGuiKey_UpArrow - ImGuiKey_NamedKey_BEGIN] = K_UPARROW;
    keyMap[ImGuiKey_DownArrow - ImGuiKey_NamedKey_BEGIN] = K_DOWNARROW;
    keyMap[ImGuiKey_PageUp - ImGuiKey_NamedKey_BEGIN] = K_PGUP;
    keyMap[ImGuiKey_PageDown - ImGuiKey_NamedKey_BEGIN] = K_PGDN;
    keyMap[ImGuiKey_Home - ImGuiKey_NamedKey_BEGIN] = K_HOME;
    keyMap[ImGuiKey_End - ImGuiKey_NamedKey_BEGIN] = K_END;
    keyMap[ImGuiKey_Delete - ImGuiKey_NamedKey_BEGIN] = K_DEL;
    keyMap[ImGuiKey_Space - ImGuiKey_NamedKey_BEGIN] = K_SPACE;
    keyMap[ImGuiKey_Backspace - ImGuiKey_NamedKey_BEGIN] = K_BACKSPACE;
    keyMap[ImGuiKey_Enter - ImGuiKey_NamedKey_BEGIN] = K_ENTER;
    keyMap[ImGuiKey_Escape - ImGuiKey_NamedKey_BEGIN] = K_ESCAPE;

    keyMap[ImGuiKey_GamepadStart - ImGuiKey_NamedKey_BEGIN] = GPAD_START;
    keyMap[ImGuiKey_GamepadBack - ImGuiKey_NamedKey_BEGIN] = GPAD_BACK;
    keyMap[ImGuiKey_GamepadDpadLeft - ImGuiKey_NamedKey_BEGIN] = GPAD_LEFT;
    keyMap[ImGuiKey_GamepadDpadRight - ImGuiKey_NamedKey_BEGIN] = GPAD_RIGHT;
    keyMap[ImGuiKey_GamepadDpadUp - ImGuiKey_NamedKey_BEGIN] = GPAD_UP;
    keyMap[ImGuiKey_GamepadDpadDown - ImGuiKey_NamedKey_BEGIN] = GPAD_DOWN;
    keyMap[ImGuiKey_GamepadFaceLeft - ImGuiKey_NamedKey_BEGIN] = GPAD_X;
    keyMap[ImGuiKey_GamepadFaceRight - ImGuiKey_NamedKey_BEGIN] = GPAD_B;
    keyMap[ImGuiKey_GamepadFaceUp - ImGuiKey_NamedKey_BEGIN] = GPAD_Y;
    keyMap[ImGuiKey_GamepadFaceDown - ImGuiKey_NamedKey_BEGIN] = GPAD_A;
    keyMap[ImGuiKey_GamepadL1 - ImGuiKey_NamedKey_BEGIN] = GPAD_L1;
    keyMap[ImGuiKey_GamepadR1 - ImGuiKey_NamedKey_BEGIN] = GPAD_R1;
    keyMap[ImGuiKey_GamepadL2 - ImGuiKey_NamedKey_BEGIN] = GPAD_L2;
    keyMap[ImGuiKey_GamepadR2 - ImGuiKey_NamedKey_BEGIN] = GPAD_R2;
    keyMap[ImGuiKey_GamepadL3 - ImGuiKey_NamedKey_BEGIN] = GPAD_L3;
    keyMap[ImGuiKey_GamepadR3 - ImGuiKey_NamedKey_BEGIN] = GPAD_R3;

    keyMap[ImGuiKey_MouseLeft - ImGuiKey_NamedKey_BEGIN] = MOUSE_1;
    keyMap[ImGuiKey_MouseRight - ImGuiKey_NamedKey_BEGIN] = MOUSE_2;
    keyMap[ImGuiKey_MouseMiddle - ImGuiKey_NamedKey_BEGIN] = MOUSE_3;
    keyMap[ImGuiKey_MouseX1 - ImGuiKey_NamedKey_BEGIN] = MOUSE_X;
    keyMap[ImGuiKey_MouseX2 - ImGuiKey_NamedKey_BEGIN] = MOUSE_Y;
    keyMap[ImGuiKey_MouseWheelX - ImGuiKey_NamedKey_BEGIN] = MOUSE_WHEEL_UP;
    keyMap[ImGuiKey_MouseWheelY - ImGuiKey_NamedKey_BEGIN] = MOUSE_WHEEL_DOWN;
}
#endif
/************************************************************************/
// IInput implementation
/************************************************************************/
float inputGetValue(InputPortIndex index, InputEnum btn)
{
    if (btn < GPAD_BTN_FIRST)
    {
        return gInputValues[btn];
    }
    else if (btn <= GPAD_BTN_LAST)
    {
        ASSERT(index >= 0 && index < (InputPortIndex)MAX_GAMEPADS);
        const Gamepad& gpad = gGamepads[index];
        return gpad.mButtons[btn - GPAD_BTN_FIRST] ? 1.0f : 0.0f;
    }
    else if (btn <= GPAD_AXIS_LAST)
    {
        ASSERT(index >= 0 && index < (InputPortIndex)MAX_GAMEPADS);
        const Gamepad& gpad = gGamepads[index];
        return gpad.mAxis[btn - GPAD_AXIS_FIRST];
    }
    else if (btn <= CUSTOM_BINDING_LAST)
    {
        ASSERT(gCustomInputs[btn - CUSTOM_BINDING_FIRST].mValid);
        const InputCustomBinding& desc = gCustomInputs[btn - CUSTOM_BINDING_FIRST];
        float                     value = 0.0f;
        for (uint32_t e = 0; e < desc.mCount; ++e)
        {
            const InputCustomBindingElement& elem = desc.mBindings[e];
            // Check if condition met
            if (elem.mCondInput != INPUT_NONE)
            {
                const bool condMet =
                    elem.mCondReleased ? inputGetValueReset(index, elem.mCondInput) : (bool)inputGetValue(index, elem.mCondInput);
                if (!condMet)
                {
                    continue;
                }
            }
            float bindingValue =
                (float)(elem.mReleased ? inputGetValueReset(index, elem.mInput) : inputGetValue(index, elem.mInput)) * elem.mMultiplier;
            if (elem.mRemoveDt)
            {
                bindingValue /= max(0.000001f, gDeltaTime);
            }
            value += bindingValue;
        }
        return value;
    }

    return 0.0f;
}

float inputGetLastValue(InputPortIndex index, InputEnum btn)
{
    if (btn < GPAD_BTN_FIRST)
    {
        return gLastInputValues[btn];
    }
    else if (btn <= GPAD_BTN_LAST)
    {
        ASSERT(index >= 0 && index < (InputPortIndex)MAX_GAMEPADS);
        const Gamepad& gpad = gGamepads[index];
        return gpad.mLastButtons[btn - GPAD_BTN_FIRST] ? 1.0f : 0.0f;
    }
    else if (btn <= GPAD_AXIS_LAST)
    {
        ASSERT(index >= 0 && index < (InputPortIndex)MAX_GAMEPADS);
        const Gamepad& gpad = gGamepads[index];
        return gpad.mLastAxis[btn - GPAD_AXIS_FIRST];
    }

    return 0.0f;
}

void inputGamepadSetAddedCallback(GamepadCallback cb)
{
    // Call after detecting new controller
    gGamepadAddedCb = cb;
}

void inputGamepadSetRemovedCallback(GamepadCallback cb)
{
    // Call after detecting controller removed
    gGamepadRemovedCb = cb;
}

bool inputGamepadIsActive(InputPortIndex index)
{
    ASSERT(index >= 0 && index < (InputPortIndex)MAX_GAMEPADS);
    Gamepad& gpad = gGamepads[index];
    return gpad.mActive;
}

const char* inputGamepadName(InputPortIndex index)
{
    ASSERT(index >= 0 && index < (InputPortIndex)MAX_GAMEPADS);
    Gamepad& gpad = gGamepads[index];
    return gpad.pName;
}

void inputGamepadSetDeadzone(InputPortIndex index, InputEnum btn, float minDeadzone, float maxDeadzone)
{
    ASSERT(index >= 0 && index < (InputPortIndex)MAX_GAMEPADS);
    ASSERT(btn >= GPAD_AXIS_FIRST && btn <= GPAD_AXIS_LAST);
    Gamepad& gpad = gGamepads[index];
    gpad.mDeadzones[btn - GPAD_AXIS_FIRST][0] = minDeadzone;
    gpad.mDeadzones[btn - GPAD_AXIS_FIRST][1] = maxDeadzone;
}

void inputSetEffect(InputPortIndex index, InputEffect effect, const InputEffectValue& value)
{
    switch (effect)
    {
    case INPUT_EFFECT_GPAD_RUMBLE_LOW:
    {
        ASSERT(index >= 0 && index < (InputPortIndex)MAX_GAMEPADS);
        Gamepad& gpad = gGamepads[index];
        gpad.mRumbleLow = value.mRumble;
        if (value.mRumble != 0.0f)
        {
            gpad.mRumbleStopped = false;
        }
        break;
    }
    case INPUT_EFFECT_GPAD_RUMBLE_HIGH:
    {
        ASSERT(index >= 0 && index < (InputPortIndex)MAX_GAMEPADS);
        Gamepad& gpad = gGamepads[index];
        gpad.mRumbleHigh = value.mRumble;
        if (value.mRumble != 0.0f)
        {
            gpad.mRumbleStopped = false;
        }
        break;
    }
    case INPUT_EFFECT_GPAD_LIGHT:
    {
        ASSERT(index >= 0 && index < (InputPortIndex)MAX_GAMEPADS);
        Gamepad& gpad = gGamepads[index];
        gpad.mLight = value.mLight;
        gpad.mLightUpdate = true;
        gpad.mLightReset = false;
        break;
    }
    case INPUT_EFFECT_GPAD_LIGHT_RESET:
    {
        ASSERT(index >= 0 && index < (InputPortIndex)MAX_GAMEPADS);
        Gamepad& gpad = gGamepads[index];
        gpad.mLightUpdate = false;
        gpad.mLightReset = true;
        break;
    }
    default:
        break;
    }
}

void inputGetCharInput(char32_t** pOutChars, uint32_t* pCount)
{
    *pOutChars = gCharacterBuffer;
    *pCount = gCharacterBufferCount;
}
/************************************************************************/
// Custom input bindings
/************************************************************************/
// Internal function which also return hardware specific enum by name
// IInput function will only return custom user defined enums
static InputEnum InputGetEnum(const char* name)
{
#define NAME_COMP_ACTION(x)                             \
    if (!strncmp(name, #x, INPUT_ENUM_NAME_LENGTH_MAX)) \
    {                                                   \
        return x;                                       \
    }
    DECL_INPUTS(NAME_COMP_ACTION)

    return inputGetCustomBindingEnum(name);
}

static InputCustomBinding* InputGetCustomBinding(const char* name)
{
    for (uint32_t b = 0; b < TF_ARRAY_COUNT(gCustomInputs); ++b)
    {
        if (!gCustomInputs[b].mValid)
        {
            continue;
        }
        if (!strncmp(gCustomInputs[b].mName, name, INPUT_ENUM_NAME_LENGTH_MAX))
        {
            return &gCustomInputs[b];
        }
    }

    for (uint32_t i = 0; i < TF_ARRAY_COUNT(gCustomInputs); ++i)
    {
        if (gCustomInputs[i].mValid)
        {
            continue;
        }
        InputCustomBinding& customInput = gCustomInputs[i];
        customInput.mValid = true;
        strncpy(customInput.mName, name, INPUT_ENUM_NAME_LENGTH_MAX);
        return &customInput;
    }

    return NULL;
}

void inputRemoveCustomBinding(InputEnum binding)
{
    ASSERT(binding >= CUSTOM_BINDING_FIRST && binding <= CUSTOM_BINDING_LAST);
    gCustomInputs[binding - CUSTOM_BINDING_FIRST].mValid = false;
}

void inputAddCustomBindings(const char* bindings)
{
    char  line[FS_MAX_PATH] = {};
    char* fileCursor = (char*)bindings;
    char* gGpuDataFileEnd = (char*)bindings + strlen(bindings);

    char  tokensData[12][256] = {};
    char* tokens[TF_ARRAY_COUNT(tokensData)] = {};
    for (uint32_t t = 0; t < TF_ARRAY_COUNT(tokensData); ++t)
    {
        tokens[t] = tokensData[t];
    }

    while (bufferedGetLine(line, &fileCursor, gGpuDataFileEnd))
    {
        uint32_t tokenCount = tokenizeLine(line, line + strlen(line), ";", 256, 8, tokens);
        if (!tokenCount)
        {
            continue;
        }
        const char*         bindingName = tokens[0];
        InputCustomBinding* binding = InputGetCustomBinding(bindingName);
        ASSERT(binding);
        InputCustomBindingElement* elem = binding->mBindings + binding->mCount;
        uint32_t                   elemCount = 0;
        uint32_t                   tokensRead = 0;

        const char* bindingType = tokens[1];
        if (!stricmp(bindingType, "buttonanalog"))
        {
            elemCount = 2;
            ASSERT(binding->mCount + elemCount <= TF_ARRAY_COUNT(binding->mBindings));
            binding->mCount += elemCount;

            //   1    2     3    4     5     6
            // name; axis; K_D; 1.0f; K_A; -1.0f
            ASSERT(tokenCount >= 6);
            tokensRead = 6;
            const char* input0 = tokens[2];
            const char* mul0 = tokens[3];
            const char* input1 = tokens[4];
            const char* mul1 = tokens[5];
            elem[0].mInput = InputGetEnum(input0);
            elem[0].mMultiplier = (float)atof(mul0);
            elem[1].mInput = InputGetEnum(input1);
            elem[1].mMultiplier = (float)atof(mul1);
        }
        else if (!stricmp(bindingType, "button"))
        {
            elemCount = 1;
            ASSERT(binding->mCount + elemCount <= TF_ARRAY_COUNT(binding->mBindings));
            binding->mCount += elemCount;

            //   1      2     3      4
            // name; button; K_D; pressed
            ASSERT(tokenCount >= 4);
            tokensRead = 4;
            const char* input = tokens[2];
            const char* phase = tokens[3];
            elem[0].mInput = InputGetEnum(input);
            elem[0].mMultiplier = 1.0f;
            elem[0].mReleased = !stricmp(phase, "released") ? true : false;
        }
        else if (!stricmp(bindingType, "analog"))
        {
            elemCount = 1;
            ASSERT(binding->mCount + elemCount <= TF_ARRAY_COUNT(binding->mBindings));
            binding->mCount += elemCount;

            //   1      2     3       4
            // name; stick; GPAD_LX; 1.0f
            ASSERT(tokenCount >= 4);
            tokensRead = 4;
            const char* input = tokens[2];
            const char* mul = tokens[3];
            elem[0].mInput = InputGetEnum(input);
            elem[0].mMultiplier = (float)atof(mul);
            elem[0].mReleased = false;
        }

        for (uint32_t e = 0; e < elemCount; ++e)
        {
            ASSERT(elem[e].mInput != INPUT_NONE);
        }

        for (uint32_t t = tokensRead; t < tokenCount; ++t)
        {
            if (!stricmp(tokens[t], "cond"))
            {
                const char* condInput = tokens[++t];
                const char* condPhase = tokens[++t];
                for (uint32_t e = 0; e < elemCount; ++e)
                {
                    elem[e].mCondInput = InputGetEnum(condInput);
                    ASSERT(elem[e].mCondInput != INPUT_NONE);
                    elem[e].mCondReleased = !stricmp(condPhase, "released") ? true : false;
                }
            }
            else if (!stricmp(tokens[t], "removedt"))
            {
                for (uint32_t e = 0; e < elemCount; ++e)
                {
                    elem[e].mRemoveDt = true;
                }
            }
        }
    }
}

InputEnum inputGetCustomBindingEnum(const char* name)
{
    for (uint32_t b = 0; b < TF_ARRAY_COUNT(gCustomInputs); ++b)
    {
        if (!gCustomInputs[b].mValid)
        {
            continue;
        }
        if (!strcmp(gCustomInputs[b].mName, name))
        {
            return (InputEnum)(CUSTOM_BINDING_FIRST + b);
        }
    }

    return INPUT_NONE;
}
