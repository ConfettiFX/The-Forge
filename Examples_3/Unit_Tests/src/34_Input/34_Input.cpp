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

// Unit Test for Input.

// Interfaces
#include "../../../../Common_3/Application/Interfaces/IApp.h"
#include "../../../../Common_3/Application/Interfaces/ICameraController.h"
#include "../../../../Common_3/Application/Interfaces/IFont.h"
#include "../../../../Common_3/OS/Interfaces/IInput.h"
#include "../../../../Common_3/Application/Interfaces/IProfiler.h"
#include "../../../../Common_3/Application/Interfaces/IScreenshot.h"
#include "../../../../Common_3/Application/Interfaces/IUI.h"
#include "../../../../Common_3/Game/Interfaces/IScripting.h"
#include "../../../../Common_3/Utilities/Interfaces/IFileSystem.h"
#include "../../../../Common_3/Utilities/Interfaces/ILog.h"
#include "../../../../Common_3/Utilities/Interfaces/ITime.h"

// Renderer
#include "../../../../Common_3/Graphics/Interfaces/IGraphics.h"
#include "../../../../Common_3/Resources/ResourceLoader/Interfaces/IResourceLoader.h"

#include "../../../../Common_3/Utilities/RingBuffer.h"

// Math
#include "../../../../Common_3/Utilities/Math/MathTypes.h"

#include "../../../../Common_3/Utilities/Interfaces/IMemory.h"

// Button bit definitions
#include "ButtonDefs.h"

/************************************************************************/
// Keep in sync with Common_3/OS/Input/InputCommon.h
/************************************************************************/
// clang-format off
#define DECL_INPUTS(action)      \
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

#define VAR_ACTION(x) static InputEnum x = {};

DECL_INPUTS(VAR_ACTION)

#if defined(ENABLE_FORGE_TOUCH_INPUT)
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
extern InputTouchEventCallback pCustomTouchEventFn;
#endif
/************************************************************************/
/************************************************************************/
struct Vertex
{
    float4 mPosition;
    float2 mTexcoord;
};

struct ConstantData
{
    float2 wndSize;

    float2 leftAxis = float2(0.0f, 0.0f);
    float2 rightAxis = float2(0.0f, 0.0f);

    // Gamepad specific
    float2 motionButtons = float2(0.0f, 0.0f);

    uint32_t deviceType = 0; // 0.Invalid 1.Gamepad 2.Touch 3&4.Keyboard+Mouse
    uint32_t buttonSet0 = 0;
    uint32_t buttonSet1 = 0;
    uint32_t buttonSet2 = 0;
    uint32_t buttonSet3 = 0;
};

ConstantData   gConstantData;
// For UI purposes, a custom variable to remove 'invalid' device type - 0.Gamepad 1.KBM 2.Touch
uint32_t       gChosenDeviceType = 0;
InputPortIndex gGamepadIndex = 0;
char           gGamepadNames[MAX_GAMEPADS][FS_MAX_PATH] = {};
bool           gEnableRumbleAndLights = false;
#if defined(ENABLE_FORGE_TOUCH_INPUT)
TouchEvent gTouches[16] = {};
#endif

#define ARRAY_LENGTH(a) (sizeof(a) / sizeof(a[0]))

// #NOTE: Two sets of resources (one in flight and one being used on CPU)
const uint32_t gDataBufferCount = 2;

Renderer* pRenderer = NULL;

Queue*     pGraphicsQueue = NULL;
GpuCmdRing gGraphicsCmdRing = {};

SwapChain* pSwapChain = NULL;
Semaphore* pImageAcquiredSemaphore = NULL;

Shader*   pBasicShader = NULL;
Pipeline* pBasicPipeline = NULL;

Buffer* pQuadVertexBuffer = NULL;
Buffer* pQuadIndexBuffer = NULL;

RootSignature* pRootSignature = NULL;
Sampler*       pSampler = NULL;
Texture*       pGamepad = NULL;
Texture*       pKeyboardMouse = NULL;
Texture*       pInputDeviceTexture = NULL;
Texture*       pPrevInputDeviceTexture = NULL;
DescriptorSet* pDescriptorDevice = NULL;
DescriptorSet* pDescriptorInputData = NULL;

Buffer* pInputDataUniformBuffer[gDataBufferCount] = { NULL };

uint32_t     gFrameIndex = 0;
ProfileToken gGpuProfileToken = PROFILE_INVALID_TOKEN;

/// UI
UIComponent* pGuiWindow = NULL;

uint32_t gFontID = 0;

FontDrawDesc gFrameTimeDraw = FontDrawDesc{ NULL, 0, 0xff00ffff, 18 };

Vertex gQuadVerts[4];
///////////////////////////////////////////////////////////////////////////

#define SET_BUTTON_BIT(set, state, button_bit) ((set) = (state) ? ((set) | (button_bit)) : ((set) & ~(button_bit)));

bool InputChecker()
{
    switch (gConstantData.deviceType)
    {
    case INPUT_DEVICE_GAMEPAD:
    {
        gConstantData.leftAxis.x = inputGetValue(gGamepadIndex, GPAD_LX);
        gConstantData.leftAxis.y = -inputGetValue(gGamepadIndex, GPAD_LY);
        gConstantData.rightAxis.x = inputGetValue(gGamepadIndex, GPAD_RX);
        gConstantData.rightAxis.y = -inputGetValue(gGamepadIndex, GPAD_RY);
        gConstantData.motionButtons.x = inputGetValue(gGamepadIndex, GPAD_L2);
        gConstantData.motionButtons.y = inputGetValue(gGamepadIndex, GPAD_R2);
        SET_BUTTON_BIT(gConstantData.buttonSet0, inputGetValue(gGamepadIndex, GPAD_LEFT), CONTROLLER_DPAD_LEFT_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet0, inputGetValue(gGamepadIndex, GPAD_RIGHT), CONTROLLER_DPAD_RIGHT_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet0, inputGetValue(gGamepadIndex, GPAD_UP), CONTROLLER_DPAD_UP_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet0, inputGetValue(gGamepadIndex, GPAD_DOWN), CONTROLLER_DPAD_DOWN_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet0, inputGetValue(gGamepadIndex, GPAD_A), CONTROLLER_A_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet0, inputGetValue(gGamepadIndex, GPAD_B), CONTROLLER_B_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet0, inputGetValue(gGamepadIndex, GPAD_X), CONTROLLER_X_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet0, inputGetValue(gGamepadIndex, GPAD_Y), CONTROLLER_Y_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet0, inputGetValue(gGamepadIndex, GPAD_L1), CONTROLLER_L1_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet0, inputGetValue(gGamepadIndex, GPAD_R1), CONTROLLER_R1_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet0, inputGetValue(gGamepadIndex, GPAD_L3), CONTROLLER_L3_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet0, inputGetValue(gGamepadIndex, GPAD_R3), CONTROLLER_R3_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet0, inputGetValue(gGamepadIndex, GPAD_START), CONTROLLER_START_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet0, inputGetValue(gGamepadIndex, GPAD_BACK), CONTROLLER_SELECT_BIT);
        break;
    }
    case INPUT_DEVICE_KBM:
    {
        SET_BUTTON_BIT(gConstantData.buttonSet0, inputGetValue(0, K_ESCAPE), ESCAPE_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet0, inputGetValue(0, K_F1), F1_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet0, inputGetValue(0, K_F2), F2_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet0, inputGetValue(0, K_F3), F3_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet0, inputGetValue(0, K_F4), F4_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet0, inputGetValue(0, K_F5), F5_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet0, inputGetValue(0, K_F6), F6_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet0, inputGetValue(0, K_F7), F7_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet0, inputGetValue(0, K_F8), F8_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet0, inputGetValue(0, K_F9), F9_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet0, inputGetValue(0, K_F10), F10_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet0, inputGetValue(0, K_F11), F11_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet0, inputGetValue(0, K_F12), F12_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet0, inputGetValue(0, K_INS), INSERT_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet0, inputGetValue(0, K_DEL), DEL_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet0, inputGetValue(0, K_KP_NUMLOCK), NUM_LOCK_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet0, inputGetValue(0, K_KP_STAR), KP_MULTIPLY_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet0, inputGetValue(0, K_1), NUM_1_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet0, inputGetValue(0, K_2), NUM_2_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet0, inputGetValue(0, K_3), NUM_3_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet0, inputGetValue(0, K_4), NUM_4_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet0, inputGetValue(0, K_5), NUM_5_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet0, inputGetValue(0, K_6), NUM_6_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet0, inputGetValue(0, K_7), NUM_7_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet0, inputGetValue(0, K_8), NUM_8_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet0, inputGetValue(0, K_9), NUM_9_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet0, inputGetValue(0, K_0), NUM_0_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet0, inputGetValue(0, K_MINUS), MINUS_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet0, inputGetValue(0, K_EQUAL), EQUAL_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet0, inputGetValue(0, K_BACKSPACE), BACK_SPACE_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet1, inputGetValue(0, K_KP_PLUS), KP_ADD_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet1, inputGetValue(0, K_KP_MINUS), KP_SUBTRACT_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet1, inputGetValue(0, K_TAB), TAB_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet1, inputGetValue(0, K_Q), Q_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet1, inputGetValue(0, K_W), W_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet1, inputGetValue(0, K_E), E_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet1, inputGetValue(0, K_R), R_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet1, inputGetValue(0, K_T), T_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet1, inputGetValue(0, K_Y), Y_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet1, inputGetValue(0, K_U), U_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet1, inputGetValue(0, K_I), I_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet1, inputGetValue(0, K_O), O_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet1, inputGetValue(0, K_P), P_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet1, inputGetValue(0, K_LEFTBRACKET), BRACKET_LEFT_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet1, inputGetValue(0, K_RIGHTBRACKET), BRACKET_RIGHT_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet1, inputGetValue(0, K_BACKSLASH), BACK_SLASH_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet1, inputGetValue(0, K_KP_HOME), KP_7_HOME_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet1, inputGetValue(0, K_KP_UPARROW), KP_8_UP_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet1, inputGetValue(0, K_KP_PGUP), KP_9_PAGE_UP_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet1, inputGetValue(0, K_CAPSLOCK), CAPS_LOCK_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet1, inputGetValue(0, K_A), A_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet1, inputGetValue(0, K_S), S_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet1, inputGetValue(0, K_D), D_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet1, inputGetValue(0, K_F), F_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet1, inputGetValue(0, K_G), G_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet1, inputGetValue(0, K_H), H_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet1, inputGetValue(0, K_J), J_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet1, inputGetValue(0, K_K), K_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet1, inputGetValue(0, K_L), L_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet1, inputGetValue(0, K_SEMICOLON), SEMICOLON_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet1, inputGetValue(0, K_APOSTROPHE), APOSTROPHE_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet1, inputGetValue(0, K_ENTER), ENTER_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet2, inputGetValue(0, K_KP_LEFTARROW), KP_4_LEFT_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet2, inputGetValue(0, K_KP_RIGHTARROW), KP_6_RIGHT_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet2, inputGetValue(0, K_LSHIFT), SHIFT_L_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet2, inputGetValue(0, K_Z), Z_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet2, inputGetValue(0, K_X), X_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet2, inputGetValue(0, K_C), C_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet2, inputGetValue(0, K_V), V_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet2, inputGetValue(0, K_B), B_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet2, inputGetValue(0, K_N), N_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet2, inputGetValue(0, K_M), M_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet2, inputGetValue(0, K_COMMA), COMMA_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet2, inputGetValue(0, K_PERIOD), PERIOD_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet2, inputGetValue(0, K_SLASH), FWRD_SLASH_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet2, inputGetValue(0, K_RSHIFT), SHIFT_R_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet2, inputGetValue(0, K_KP_END), KP_1_END_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet2, inputGetValue(0, K_KP_DOWNARROW), KP_2_DOWN_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet2, inputGetValue(0, K_KP_PGDN), KP_3_PAGE_DOWN_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet2, inputGetValue(0, K_LCTRL), CTRL_L_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet2, inputGetValue(0, K_LALT), ALT_L_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet2, inputGetValue(0, K_SPACE), SPACE_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet2, inputGetValue(0, K_RALT), ALT_R_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet2, inputGetValue(0, K_RCTRL), CTRL_R_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet2, inputGetValue(0, K_LEFTARROW), LEFT_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet2, inputGetValue(0, K_RIGHTARROW), RIGHT_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet2, inputGetValue(0, K_UPARROW), UP_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet2, inputGetValue(0, K_DOWNARROW), DOWN_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet2, inputGetValue(0, K_KP_INS), KP_0_INSERT_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet2, inputGetValue(0, K_KP_NUMPAD_5), KP_5_BEGIN_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet2, inputGetValue(0, MOUSE_1), LEFT_CLICK_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet2, inputGetValue(0, MOUSE_2), RIGHT_CLICK_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet2, inputGetValue(0, MOUSE_3), MID_CLICK_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet2, inputGetValue(0, MOUSE_WHEEL_UP), SCROLL_UP_BIT);
        SET_BUTTON_BIT(gConstantData.buttonSet3, inputGetValue(0, MOUSE_WHEEL_DOWN), SCROLL_DOWN_BIT);
        // SET_BUTTON_BIT(gConstantData.buttonSet0, ctx->mBool, BREAK_BIT);
        // SET_BUTTON_BIT(gConstantData.buttonSet0, ctx->mBool, ACUTE_BIT);
        break;
    }
#if defined(ENABLE_FORGE_TOUCH_INPUT)
    case INPUT_DEVICE_TOUCH:
        break;
#endif
    }

    return true;
};

IApp* gUnitTestApp = NULL;

void OnDeviceSwitch(void* pUserData)
{
    UNREF_PARAM(pUserData);
    waitQueueIdle(pGraphicsQueue);
    gConstantData.deviceType = gChosenDeviceType;

    switch (gConstantData.deviceType)
    {
    case INPUT_DEVICE_GAMEPAD:
    {
        pInputDeviceTexture = pGamepad;
        break;
    }
    case INPUT_DEVICE_KBM:
    {
        pInputDeviceTexture = pKeyboardMouse;
        break;
    }
#if defined(ENABLE_FORGE_TOUCH_INPUT)
    case INPUT_DEVICE_TOUCH:
    {
        break;
    }
#endif
    }
};

void OnLightsAndRumbleSwitch(void* pUserData)
{
    UNREF_PARAM(pUserData);
    // Reset controller events
    InputEffectValue effectValue = {};
    inputSetEffect(gGamepadIndex, INPUT_EFFECT_GPAD_RUMBLE_LOW, effectValue);
    inputSetEffect(gGamepadIndex, INPUT_EFFECT_GPAD_RUMBLE_HIGH, effectValue);
    inputSetEffect(gGamepadIndex, INPUT_EFFECT_GPAD_LIGHT_RESET, effectValue);
}

void DoRumbleAndLightsTestSequence(float deltaTime)
{
    static float total = 0.0f;
    const float  threshold = 1.0f;
    total += deltaTime;
    bool set = total >= threshold;
    if (set)
    {
        total = 0.0f;
        InputEffectValue effectValue = {};
        effectValue.mRumble = randomFloat(0.0f, 1.0f);
        inputSetEffect(gGamepadIndex, INPUT_EFFECT_GPAD_RUMBLE_LOW, effectValue);
        effectValue.mRumble = randomFloat(0.0f, 0.5f);
        inputSetEffect(gGamepadIndex, INPUT_EFFECT_GPAD_RUMBLE_HIGH, effectValue);
        effectValue.mLight = { randomFloat(0.0f, 1.0f), randomFloat(0.0f, 1.0f), randomFloat(0.0f, 1.0f) };
        inputSetEffect(gGamepadIndex, INPUT_EFFECT_GPAD_LIGHT, effectValue);
    }
}

#if defined(ENABLE_FORGE_TOUCH_INPUT)
static void OnTouchEvent(const TouchEvent* event, void* pData)
{
    UNREF_PARAM(pData);
    if (TOUCH_BEGAN == event->mPhase)
    {
        for (uint32_t t = 0; t < TF_ARRAY_COUNT(gTouches); ++t)
        {
            if (TOUCH_ID_INVALID == gTouches[t].mId)
            {
                gTouches[t] = *event;
                break;
            }
        }
    }
    else if (TOUCH_MOVED == event->mPhase)
    {
        for (uint32_t t = 0; t < TF_ARRAY_COUNT(gTouches); ++t)
        {
            if (event->mId == gTouches[t].mId)
            {
                gTouches[t] = *event;
                break;
            }
        }
    }
    else
    {
        for (uint32_t t = 0; t < TF_ARRAY_COUNT(gTouches); ++t)
        {
            if (event->mId == gTouches[t].mId)
            {
                gTouches[t].mId = TOUCH_ID_INVALID;
                break;
            }
        }
    }
}
#endif

class Input: public IApp
{
public:
    Input()
    {
        gUnitTestApp = this;

        mSettings.mCentered = true;
        mSettings.mBorderlessWindow = false;
        mSettings.mForceLowDPI = true;

        gConstantData.deviceType = gChosenDeviceType;
    }

    bool Init()
    {
        // window and renderer setup
        RendererDesc settings;
        memset(&settings, 0, sizeof(settings));
        initGPUConfiguration(settings.pExtendedSettings);
        initRenderer(GetName(), &settings, &pRenderer);
        // check for init success
        if (!pRenderer)
        {
            ShowUnsupportedMessage("Failed To Initialize renderer!");
            return false;
        }
        setupGPUConfigurationPlatformParameters(pRenderer, settings.pExtendedSettings);

        QueueDesc queueDesc = {};
        queueDesc.mType = QUEUE_TYPE_GRAPHICS;
        queueDesc.mFlag = QUEUE_FLAG_INIT_MICROPROFILE;
        initQueue(pRenderer, &queueDesc, &pGraphicsQueue);

        GpuCmdRingDesc cmdRingDesc = {};
        cmdRingDesc.pQueue = pGraphicsQueue;
        cmdRingDesc.mPoolCount = gDataBufferCount;
        cmdRingDesc.mCmdPerPoolCount = 1;
        cmdRingDesc.mAddSyncPrimitives = true;
        initGpuCmdRing(pRenderer, &cmdRingDesc, &gGraphicsCmdRing);

        initSemaphore(pRenderer, &pImageAcquiredSemaphore);

        initResourceLoaderInterface(pRenderer);

        TextureLoadDesc textureDesc = {};
        textureDesc.pFileName = "input/controller.tex";
        textureDesc.ppTexture = &pGamepad;
        addResource(&textureDesc, NULL);

        textureDesc.pFileName = "input/keyboard+mouse.tex";
        textureDesc.ppTexture = &pKeyboardMouse;
        addResource(&textureDesc, NULL);

        SamplerDesc samplerDesc = {};
        samplerDesc.mAddressU = ADDRESS_MODE_CLAMP_TO_BORDER;
        samplerDesc.mAddressV = ADDRESS_MODE_CLAMP_TO_BORDER;
        samplerDesc.mAddressW = ADDRESS_MODE_CLAMP_TO_BORDER;
        samplerDesc.mMinFilter = FILTER_LINEAR;
        samplerDesc.mMagFilter = FILTER_LINEAR;
        samplerDesc.mMipMapMode = MIPMAP_MODE_LINEAR;
        addSampler(pRenderer, &samplerDesc, &pSampler);

        BufferLoadDesc bufferDesc = {};
        bufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        bufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
        bufferDesc.mDesc.mSize = sizeof(ConstantData);
        bufferDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
        bufferDesc.pData = NULL;
        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            bufferDesc.ppBuffer = &pInputDataUniformBuffer[i];
            addResource(&bufferDesc, NULL);
        }

        // Load fonts
        FontDesc font = {};
        font.pFontPath = "TitilliumText/TitilliumText-Bold.otf";
        fntDefineFonts(&font, 1, &gFontID);

        FontSystemDesc fontRenderDesc = {};
        fontRenderDesc.pRenderer = pRenderer;
        if (!initFontSystem(&fontRenderDesc))
            return false; // report?

        gQuadVerts[0].mPosition = { 1.0f, 1.0f, 0.0f, 1.0f };
        gQuadVerts[0].mTexcoord = { 1.0f, 0.0f };
        gQuadVerts[1].mPosition = { -1.0f, 1.0f, 0.0f, 1.0f };
        gQuadVerts[1].mTexcoord = { 0.0f, 0.0f };
        gQuadVerts[2].mPosition = { -1.0f, -1.0f, 0.0f, 1.0f };
        gQuadVerts[2].mTexcoord = { 0.0f, 1.0f };
        gQuadVerts[3].mPosition = { 1.0f, -1.0f, 0.0f, 1.0f };
        gQuadVerts[3].mTexcoord = { 1.0f, 1.0f };

        uint16_t indices[] = { 0, 1, 2, 0, 2, 3 };

        BufferLoadDesc joystickVBDesc = {};
        joystickVBDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
        joystickVBDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
        joystickVBDesc.mDesc.mSize = sizeof(gQuadVerts);
        joystickVBDesc.pData = gQuadVerts;
        joystickVBDesc.ppBuffer = &pQuadVertexBuffer;
        addResource(&joystickVBDesc, NULL);

        BufferLoadDesc ibDesc = {};
        ibDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_INDEX_BUFFER;
        ibDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
        ibDesc.mDesc.mSize = sizeof(indices);
        ibDesc.pData = indices;
        ibDesc.ppBuffer = &pQuadIndexBuffer;
        addResource(&ibDesc, NULL);

        // Initialize Forge User Interface Rendering
        UserInterfaceDesc uiRenderDesc = {};
        uiRenderDesc.pRenderer = pRenderer;
        initUserInterface(&uiRenderDesc);

        // Initialize micro profiler and its UI.
        ProfilerDesc profiler = {};
        profiler.pRenderer = pRenderer;
        initProfiler(&profiler);

        // Gpu profiler can only be added after initProfiler.
        gGpuProfileToken = initGpuProfiler(pRenderer, pGraphicsQueue, "Graphics");

        // Setup action mappings and callbacks
        OnDeviceSwitch(nullptr);

        waitForAllResourceLoads();

#if defined(ENABLE_FORGE_TOUCH_INPUT)
        for (uint32_t t = 0; t < TF_ARRAY_COUNT(gTouches); ++t)
        {
            gTouches[t].mId = TOUCH_ID_INVALID;
        }
        pCustomTouchEventFn = OnTouchEvent;
#endif

        mSettings.mShowPlatformUI = false;

        // Add custom binding for the input
#define BINDING_ACTION(x)                                \
    inputAddCustomBindings(#x "; analog; " #x "; 1.0f"); \
    x = inputGetCustomBindingEnum(#x);
        DECL_INPUTS(BINDING_ACTION)

        initScreenshotInterface(pRenderer, pGraphicsQueue);
        return true;
    }

    void Exit()
    {
        exitScreenshotInterface();
        waitQueueIdle(pGraphicsQueue);

        exitUserInterface();

        exitFontSystem();

        exitProfiler();

        for (uint32_t i = 0; i < gDataBufferCount; i++)
        {
            removeResource(pInputDataUniformBuffer[i]);
        }

        removeResource(pGamepad);
        removeResource(pKeyboardMouse);

        removeResource(pQuadVertexBuffer);
        removeResource(pQuadIndexBuffer);

        removeSampler(pRenderer, pSampler);

        exitGpuCmdRing(pRenderer, &gGraphicsCmdRing);
        exitSemaphore(pRenderer, pImageAcquiredSemaphore);

        exitResourceLoaderInterface(pRenderer);
        exitQueue(pRenderer, pGraphicsQueue);
        exitRenderer(pRenderer);
        exitGPUConfiguration();
        pRenderer = NULL;
    }

    bool Load(ReloadDesc* pReloadDesc)
    {
        if (pReloadDesc->mType & RELOAD_TYPE_SHADER)
        {
            addShaders();
            addRootSignatures();
            addDescriptorSets();
        }

        if (pReloadDesc->mType & (RELOAD_TYPE_RESIZE | RELOAD_TYPE_RENDERTARGET))
        {
            loadProfilerUI(mSettings.mWidth, mSettings.mHeight);

            UIComponentDesc guiDesc = {};
            guiDesc.mStartPosition += vec2(mSettings.mWidth * 0.01f, mSettings.mHeight * 0.2f);
            uiAddComponent(GetName(), &guiDesc, &pGuiWindow);

            static const char* enumNames[] = {
                "Gamepad",
                "Keyboard+Mouse",
#if defined(ENABLE_FORGE_TOUCH_INPUT)
                "Touch",
#endif
            };

            DropdownWidget deviceDropdown;
            deviceDropdown.pData = &gChosenDeviceType;
            deviceDropdown.pNames = enumNames;
            deviceDropdown.mCount = TF_ARRAY_COUNT(enumNames);

            UIWidget* pRunScript = uiAddComponentWidget(pGuiWindow, "Device Type: ", &deviceDropdown, WIDGET_TYPE_DROPDOWN);
            uiSetWidgetOnEditedCallback(pRunScript, nullptr, OnDeviceSwitch);
            luaRegisterWidget(pRunScript);

            CheckboxWidget lightsAndRumbleCheckbox;
            lightsAndRumbleCheckbox.pData = &gEnableRumbleAndLights;

            UIWidget* pLARWidget =
                uiAddComponentWidget(pGuiWindow, "Enable Rumble and Lights Test Sequence:", &lightsAndRumbleCheckbox, WIDGET_TYPE_CHECKBOX);
            uiSetWidgetOnEditedCallback(pLARWidget, nullptr, OnLightsAndRumbleSwitch);
            luaRegisterWidget(pRunScript);

            static const char* gamepadNames[MAX_GAMEPADS] = {};
            for (uint32_t i = 0; i < TF_ARRAY_COUNT(gamepadNames); ++i)
            {
                gamepadNames[i] = gGamepadNames[i];
                snprintf(gGamepadNames[i], TF_ARRAY_COUNT(gGamepadNames[i]), "Gamepad[%u]", i);
            }
            DropdownWidget gamepadDropdown = {};
            gamepadDropdown.mCount = MAX_GAMEPADS;
            gamepadDropdown.pData = (uint32_t*)&gGamepadIndex;
            gamepadDropdown.pNames = gamepadNames;
            uiAddComponentWidget(pGuiWindow, "Gamepad", &gamepadDropdown, WIDGET_TYPE_DROPDOWN);

            if (!addSwapChain())
                return false;
        }

        if (pReloadDesc->mType & (RELOAD_TYPE_SHADER | RELOAD_TYPE_RENDERTARGET))
        {
            addPipelines();
        }

        prepareDescriptorSets();

        UserInterfaceLoadDesc uiLoad = {};
        uiLoad.mColorFormat = pSwapChain->ppRenderTargets[0]->mFormat;
        uiLoad.mHeight = mSettings.mHeight;
        uiLoad.mWidth = mSettings.mWidth;
        uiLoad.mLoadType = pReloadDesc->mType;
        loadUserInterface(&uiLoad);

        FontSystemLoadDesc fontLoad = {};
        fontLoad.mColorFormat = pSwapChain->ppRenderTargets[0]->mFormat;
        fontLoad.mHeight = mSettings.mHeight;
        fontLoad.mWidth = mSettings.mWidth;
        fontLoad.mLoadType = pReloadDesc->mType;
        loadFontSystem(&fontLoad);

        return true;
    }

    void Unload(ReloadDesc* pReloadDesc)
    {
        waitQueueIdle(pGraphicsQueue);

        unloadFontSystem(pReloadDesc->mType);
        unloadUserInterface(pReloadDesc->mType);

        if (pReloadDesc->mType & (RELOAD_TYPE_SHADER | RELOAD_TYPE_RENDERTARGET))
        {
            removePipelines();
        }

        if (pReloadDesc->mType & (RELOAD_TYPE_RESIZE | RELOAD_TYPE_RENDERTARGET))
        {
            removeSwapChain(pRenderer, pSwapChain);
            uiRemoveComponent(pGuiWindow);
            unloadProfilerUI();
        }

        if (pReloadDesc->mType & RELOAD_TYPE_SHADER)
        {
            removeDescriptorSets();
            removeRootSignatures();
            removeShaders();
        }
    }

    void Update(float deltaTime)
    {
        for (uint32_t i = 0; i < TF_ARRAY_COUNT(gGamepadNames); ++i)
        {
            snprintf(gGamepadNames[i], TF_ARRAY_COUNT(gGamepadNames[i]), "Gamepad[%u] %s", i,
                     (inputGamepadIsActive(i) ? inputGamepadName(i) : "Not connected"));
        }

        InputChecker();

        if (pInputDeviceTexture != pPrevInputDeviceTexture)
        {
            DescriptorData param = {};
            param.pName = "uTexture";
            param.ppTextures = &pInputDeviceTexture;
            updateDescriptorSet(pRenderer, 0, pDescriptorDevice, 1, &param);

            pPrevInputDeviceTexture = pInputDeviceTexture;
        }

        if (gEnableRumbleAndLights)
        {
            DoRumbleAndLightsTestSequence(deltaTime);
        }
    }

    void Draw()
    {
        if ((bool)pSwapChain->mEnableVsync != mSettings.mVSyncEnabled)
        {
            waitQueueIdle(pGraphicsQueue);
            ::toggleVSync(pRenderer, &pSwapChain);
        }
        uint32_t swapchainImageIndex;
        acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, NULL, &swapchainImageIndex);

        RenderTarget* pRenderTarget = pSwapChain->ppRenderTargets[swapchainImageIndex];

        // Stall if CPU is running "gDataBufferCount" frames ahead of GPU
        GpuCmdRingElement elem = getNextGpuCmdRingElement(&gGraphicsCmdRing, true, 1);
        FenceStatus       fenceStatus;
        getFenceStatus(pRenderer, elem.pFence, &fenceStatus);
        if (fenceStatus == FENCE_STATUS_INCOMPLETE)
            waitForFences(pRenderer, 1, &elem.pFence);

        gConstantData.wndSize = { pRenderTarget->mWidth, pRenderTarget->mHeight };

        BufferUpdateDesc inputData = { pInputDataUniformBuffer[gFrameIndex] };
        beginUpdateResource(&inputData);
        memcpy(inputData.pMappedData, &gConstantData, sizeof(gConstantData));
        endUpdateResource(&inputData);

        // Reset cmd pool for this frame
        resetCmdPool(pRenderer, elem.pCmdPool);

        Cmd* cmd = elem.pCmds[0];
        beginCmd(cmd);

        cmdBeginGpuFrameProfile(cmd, gGpuProfileToken);

        const uint32_t stride = 6 * sizeof(float);

        RenderTargetBarrier barrier;

        barrier = { pRenderTarget, RESOURCE_STATE_PRESENT, RESOURCE_STATE_RENDER_TARGET };
        cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, &barrier);

        cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Basic Draw");

        BindRenderTargetsDesc bindRenderTargets = {};
        bindRenderTargets.mRenderTargetCount = 1;
        bindRenderTargets.mRenderTargets[0] = { pRenderTarget, LOAD_ACTION_CLEAR };
        cmdBindRenderTargets(cmd, &bindRenderTargets);
        cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mWidth, (float)pRenderTarget->mHeight, 0.0f, 1.0f);
        cmdSetScissor(cmd, 0, 0, pRenderTarget->mWidth, pRenderTarget->mHeight);

        cmdBindPipeline(cmd, pBasicPipeline);
        cmdBindIndexBuffer(cmd, pQuadIndexBuffer, INDEX_TYPE_UINT16, 0);

        cmdBindDescriptorSet(cmd, 0, pDescriptorDevice);
        cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorInputData);
        cmdBindVertexBuffer(cmd, 1, &pQuadVertexBuffer, &stride, NULL);
        cmdDrawIndexed(cmd, 6, 0, 0);

        cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);

        cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Draw UI");

        gFrameTimeDraw.mFontColor = 0xff00ffff;
        gFrameTimeDraw.mFontID = gFontID;

#if defined(ENABLE_FORGE_TOUCH_INPUT)
        float  yTxtOffset = 12.f;
        float  xTxtOffset = 8.f;
        float  yTxtOrig = yTxtOffset;
        float2 txtSizePx = {};
        yTxtOrig += txtSizePx.y + 7 * yTxtOffset;
        yTxtOffset = 32.f;
        txtSizePx.y = 15.0f;
        if (gChosenDeviceType == INPUT_DEVICE_TOUCH)
        {
            for (uint32_t t = 0; t < TF_ARRAY_COUNT(gTouches); ++t)
            {
                const char* phaseNames[] = {
                    "BEGAN",
                    "MOVED",
                    "ENDED",
                    "CANCELED",
                };
                const TouchEvent& event = gTouches[t];
                if (TOUCH_ID_INVALID == event.mId)
                {
                    continue;
                }
                const int textSize = 256;
                char      gestureText[textSize] = { 0 };
                snprintf(gestureText, textSize, "TouchEvent %10d : Pos (%d : %d) Phase (%s)", event.mId, event.mPos[0], event.mPos[1],
                         phaseNames[event.mPhase]);
                gFrameTimeDraw.pText = gestureText;
                cmdDrawTextWithFont(cmd, float2(xTxtOffset, yTxtOrig), &gFrameTimeDraw);
                yTxtOrig += txtSizePx.y + yTxtOffset;
            }
        }
#endif

        cmdDrawUserInterface(cmd);
        cmdBindRenderTargets(cmd, NULL);
        cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);

        barrier = { pRenderTarget, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PRESENT };
        cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, &barrier);

        cmdEndGpuFrameProfile(cmd, gGpuProfileToken);
        endCmd(cmd);

        FlushResourceUpdateDesc flushUpdateDesc = {};
        flushUpdateDesc.mNodeIndex = 0;
        flushResourceUpdates(&flushUpdateDesc);
        Semaphore* waitSemaphores[2] = { flushUpdateDesc.pOutSubmittedSemaphore, pImageAcquiredSemaphore };

        QueueSubmitDesc submitDesc = {};
        submitDesc.mCmdCount = 1;
        submitDesc.mSignalSemaphoreCount = 1;
        submitDesc.mWaitSemaphoreCount = TF_ARRAY_COUNT(waitSemaphores);
        submitDesc.ppCmds = &cmd;
        submitDesc.ppSignalSemaphores = &elem.pSemaphore;
        submitDesc.ppWaitSemaphores = waitSemaphores;
        submitDesc.pSignalFence = elem.pFence;
        queueSubmit(pGraphicsQueue, &submitDesc);

        QueuePresentDesc presentDesc = {};
        presentDesc.mIndex = (uint8_t)swapchainImageIndex;
        presentDesc.mWaitSemaphoreCount = 1;
        presentDesc.pSwapChain = pSwapChain;
        presentDesc.ppWaitSemaphores = &elem.pSemaphore;
        presentDesc.mSubmitDone = true;
        queuePresent(pGraphicsQueue, &presentDesc);

        flipProfiler();

        gFrameIndex = (gFrameIndex + 1) % gDataBufferCount;
    }

    const char* GetName() { return "34_Input"; }

    bool addSwapChain()
    {
        SwapChainDesc swapChainDesc = {};
        swapChainDesc.mWindowHandle = pWindow->handle;
        swapChainDesc.mPresentQueueCount = 1;
        swapChainDesc.ppPresentQueues = &pGraphicsQueue;
        swapChainDesc.mWidth = mSettings.mWidth;
        swapChainDesc.mHeight = mSettings.mHeight;
        swapChainDesc.mImageCount = getRecommendedSwapchainImageCount(pRenderer, &pWindow->handle);
        swapChainDesc.mColorFormat = getSupportedSwapchainFormat(pRenderer, &swapChainDesc, COLOR_SPACE_SDR_SRGB);
        swapChainDesc.mColorSpace = COLOR_SPACE_SDR_SRGB;
        swapChainDesc.mEnableVsync = mSettings.mVSyncEnabled;
        ::addSwapChain(pRenderer, &swapChainDesc, &pSwapChain);

        return pSwapChain != NULL;
    }

    void addDescriptorSets()
    {
        DescriptorSetDesc desc = { pRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, gDataBufferCount * 2 };
        addDescriptorSet(pRenderer, &desc, &pDescriptorDevice);

        desc = { pRootSignature, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gDataBufferCount };
        addDescriptorSet(pRenderer, &desc, &pDescriptorInputData);
    }

    void removeDescriptorSets()
    {
        removeDescriptorSet(pRenderer, pDescriptorDevice);
        removeDescriptorSet(pRenderer, pDescriptorInputData);
    }

    void addRootSignatures()
    {
        Shader*           shaders[] = { pBasicShader };
        const char*       pStaticSamplers[] = { "uSampler" };
        RootSignatureDesc rootDesc = {};
        rootDesc.mStaticSamplerCount = 1;
        rootDesc.ppStaticSamplerNames = pStaticSamplers;
        rootDesc.ppStaticSamplers = &pSampler;
        rootDesc.mShaderCount = ARRAY_LENGTH(shaders);
        rootDesc.ppShaders = shaders;
        addRootSignature(pRenderer, &rootDesc, &pRootSignature);
    }

    void removeRootSignatures() { removeRootSignature(pRenderer, pRootSignature); }

    void addShaders()
    {
        ShaderLoadDesc basicShader = {};
        basicShader.mVert.pFileName = "basic.vert";
        basicShader.mFrag.pFileName = "basic.frag";
        addShader(pRenderer, &basicShader, &pBasicShader);
    }

    void removeShaders() { removeShader(pRenderer, pBasicShader); }

    void addPipelines()
    {
        // layout and pipeline for sphere draw
        VertexLayout vertexLayout = {};
        vertexLayout.mBindingCount = 1;
        vertexLayout.mAttribCount = 2;
        vertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
        vertexLayout.mAttribs[0].mFormat = TinyImageFormat_R32G32B32A32_SFLOAT;
        vertexLayout.mAttribs[0].mBinding = 0;
        vertexLayout.mAttribs[0].mLocation = 0;
        vertexLayout.mAttribs[0].mOffset = 0;
        vertexLayout.mAttribs[1].mSemantic = SEMANTIC_TEXCOORD0;
        vertexLayout.mAttribs[1].mFormat = TinyImageFormat_R32G32_SFLOAT;
        vertexLayout.mAttribs[1].mBinding = 0;
        vertexLayout.mAttribs[1].mLocation = 1;
        vertexLayout.mAttribs[1].mOffset = 4 * sizeof(float);

        RasterizerStateDesc rasterizerStateDesc = {};
        rasterizerStateDesc.mCullMode = CULL_MODE_BACK;

        DepthStateDesc depthStateDesc = {};

        PipelineDesc desc = {};
        desc.mType = PIPELINE_TYPE_GRAPHICS;
        GraphicsPipelineDesc& pipelineSettings = desc.mGraphicsDesc;
        pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
        pipelineSettings.mRenderTargetCount = 1;
        pipelineSettings.pDepthState = &depthStateDesc;
        pipelineSettings.pColorFormats = &pSwapChain->ppRenderTargets[0]->mFormat;
        pipelineSettings.mSampleCount = pSwapChain->ppRenderTargets[0]->mSampleCount;
        pipelineSettings.mSampleQuality = pSwapChain->ppRenderTargets[0]->mSampleQuality;
        pipelineSettings.pRootSignature = pRootSignature;
        pipelineSettings.pShaderProgram = pBasicShader;
        pipelineSettings.pVertexLayout = &vertexLayout;
        pipelineSettings.pRasterizerState = &rasterizerStateDesc;
        addPipeline(pRenderer, &desc, &pBasicPipeline);
    }

    void removePipelines() { removePipeline(pRenderer, pBasicPipeline); }

    void prepareDescriptorSets()
    {
        DescriptorData param = {};
        param.pName = "uTexture";
        switch (gConstantData.deviceType)
        {
        case INPUT_DEVICE_GAMEPAD:
            param.ppTextures = &pGamepad;
            updateDescriptorSet(pRenderer, 0, pDescriptorDevice, 1, &param);
            break;
        case INPUT_DEVICE_KBM:
            param.ppTextures = &pKeyboardMouse;
            updateDescriptorSet(pRenderer, 0, pDescriptorDevice, 1, &param);
            break;
#if defined(ENABLE_FORGE_TOUCH_INPUT)
        case INPUT_DEVICE_TOUCH:
            break;
#endif
        }

        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            DescriptorData uParam = {};
            uParam.pName = "uInputData";
            uParam.ppBuffers = &pInputDataUniformBuffer[i];
            updateDescriptorSet(pRenderer, i, pDescriptorInputData, 1, &uParam);
        }
    }
};
DEFINE_APPLICATION_MAIN(Input)
