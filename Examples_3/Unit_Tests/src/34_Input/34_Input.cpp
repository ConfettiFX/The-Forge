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
#include "../../../../Common_3/Application/Interfaces/IInput.h"
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

#if (defined(TARGET_IOS) || defined(__ANDROID__) || defined(NX64)) && !defined(QUEST_VR)
#define TOUCH_INPUT 1
#endif

// Button bit definitions
#include "ButtonDefs.h"

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

ConstantData gConstantData;
// For UI purposes, a custom variable to remove 'invalid' device type - 0.Gamepad 1.Touch 2&3.Keyboard+Mouse
uint32_t     gChosenDeviceType = 0;
bool         gEnableRumbleAndLights = false;

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

/////// ACTION MAPPINGS IDS FOR EACH INPUT DEVICE WE TEST /////////////////
// Note: for this unit-test we create action mappings that are 1 to 1 with
//       the hardware device inputs.  This allows us to test the gainput
//       backend to ensure all devices' input controls/buttons are handled.

// KB+Mouse
typedef struct KeyboardMouseInputActions
{
    typedef enum KeyboardMouseInputAction
    {
        // Keyboard
        ESCAPE,
        F1,
        F2,
        F3,
        F4,
        F5,
        F6,
        F7,
        F8,
        F9,
        F10,
        F11,
        F12,
        BREAK,
        INSERT,
        DEL,
        NUM_LOCK,
        KP_MULTIPLY,

        ACUTE,
        NUM_1,
        NUM_2,
        NUM_3,
        NUM_4,
        NUM_5,
        NUM_6,
        NUM_7,
        NUM_8,
        NUM_9,
        NUM_0,
        MINUS,
        EQUAL,
        BACK_SPACE,
        KP_ADD,
        KP_SUBTRACT,

        TAB,
        Q,
        W,
        E,
        R,
        T,
        Y,
        U,
        I,
        O,
        P,
        BRACKET_LEFT,
        BRACKET_RIGHT,
        BACK_SLASH,
        KP_7_HOME,
        KP_8_UP,
        KP_9_PAGE_UP,

        CAPS_LOCK,
        A,
        S,
        D,
        F,
        G,
        H,
        J,
        K,
        L,
        SEMICOLON,
        APOSTROPHE,
        ENTER,
        KP_4_LEFT,
        KP_5_BEGIN,
        KP_6_RIGHT,

        SHIFT_L,
        Z,
        X,
        C,
        V,
        B,
        N,
        M,
        COMMA,
        PERIOD,
        FWRD_SLASH,
        SHIFT_R,
        KP_1_END,
        KP_2_DOWN,
        KP_3_PAGE_DOWN,

        CTRL_L,
        ALT_L,
        SPACE,
        ALT_R,
        CTRL_R,
        LEFT,
        RIGHT,
        UP,
        DOWN,
        KP_0_INSERT,

        // Mouse
        LEFT_CLICK,
        MID_CLICK,
        RIGHT_CLICK,
        SCROLL_UP,
        SCROLL_DOWN,

        // Full screen / dump data actions (we always need those)
        DUMP_PROFILE_DATA,
        TOGGLE_FULLSCREEN,

        // Default actions for UI (we always need those to be able to interact with the UI)
        UI_MOUSE_LEFT = UISystemInputActions::UI_ACTION_MOUSE_LEFT,
        UI_MOUSE_RIGHT = UISystemInputActions::UI_ACTION_MOUSE_RIGHT,
        UI_MOUSE_MIDDLE = UISystemInputActions::UI_ACTION_MOUSE_MIDDLE,
        UI_MOUSE_SCROLL_UP = UISystemInputActions::UI_ACTION_MOUSE_SCROLL_UP,
        UI_MOUSE_SCROLL_DOWN = UISystemInputActions::UI_ACTION_MOUSE_SCROLL_DOWN,
        UI_NAV_TOGGLE_UI = UISystemInputActions::UI_ACTION_NAV_TOGGLE_UI,
        UI_NAV_ACTIVATE = UISystemInputActions::UI_ACTION_NAV_ACTIVATE,
        UI_NAV_CANCEL = UISystemInputActions::UI_ACTION_NAV_CANCEL,
        UI_NAV_INPUT = UISystemInputActions::UI_ACTION_NAV_INPUT,
        UI_NAV_MENU = UISystemInputActions::UI_ACTION_NAV_MENU,
        UI_NAV_TWEAK_WINDOW_LEFT = UISystemInputActions::UI_ACTION_NAV_TWEAK_WINDOW_LEFT,
        UI_NAV_TWEAK_WINDOW_RIGHT = UISystemInputActions::UI_ACTION_NAV_TWEAK_WINDOW_RIGHT,
        UI_NAV_TWEAK_WINDOW_UP = UISystemInputActions::UI_ACTION_NAV_TWEAK_WINDOW_UP,
        UI_NAV_TWEAK_WINDOW_DOWN = UISystemInputActions::UI_ACTION_NAV_TWEAK_WINDOW_DOWN,
        UI_NAV_SCROLL_MOVE_WINDOW = UISystemInputActions::UI_ACTION_NAV_SCROLL_MOVE_WINDOW,
        UI_NAV_FOCUS_PREV = UISystemInputActions::UI_ACTION_NAV_FOCUS_PREV,
        UI_NAV_FOCUS_NEXT = UISystemInputActions::UI_ACTION_NAV_FOCUS_NEXT,
        UI_NAV_TWEAK_SLOW = UISystemInputActions::UI_ACTION_NAV_TWEAK_SLOW,
        UI_NAV_TWEAK_FAST = UISystemInputActions::UI_ACTION_NAV_TWEAK_FAST,
    } KeyboardMouseInputAction;
} KeyboardMouseInputActions;

ActionMappingDesc gKeyboardMouseActionMappings[] = {
    // Keyboard
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::ESCAPE,
      { KeyboardButton::KEYBOARD_BUTTON_ESCAPE } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::F1,
      { KeyboardButton::KEYBOARD_BUTTON_F1 } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::F2,
      { KeyboardButton::KEYBOARD_BUTTON_F2 } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::F3,
      { KeyboardButton::KEYBOARD_BUTTON_F3 } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::F4,
      { KeyboardButton::KEYBOARD_BUTTON_F4 } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::F5,
      { KeyboardButton::KEYBOARD_BUTTON_F5 } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::F6,
      { KeyboardButton::KEYBOARD_BUTTON_F6 } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::F7,
      { KeyboardButton::KEYBOARD_BUTTON_F7 } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::F8,
      { KeyboardButton::KEYBOARD_BUTTON_F8 } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::F9,
      { KeyboardButton::KEYBOARD_BUTTON_F9 } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::F10,
      { KeyboardButton::KEYBOARD_BUTTON_F10 } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::F11,
      { KeyboardButton::KEYBOARD_BUTTON_F11 } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::F12,
      { KeyboardButton::KEYBOARD_BUTTON_F12 } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::BREAK,
      { KeyboardButton::KEYBOARD_BUTTON_BREAK } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::INSERT,
      { KeyboardButton::KEYBOARD_BUTTON_INSERT } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::DEL,
      { KeyboardButton::KEYBOARD_BUTTON_DELETE } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::NUM_LOCK,
      { KeyboardButton::KEYBOARD_BUTTON_NUM_LOCK } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::KP_MULTIPLY,
      { KeyboardButton::KEYBOARD_BUTTON_KP_MULTIPLY } },

    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::ACUTE,
      { KeyboardButton::KEYBOARD_BUTTON_EXTRA5 } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::NUM_1,
      { KeyboardButton::KEYBOARD_BUTTON_1 } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::NUM_2,
      { KeyboardButton::KEYBOARD_BUTTON_2 } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::NUM_3,
      { KeyboardButton::KEYBOARD_BUTTON_3 } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::NUM_4,
      { KeyboardButton::KEYBOARD_BUTTON_4 } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::NUM_5,
      { KeyboardButton::KEYBOARD_BUTTON_5 } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::NUM_6,
      { KeyboardButton::KEYBOARD_BUTTON_6 } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::NUM_7,
      { KeyboardButton::KEYBOARD_BUTTON_7 } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::NUM_8,
      { KeyboardButton::KEYBOARD_BUTTON_8 } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::NUM_9,
      { KeyboardButton::KEYBOARD_BUTTON_9 } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::NUM_0,
      { KeyboardButton::KEYBOARD_BUTTON_0 } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::MINUS,
      { KeyboardButton::KEYBOARD_BUTTON_MINUS } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::EQUAL,
      { KeyboardButton::KEYBOARD_BUTTON_EQUAL } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::BACK_SPACE,
      { KeyboardButton::KEYBOARD_BUTTON_BACK_SPACE } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::KP_ADD,
      { KeyboardButton::KEYBOARD_BUTTON_KP_ADD } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::KP_SUBTRACT,
      { KeyboardButton::KEYBOARD_BUTTON_KP_SUBTRACT } },

    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::TAB,
      { KeyboardButton::KEYBOARD_BUTTON_TAB } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::Q,
      { KeyboardButton::KEYBOARD_BUTTON_Q } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::W,
      { KeyboardButton::KEYBOARD_BUTTON_W } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::E,
      { KeyboardButton::KEYBOARD_BUTTON_E } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::R,
      { KeyboardButton::KEYBOARD_BUTTON_R } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::T,
      { KeyboardButton::KEYBOARD_BUTTON_T } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::Y,
      { KeyboardButton::KEYBOARD_BUTTON_Y } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::U,
      { KeyboardButton::KEYBOARD_BUTTON_U } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::I,
      { KeyboardButton::KEYBOARD_BUTTON_I } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::O,
      { KeyboardButton::KEYBOARD_BUTTON_O } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::P,
      { KeyboardButton::KEYBOARD_BUTTON_P } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::BRACKET_LEFT,
      { KeyboardButton::KEYBOARD_BUTTON_BRACKET_LEFT } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::BRACKET_RIGHT,
      { KeyboardButton::KEYBOARD_BUTTON_BRACKET_RIGHT } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::BACK_SLASH,
      { KeyboardButton::KEYBOARD_BUTTON_BACKSLASH } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::KP_7_HOME,
      { KeyboardButton::KEYBOARD_BUTTON_KP_HOME } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::KP_8_UP,
      { KeyboardButton::KEYBOARD_BUTTON_KP_UP } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::KP_9_PAGE_UP,
      { KeyboardButton::KEYBOARD_BUTTON_KP_PAGE_UP } },

    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::CAPS_LOCK,
      { KeyboardButton::KEYBOARD_BUTTON_CAPS_LOCK } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::A,
      { KeyboardButton::KEYBOARD_BUTTON_A } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::S,
      { KeyboardButton::KEYBOARD_BUTTON_S } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::D,
      { KeyboardButton::KEYBOARD_BUTTON_D } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::F,
      { KeyboardButton::KEYBOARD_BUTTON_F } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::G,
      { KeyboardButton::KEYBOARD_BUTTON_G } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::H,
      { KeyboardButton::KEYBOARD_BUTTON_H } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::J,
      { KeyboardButton::KEYBOARD_BUTTON_J } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::K,
      { KeyboardButton::KEYBOARD_BUTTON_K } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::L,
      { KeyboardButton::KEYBOARD_BUTTON_L } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::SEMICOLON,
      { KeyboardButton::KEYBOARD_BUTTON_SEMICOLON } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::APOSTROPHE,
      { KeyboardButton::KEYBOARD_BUTTON_APOSTROPHE } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::ENTER,
      { KeyboardButton::KEYBOARD_BUTTON_RETURN } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::KP_4_LEFT,
      { KeyboardButton::KEYBOARD_BUTTON_KP_LEFT } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::KP_5_BEGIN,
      { KeyboardButton::KEYBOARD_BUTTON_KP_BEGIN } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::KP_6_RIGHT,
      { KeyboardButton::KEYBOARD_BUTTON_KP_RIGHT } },

    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::SHIFT_L,
      { KeyboardButton::KEYBOARD_BUTTON_SHIFT_L } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::Z,
      { KeyboardButton::KEYBOARD_BUTTON_Z } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::X,
      { KeyboardButton::KEYBOARD_BUTTON_X } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::C,
      { KeyboardButton::KEYBOARD_BUTTON_C } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::V,
      { KeyboardButton::KEYBOARD_BUTTON_V } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::B,
      { KeyboardButton::KEYBOARD_BUTTON_B } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::N,
      { KeyboardButton::KEYBOARD_BUTTON_N } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::M,
      { KeyboardButton::KEYBOARD_BUTTON_M } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::COMMA,
      { KeyboardButton::KEYBOARD_BUTTON_COMMA } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::PERIOD,
      { KeyboardButton::KEYBOARD_BUTTON_PERIOD } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::FWRD_SLASH,
      { KeyboardButton::KEYBOARD_BUTTON_SLASH } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::SHIFT_R,
      { KeyboardButton::KEYBOARD_BUTTON_SHIFT_R } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::KP_1_END,
      { KeyboardButton::KEYBOARD_BUTTON_KP_END } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::KP_2_DOWN,
      { KeyboardButton::KEYBOARD_BUTTON_KP_DOWN } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::KP_3_PAGE_DOWN,
      { KeyboardButton::KEYBOARD_BUTTON_KP_PAGE_DOWN } },

    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::CTRL_L,
      { KeyboardButton::KEYBOARD_BUTTON_CTRL_L } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::ALT_L,
      { KeyboardButton::KEYBOARD_BUTTON_ALT_L } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::SPACE,
      { KeyboardButton::KEYBOARD_BUTTON_SPACE } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::ALT_R,
      { KeyboardButton::KEYBOARD_BUTTON_ALT_R } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::CTRL_R,
      { KeyboardButton::KEYBOARD_BUTTON_CTRL_R } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::LEFT,
      { KeyboardButton::KEYBOARD_BUTTON_LEFT } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::RIGHT,
      { KeyboardButton::KEYBOARD_BUTTON_RIGHT } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::UP,
      { KeyboardButton::KEYBOARD_BUTTON_UP } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::DOWN,
      { KeyboardButton::KEYBOARD_BUTTON_DOWN } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::KP_0_INSERT,
      { KeyboardButton::KEYBOARD_BUTTON_KP_INSERT } },

    // Mouse
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_MOUSE,
      KeyboardMouseInputActions::LEFT_CLICK,
      { MouseButton::MOUSE_BUTTON_LEFT } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_MOUSE,
      KeyboardMouseInputActions::MID_CLICK,
      { MouseButton::MOUSE_BUTTON_MIDDLE } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_MOUSE,
      KeyboardMouseInputActions::RIGHT_CLICK,
      { MouseButton::MOUSE_BUTTON_RIGHT } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_MOUSE,
      KeyboardMouseInputActions::SCROLL_UP,
      { MouseButton::MOUSE_BUTTON_WHEEL_UP } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_MOUSE,
      KeyboardMouseInputActions::SCROLL_DOWN,
      { MouseButton::MOUSE_BUTTON_WHEEL_DOWN } },

    // Profile data / toggle fullscreen / exit actions
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::DUMP_PROFILE_DATA,
      { KeyboardButton::KEYBOARD_BUTTON_F3 } },
    { INPUT_ACTION_MAPPING_COMBO,
      INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
      KeyboardMouseInputActions::DUMP_PROFILE_DATA,
      { GamepadButton::GAMEPAD_BUTTON_START, GamepadButton::GAMEPAD_BUTTON_B } },
    { INPUT_ACTION_MAPPING_COMBO,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::TOGGLE_FULLSCREEN,
      { KeyboardButton::KEYBOARD_BUTTON_ALT_L, KeyboardButton::KEYBOARD_BUTTON_RETURN } },

    // UI specific actions
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_MOUSE,
      KeyboardMouseInputActions::UI_MOUSE_LEFT,
      { MouseButton::MOUSE_BUTTON_LEFT } },
    { INPUT_ACTION_MAPPING_NORMAL, INPUT_ACTION_MAPPING_TARGET_TOUCH, KeyboardMouseInputActions::UI_MOUSE_LEFT },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_MOUSE,
      KeyboardMouseInputActions::UI_MOUSE_RIGHT,
      { MouseButton::MOUSE_BUTTON_RIGHT } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_MOUSE,
      KeyboardMouseInputActions::UI_MOUSE_MIDDLE,
      { MouseButton::MOUSE_BUTTON_MIDDLE } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_MOUSE,
      KeyboardMouseInputActions::UI_MOUSE_SCROLL_UP,
      { MouseButton::MOUSE_BUTTON_WHEEL_UP } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_MOUSE,
      KeyboardMouseInputActions::UI_MOUSE_SCROLL_DOWN,
      { MouseButton::MOUSE_BUTTON_WHEEL_DOWN } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
      KeyboardMouseInputActions::UI_NAV_TOGGLE_UI,
      { GamepadButton::GAMEPAD_BUTTON_R3 } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      KeyboardMouseInputActions::UI_NAV_TOGGLE_UI,
      { KeyboardButton::KEYBOARD_BUTTON_F1 } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
      KeyboardMouseInputActions::UI_NAV_ACTIVATE,
      { GamepadButton::GAMEPAD_BUTTON_A } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
      KeyboardMouseInputActions::UI_NAV_CANCEL,
      { GamepadButton::GAMEPAD_BUTTON_B } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
      KeyboardMouseInputActions::UI_NAV_INPUT,
      { GamepadButton::GAMEPAD_BUTTON_Y } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
      KeyboardMouseInputActions::UI_NAV_MENU,
      { GamepadButton::GAMEPAD_BUTTON_X } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
      KeyboardMouseInputActions::UI_NAV_TWEAK_WINDOW_LEFT,
      { GamepadButton::GAMEPAD_BUTTON_LEFT } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
      KeyboardMouseInputActions::UI_NAV_TWEAK_WINDOW_RIGHT,
      { GamepadButton::GAMEPAD_BUTTON_RIGHT } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
      KeyboardMouseInputActions::UI_NAV_TWEAK_WINDOW_UP,
      { GamepadButton::GAMEPAD_BUTTON_UP } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
      KeyboardMouseInputActions::UI_NAV_TWEAK_WINDOW_DOWN,
      { GamepadButton::GAMEPAD_BUTTON_DOWN } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
      KeyboardMouseInputActions::UI_NAV_SCROLL_MOVE_WINDOW,
      { GamepadButton::GAMEPAD_BUTTON_LEFT_STICK_X },
      2 },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
      KeyboardMouseInputActions::UI_NAV_FOCUS_PREV,
      { GamepadButton::GAMEPAD_BUTTON_L1 } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
      KeyboardMouseInputActions::UI_NAV_FOCUS_NEXT,
      { GamepadButton::GAMEPAD_BUTTON_R1 } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
      KeyboardMouseInputActions::UI_NAV_TWEAK_SLOW,
      { GamepadButton::GAMEPAD_BUTTON_L2 } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
      KeyboardMouseInputActions::UI_NAV_TWEAK_FAST,
      { GamepadButton::GAMEPAD_BUTTON_R2 } },
};

// Controller
typedef struct ControllerInputActions
{
    typedef enum ControllerInputAction
    {
        // Controller
        LEFT_STICK,
        RIGHT_STICK,
        FLOAT_L2,
        FLOAT_R2,
        A,
        B,
        X,
        Y,
        L1,
        R1,
        L3,
        R3,
        DPAD_LEFT,
        DPAD_RIGHT,
        DPAD_UP,
        DPAD_DOWN,
        START,
        SELECT,
        HOME,

        // Full screen / dump data actions (we always need those)
        DUMP_PROFILE_DATA,
        TOGGLE_FULLSCREEN,

        // Default actions for UI (we always need those to be able to interact with the UI)
        UI_MOUSE_LEFT = UISystemInputActions::UI_ACTION_MOUSE_LEFT,
        UI_MOUSE_RIGHT = UISystemInputActions::UI_ACTION_MOUSE_RIGHT,
        UI_MOUSE_MIDDLE = UISystemInputActions::UI_ACTION_MOUSE_MIDDLE,
        UI_MOUSE_SCROLL_UP = UISystemInputActions::UI_ACTION_MOUSE_SCROLL_UP,
        UI_MOUSE_SCROLL_DOWN = UISystemInputActions::UI_ACTION_MOUSE_SCROLL_DOWN,
        UI_NAV_TOGGLE_UI = UISystemInputActions::UI_ACTION_NAV_TOGGLE_UI,
        UI_NAV_ACTIVATE = UISystemInputActions::UI_ACTION_NAV_ACTIVATE,
        UI_NAV_CANCEL = UISystemInputActions::UI_ACTION_NAV_CANCEL,
        UI_NAV_INPUT = UISystemInputActions::UI_ACTION_NAV_INPUT,
        UI_NAV_MENU = UISystemInputActions::UI_ACTION_NAV_MENU,
        UI_NAV_TWEAK_WINDOW_LEFT = UISystemInputActions::UI_ACTION_NAV_TWEAK_WINDOW_LEFT,
        UI_NAV_TWEAK_WINDOW_RIGHT = UISystemInputActions::UI_ACTION_NAV_TWEAK_WINDOW_RIGHT,
        UI_NAV_TWEAK_WINDOW_UP = UISystemInputActions::UI_ACTION_NAV_TWEAK_WINDOW_UP,
        UI_NAV_TWEAK_WINDOW_DOWN = UISystemInputActions::UI_ACTION_NAV_TWEAK_WINDOW_DOWN,
        UI_NAV_SCROLL_MOVE_WINDOW = UISystemInputActions::UI_ACTION_NAV_SCROLL_MOVE_WINDOW,
        UI_NAV_FOCUS_PREV = UISystemInputActions::UI_ACTION_NAV_FOCUS_PREV,
        UI_NAV_FOCUS_NEXT = UISystemInputActions::UI_ACTION_NAV_FOCUS_NEXT,
        UI_NAV_TWEAK_SLOW = UISystemInputActions::UI_ACTION_NAV_TWEAK_SLOW,
        UI_NAV_TWEAK_FAST = UISystemInputActions::UI_ACTION_NAV_TWEAK_FAST,
    } ControllerInputAction;
} ControllerInputActions;

ActionMappingDesc gControllerActionMappings[] = {
    // Controller
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
      ControllerInputActions::LEFT_STICK,
      { GamepadButton::GAMEPAD_BUTTON_LEFT_STICK_X },
      2 },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
      ControllerInputActions::RIGHT_STICK,
      { GamepadButton::GAMEPAD_BUTTON_RIGHT_STICK_X },
      2 },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
      ControllerInputActions::FLOAT_L2,
      { GamepadButton::GAMEPAD_BUTTON_AXIS_4 },
      1 },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
      ControllerInputActions::FLOAT_R2,
      { GamepadButton::GAMEPAD_BUTTON_AXIS_5 },
      1 },
    { INPUT_ACTION_MAPPING_NORMAL, INPUT_ACTION_MAPPING_TARGET_CONTROLLER, ControllerInputActions::A, { GamepadButton::GAMEPAD_BUTTON_A } },
    { INPUT_ACTION_MAPPING_NORMAL, INPUT_ACTION_MAPPING_TARGET_CONTROLLER, ControllerInputActions::B, { GamepadButton::GAMEPAD_BUTTON_B } },
    { INPUT_ACTION_MAPPING_NORMAL, INPUT_ACTION_MAPPING_TARGET_CONTROLLER, ControllerInputActions::X, { GamepadButton::GAMEPAD_BUTTON_X } },
    { INPUT_ACTION_MAPPING_NORMAL, INPUT_ACTION_MAPPING_TARGET_CONTROLLER, ControllerInputActions::Y, { GamepadButton::GAMEPAD_BUTTON_Y } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
      ControllerInputActions::L1,
      { GamepadButton::GAMEPAD_BUTTON_L1 } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
      ControllerInputActions::R1,
      { GamepadButton::GAMEPAD_BUTTON_R1 } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
      ControllerInputActions::L3,
      { GamepadButton::GAMEPAD_BUTTON_L3 } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
      ControllerInputActions::R3,
      { GamepadButton::GAMEPAD_BUTTON_R3 } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
      ControllerInputActions::DPAD_LEFT,
      { GamepadButton::GAMEPAD_BUTTON_LEFT } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
      ControllerInputActions::DPAD_RIGHT,
      { GamepadButton::GAMEPAD_BUTTON_RIGHT } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
      ControllerInputActions::DPAD_UP,
      { GamepadButton::GAMEPAD_BUTTON_UP } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
      ControllerInputActions::DPAD_DOWN,
      { GamepadButton::GAMEPAD_BUTTON_DOWN } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
      ControllerInputActions::START,
      { GamepadButton::GAMEPAD_BUTTON_START } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
      ControllerInputActions::SELECT,
      { GamepadButton::GAMEPAD_BUTTON_SELECT } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
      ControllerInputActions::HOME,
      { GamepadButton::GAMEPAD_BUTTON_HOME } },

    // Profile data / toggle fullscreen / exit actions
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      ControllerInputActions::DUMP_PROFILE_DATA,
      { KeyboardButton::KEYBOARD_BUTTON_F3 } },
    { INPUT_ACTION_MAPPING_COMBO,
      INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
      ControllerInputActions::DUMP_PROFILE_DATA,
      { GamepadButton::GAMEPAD_BUTTON_START, GamepadButton::GAMEPAD_BUTTON_B } },
    { INPUT_ACTION_MAPPING_COMBO,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      ControllerInputActions::TOGGLE_FULLSCREEN,
      { KeyboardButton::KEYBOARD_BUTTON_ALT_L, KeyboardButton::KEYBOARD_BUTTON_RETURN } },

    // UI specific actions
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_MOUSE,
      ControllerInputActions::UI_MOUSE_LEFT,
      { MouseButton::MOUSE_BUTTON_LEFT } },
    { INPUT_ACTION_MAPPING_NORMAL, INPUT_ACTION_MAPPING_TARGET_TOUCH, ControllerInputActions::UI_MOUSE_LEFT },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_MOUSE,
      ControllerInputActions::UI_MOUSE_RIGHT,
      { MouseButton::MOUSE_BUTTON_RIGHT } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_MOUSE,
      ControllerInputActions::UI_MOUSE_MIDDLE,
      { MouseButton::MOUSE_BUTTON_MIDDLE } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_MOUSE,
      ControllerInputActions::UI_MOUSE_SCROLL_UP,
      { MouseButton::MOUSE_BUTTON_WHEEL_UP } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_MOUSE,
      ControllerInputActions::UI_MOUSE_SCROLL_DOWN,
      { MouseButton::MOUSE_BUTTON_WHEEL_DOWN } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
      ControllerInputActions::UI_NAV_TOGGLE_UI,
      { GamepadButton::GAMEPAD_BUTTON_R3 } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
      ControllerInputActions::UI_NAV_TOGGLE_UI,
      { KeyboardButton::KEYBOARD_BUTTON_F1 } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
      ControllerInputActions::UI_NAV_ACTIVATE,
      { GamepadButton::GAMEPAD_BUTTON_A } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
      ControllerInputActions::UI_NAV_CANCEL,
      { GamepadButton::GAMEPAD_BUTTON_B } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
      ControllerInputActions::UI_NAV_INPUT,
      { GamepadButton::GAMEPAD_BUTTON_Y } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
      ControllerInputActions::UI_NAV_MENU,
      { GamepadButton::GAMEPAD_BUTTON_X } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
      ControllerInputActions::UI_NAV_TWEAK_WINDOW_LEFT,
      { GamepadButton::GAMEPAD_BUTTON_LEFT } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
      ControllerInputActions::UI_NAV_TWEAK_WINDOW_RIGHT,
      { GamepadButton::GAMEPAD_BUTTON_RIGHT } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
      ControllerInputActions::UI_NAV_TWEAK_WINDOW_UP,
      { GamepadButton::GAMEPAD_BUTTON_UP } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
      ControllerInputActions::UI_NAV_TWEAK_WINDOW_DOWN,
      { GamepadButton::GAMEPAD_BUTTON_DOWN } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
      ControllerInputActions::UI_NAV_SCROLL_MOVE_WINDOW,
      { GamepadButton::GAMEPAD_BUTTON_LEFT_STICK_X },
      2 },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
      ControllerInputActions::UI_NAV_FOCUS_PREV,
      { GamepadButton::GAMEPAD_BUTTON_L1 } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
      ControllerInputActions::UI_NAV_FOCUS_NEXT,
      { GamepadButton::GAMEPAD_BUTTON_R1 } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
      ControllerInputActions::UI_NAV_TWEAK_SLOW,
      { GamepadButton::GAMEPAD_BUTTON_L2 } },
    { INPUT_ACTION_MAPPING_NORMAL,
      INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
      ControllerInputActions::UI_NAV_TWEAK_FAST,
      { GamepadButton::GAMEPAD_BUTTON_R2 } },
};

#ifdef TOUCH_INPUT

// Touch
typedef struct TouchInputActions
{
    typedef enum TouchInputAction
    {
        TAP = 0,
        PAN,
        DOUBLE_TAP,
        SWIPE,
        PINCH,
        ROTATE,
        LONG_PRESS,

        // Default action for UI (we always need this to be able to interact with the UI)
        UI_MOUSE_LEFT = UISystemInputActions::UI_ACTION_MOUSE_LEFT,
        COUNT

    } TouchInputAction;
} TouchInputActions;

ActionMappingDesc gTouchActionMappings[] = {
    // Touch
    { INPUT_ACTION_MAPPING_TOUCH_GESTURE,
      INPUT_ACTION_MAPPING_TARGET_TOUCH,
      TouchInputActions::TAP,
      { TouchGesture::TOUCH_GESTURE_TAP },
      1,
      0,
      0,
      20.f,
      200.f,
      1.f,
      AREA_FULL },
    { INPUT_ACTION_MAPPING_TOUCH_GESTURE,
      INPUT_ACTION_MAPPING_TARGET_TOUCH,
      TouchInputActions::PAN,
      { TouchGesture::TOUCH_GESTURE_PAN },
      1,
      0,
      0,
      20.f,
      200.f,
      1.f,
      AREA_FULL },
    { INPUT_ACTION_MAPPING_TOUCH_GESTURE,
      INPUT_ACTION_MAPPING_TARGET_TOUCH,
      TouchInputActions::DOUBLE_TAP,
      { TouchGesture::TOUCH_GESTURE_DOUBLE_TAP },
      1,
      0,
      0,
      20.f,
      200.f,
      1.f,
      AREA_FULL },
    { INPUT_ACTION_MAPPING_TOUCH_GESTURE,
      INPUT_ACTION_MAPPING_TARGET_TOUCH,
      TouchInputActions::SWIPE,
      { TouchGesture::TOUCH_GESTURE_SWIPE },
      1,
      0,
      0,
      20.f,
      200.f,
      1.f,
      AREA_FULL },
    { INPUT_ACTION_MAPPING_TOUCH_GESTURE,
      INPUT_ACTION_MAPPING_TARGET_TOUCH,
      TouchInputActions::PINCH,
      { TouchGesture::TOUCH_GESTURE_PINCH },
      1,
      0,
      0,
      20.f,
      200.f,
      1.f,
      AREA_FULL },
    { INPUT_ACTION_MAPPING_TOUCH_GESTURE,
      INPUT_ACTION_MAPPING_TARGET_TOUCH,
      TouchInputActions::ROTATE,
      { TouchGesture::TOUCH_GESTURE_ROTATE },
      1,
      0,
      0,
      20.f,
      200.f,
      1.f,
      AREA_FULL },
    { INPUT_ACTION_MAPPING_TOUCH_GESTURE,
      INPUT_ACTION_MAPPING_TARGET_TOUCH,
      TouchInputActions::LONG_PRESS,
      { TouchGesture::TOUCH_GESTURE_LONG_PRESS },
      1,
      0,
      0,
      20.f,
      200.f,
      1.f,
      AREA_FULL },
    { INPUT_ACTION_MAPPING_NORMAL, INPUT_ACTION_MAPPING_TARGET_TOUCH, TouchInputActions::UI_MOUSE_LEFT },
};

struct GestureContext
{
    InputActionContext mCtx;
    float              mTime;
    bool               mPrinted;
};

float gGestureTextTimeout = 3.0f;

GestureContext gGestureContexts[TF_ARRAY_COUNT(gTouchActionMappings)] = {};

#endif

///////////////////////////////////////////////////////////////////////////

#define SET_BUTTON_BIT(set, state, button_bit) ((set) = (state) ? ((set) | (button_bit)) : ((set) & ~(button_bit)));

bool StickChecker(InputActionContext* ctx)
{
    const uint32_t actionId = ctx->mActionId;

    if (actionId == ControllerInputActions::LEFT_STICK)
    {
        gConstantData.leftAxis.x = ctx->mFloat2.x;
        gConstantData.leftAxis.y = -ctx->mFloat2.y;
    }
    else if (actionId == ControllerInputActions::RIGHT_STICK)
    {
        gConstantData.rightAxis.x = ctx->mFloat2.x;
        gConstantData.rightAxis.y = -ctx->mFloat2.y;
    }

    return true;
}

bool KeyChecker(InputActionContext* ctx)
{
    const uint32_t actionId = ctx->mActionId;

    switch (gConstantData.deviceType)
    {
    case InputDeviceType::INPUT_DEVICE_GAMEPAD:
    {
        if (actionId == ControllerInputActions::FLOAT_L2)
        {
            gConstantData.motionButtons.x = ctx->mFloat;
            return true;
        }
        else if (actionId == ControllerInputActions::FLOAT_R2)
        {
            gConstantData.motionButtons.y = ctx->mFloat;
            return true;
        }

        switch (actionId)
        {
        case ControllerInputActions::DPAD_LEFT:
            SET_BUTTON_BIT(gConstantData.buttonSet0, ctx->mBool, CONTROLLER_DPAD_LEFT_BIT);
            return true;
        case ControllerInputActions::DPAD_RIGHT:
            SET_BUTTON_BIT(gConstantData.buttonSet0, ctx->mBool, CONTROLLER_DPAD_RIGHT_BIT);
            return true;
        case ControllerInputActions::DPAD_UP:
            SET_BUTTON_BIT(gConstantData.buttonSet0, ctx->mBool, CONTROLLER_DPAD_UP_BIT);
            return true;
        case ControllerInputActions::DPAD_DOWN:
            SET_BUTTON_BIT(gConstantData.buttonSet0, ctx->mBool, CONTROLLER_DPAD_DOWN_BIT);
            return true;
        case ControllerInputActions::A:
            SET_BUTTON_BIT(gConstantData.buttonSet0, ctx->mBool, CONTROLLER_A_BIT);
            return true;
        case ControllerInputActions::B:
            SET_BUTTON_BIT(gConstantData.buttonSet0, ctx->mBool, CONTROLLER_B_BIT);
            return true;
        case ControllerInputActions::X:
            SET_BUTTON_BIT(gConstantData.buttonSet0, ctx->mBool, CONTROLLER_X_BIT);
            return true;
        case ControllerInputActions::Y:
            SET_BUTTON_BIT(gConstantData.buttonSet0, ctx->mBool, CONTROLLER_Y_BIT);
            return true;
        case ControllerInputActions::L1:
            SET_BUTTON_BIT(gConstantData.buttonSet0, ctx->mBool, CONTROLLER_L1_BIT);
            return true;
        case ControllerInputActions::R1:
            SET_BUTTON_BIT(gConstantData.buttonSet0, ctx->mBool, CONTROLLER_R1_BIT);
            return true;
        case ControllerInputActions::L3:
            SET_BUTTON_BIT(gConstantData.buttonSet0, ctx->mBool, CONTROLLER_L3_BIT);
            // rumble for 1s when pressing L3
            setRumbleEffect(0, 1.0f, 1.0f, 1000);
            return true;
        case ControllerInputActions::R3:
            SET_BUTTON_BIT(gConstantData.buttonSet0, ctx->mBool, CONTROLLER_R3_BIT);
            return true;
        case ControllerInputActions::START:
            SET_BUTTON_BIT(gConstantData.buttonSet0, ctx->mBool, CONTROLLER_START_BIT);
            return true;
        case ControllerInputActions::SELECT:
            SET_BUTTON_BIT(gConstantData.buttonSet0, ctx->mBool, CONTROLLER_SELECT_BIT);
            return true;
        case ControllerInputActions::HOME:
            SET_BUTTON_BIT(gConstantData.buttonSet0, ctx->mBool, CONTROLLER_HOME_BIT);
            return true;
        }
        break;
    }
    case InputDeviceType::INPUT_DEVICE_TOUCH:
        break;
    case InputDeviceType::INPUT_DEVICE_KEYBOARD:
    case InputDeviceType::INPUT_DEVICE_MOUSE:
    {
        switch (actionId)
        {
        case KeyboardMouseInputActions::ESCAPE:
            SET_BUTTON_BIT(gConstantData.buttonSet0, ctx->mBool, ESCAPE_BIT);
            return true;
        case KeyboardMouseInputActions::F1:
            SET_BUTTON_BIT(gConstantData.buttonSet0, ctx->mBool, F1_BIT);
            return true;
        case KeyboardMouseInputActions::F2:
            SET_BUTTON_BIT(gConstantData.buttonSet0, ctx->mBool, F2_BIT);
            return true;
        case KeyboardMouseInputActions::F3:
            SET_BUTTON_BIT(gConstantData.buttonSet0, ctx->mBool, F3_BIT);
            return true;
        case KeyboardMouseInputActions::F4:
            SET_BUTTON_BIT(gConstantData.buttonSet0, ctx->mBool, F4_BIT);
            return true;
        case KeyboardMouseInputActions::F5:
            SET_BUTTON_BIT(gConstantData.buttonSet0, ctx->mBool, F5_BIT);
            return true;
        case KeyboardMouseInputActions::F6:
            SET_BUTTON_BIT(gConstantData.buttonSet0, ctx->mBool, F6_BIT);
            return true;
        case KeyboardMouseInputActions::F7:
            SET_BUTTON_BIT(gConstantData.buttonSet0, ctx->mBool, F7_BIT);
            return true;
        case KeyboardMouseInputActions::F8:
            SET_BUTTON_BIT(gConstantData.buttonSet0, ctx->mBool, F8_BIT);
            return true;
        case KeyboardMouseInputActions::F9:
            SET_BUTTON_BIT(gConstantData.buttonSet0, ctx->mBool, F9_BIT);
            return true;
        case KeyboardMouseInputActions::F10:
            SET_BUTTON_BIT(gConstantData.buttonSet0, ctx->mBool, F10_BIT);
            return true;
        case KeyboardMouseInputActions::F11:
            SET_BUTTON_BIT(gConstantData.buttonSet0, ctx->mBool, F11_BIT);
            return true;
        case KeyboardMouseInputActions::F12:
            SET_BUTTON_BIT(gConstantData.buttonSet0, ctx->mBool, F12_BIT);
            return true;
        case KeyboardMouseInputActions::BREAK:
            SET_BUTTON_BIT(gConstantData.buttonSet0, ctx->mBool, BREAK_BIT);
            return true;
        case KeyboardMouseInputActions::INSERT:
            SET_BUTTON_BIT(gConstantData.buttonSet0, ctx->mBool, INSERT_BIT);
            return true;
        case KeyboardMouseInputActions::DEL:
            SET_BUTTON_BIT(gConstantData.buttonSet0, ctx->mBool, DEL_BIT);
            return true;
        case KeyboardMouseInputActions::NUM_LOCK:
            SET_BUTTON_BIT(gConstantData.buttonSet0, ctx->mBool, NUM_LOCK_BIT);
            return true;
        case KeyboardMouseInputActions::KP_MULTIPLY:
            SET_BUTTON_BIT(gConstantData.buttonSet0, ctx->mBool, KP_MULTIPLY_BIT);
            return true;

        case KeyboardMouseInputActions::ACUTE:
            SET_BUTTON_BIT(gConstantData.buttonSet0, ctx->mBool, ACUTE_BIT);
            return true;
        case KeyboardMouseInputActions::NUM_1:
            SET_BUTTON_BIT(gConstantData.buttonSet0, ctx->mBool, NUM_1_BIT);
            return true;
        case KeyboardMouseInputActions::NUM_2:
            SET_BUTTON_BIT(gConstantData.buttonSet0, ctx->mBool, NUM_2_BIT);
            return true;
        case KeyboardMouseInputActions::NUM_3:
            SET_BUTTON_BIT(gConstantData.buttonSet0, ctx->mBool, NUM_3_BIT);
            return true;
        case KeyboardMouseInputActions::NUM_4:
            SET_BUTTON_BIT(gConstantData.buttonSet0, ctx->mBool, NUM_4_BIT);
            return true;
        case KeyboardMouseInputActions::NUM_5:
            SET_BUTTON_BIT(gConstantData.buttonSet0, ctx->mBool, NUM_5_BIT);
            return true;
        case KeyboardMouseInputActions::NUM_6:
            SET_BUTTON_BIT(gConstantData.buttonSet0, ctx->mBool, NUM_6_BIT);
            return true;
        case KeyboardMouseInputActions::NUM_7:
            SET_BUTTON_BIT(gConstantData.buttonSet0, ctx->mBool, NUM_7_BIT);
            return true;
        case KeyboardMouseInputActions::NUM_8:
            SET_BUTTON_BIT(gConstantData.buttonSet0, ctx->mBool, NUM_8_BIT);
            return true;
        case KeyboardMouseInputActions::NUM_9:
            SET_BUTTON_BIT(gConstantData.buttonSet0, ctx->mBool, NUM_9_BIT);
            return true;
        case KeyboardMouseInputActions::NUM_0:
            SET_BUTTON_BIT(gConstantData.buttonSet0, ctx->mBool, NUM_0_BIT);
            return true;
        case KeyboardMouseInputActions::MINUS:
            SET_BUTTON_BIT(gConstantData.buttonSet0, ctx->mBool, MINUS_BIT);
            return true;
        case KeyboardMouseInputActions::EQUAL:
            SET_BUTTON_BIT(gConstantData.buttonSet0, ctx->mBool, EQUAL_BIT);
            return true;
        case KeyboardMouseInputActions::BACK_SPACE:
            SET_BUTTON_BIT(gConstantData.buttonSet0, ctx->mBool, BACK_SPACE_BIT);
            return true;
        case KeyboardMouseInputActions::KP_ADD:
            SET_BUTTON_BIT(gConstantData.buttonSet1, ctx->mBool, KP_ADD_BIT);
            return true;
        case KeyboardMouseInputActions::KP_SUBTRACT:
            SET_BUTTON_BIT(gConstantData.buttonSet1, ctx->mBool, KP_SUBTRACT_BIT);
            return true;

        case KeyboardMouseInputActions::TAB:
            SET_BUTTON_BIT(gConstantData.buttonSet1, ctx->mBool, TAB_BIT);
            return true;
        case KeyboardMouseInputActions::Q:
            SET_BUTTON_BIT(gConstantData.buttonSet1, ctx->mBool, Q_BIT);
            return true;
        case KeyboardMouseInputActions::W:
            SET_BUTTON_BIT(gConstantData.buttonSet1, ctx->mBool, W_BIT);
            return true;
        case KeyboardMouseInputActions::E:
            SET_BUTTON_BIT(gConstantData.buttonSet1, ctx->mBool, E_BIT);
            return true;
        case KeyboardMouseInputActions::R:
            SET_BUTTON_BIT(gConstantData.buttonSet1, ctx->mBool, R_BIT);
            return true;
        case KeyboardMouseInputActions::T:
            SET_BUTTON_BIT(gConstantData.buttonSet1, ctx->mBool, T_BIT);
            return true;
        case KeyboardMouseInputActions::Y:
            SET_BUTTON_BIT(gConstantData.buttonSet1, ctx->mBool, Y_BIT);
            return true;
        case KeyboardMouseInputActions::U:
            SET_BUTTON_BIT(gConstantData.buttonSet1, ctx->mBool, U_BIT);
            return true;
        case KeyboardMouseInputActions::I:
            SET_BUTTON_BIT(gConstantData.buttonSet1, ctx->mBool, I_BIT);
            return true;
        case KeyboardMouseInputActions::O:
            SET_BUTTON_BIT(gConstantData.buttonSet1, ctx->mBool, O_BIT);
            return true;
        case KeyboardMouseInputActions::P:
            SET_BUTTON_BIT(gConstantData.buttonSet1, ctx->mBool, P_BIT);
            return true;
        case KeyboardMouseInputActions::BRACKET_LEFT:
            SET_BUTTON_BIT(gConstantData.buttonSet1, ctx->mBool, BRACKET_LEFT_BIT);
            return true;
        case KeyboardMouseInputActions::BRACKET_RIGHT:
            SET_BUTTON_BIT(gConstantData.buttonSet1, ctx->mBool, BRACKET_RIGHT_BIT);
            return true;
        case KeyboardMouseInputActions::BACK_SLASH:
            SET_BUTTON_BIT(gConstantData.buttonSet1, ctx->mBool, BACK_SLASH_BIT);
            return true;
        case KeyboardMouseInputActions::KP_7_HOME:
            SET_BUTTON_BIT(gConstantData.buttonSet1, ctx->mBool, KP_7_HOME_BIT);
            return true;
        case KeyboardMouseInputActions::KP_8_UP:
            SET_BUTTON_BIT(gConstantData.buttonSet1, ctx->mBool, KP_8_UP_BIT);
            return true;
        case KeyboardMouseInputActions::KP_9_PAGE_UP:
            SET_BUTTON_BIT(gConstantData.buttonSet1, ctx->mBool, KP_9_PAGE_UP_BIT);
            return true;

        case KeyboardMouseInputActions::CAPS_LOCK:
            SET_BUTTON_BIT(gConstantData.buttonSet1, ctx->mBool, CAPS_LOCK_BIT);
            return true;
        case KeyboardMouseInputActions::A:
            SET_BUTTON_BIT(gConstantData.buttonSet1, ctx->mBool, A_BIT);
            return true;
        case KeyboardMouseInputActions::S:
            SET_BUTTON_BIT(gConstantData.buttonSet1, ctx->mBool, S_BIT);
            return true;
        case KeyboardMouseInputActions::D:
            SET_BUTTON_BIT(gConstantData.buttonSet1, ctx->mBool, D_BIT);
            return true;
        case KeyboardMouseInputActions::F:
            SET_BUTTON_BIT(gConstantData.buttonSet1, ctx->mBool, F_BIT);
            return true;
        case KeyboardMouseInputActions::G:
            SET_BUTTON_BIT(gConstantData.buttonSet1, ctx->mBool, G_BIT);
            return true;
        case KeyboardMouseInputActions::H:
            SET_BUTTON_BIT(gConstantData.buttonSet1, ctx->mBool, H_BIT);
            return true;
        case KeyboardMouseInputActions::J:
            SET_BUTTON_BIT(gConstantData.buttonSet1, ctx->mBool, J_BIT);
            return true;
        case KeyboardMouseInputActions::K:
            SET_BUTTON_BIT(gConstantData.buttonSet1, ctx->mBool, K_BIT);
            return true;
        case KeyboardMouseInputActions::L:
            SET_BUTTON_BIT(gConstantData.buttonSet1, ctx->mBool, L_BIT);
            return true;
        case KeyboardMouseInputActions::SEMICOLON:
            SET_BUTTON_BIT(gConstantData.buttonSet1, ctx->mBool, SEMICOLON_BIT);
            return true;
        case KeyboardMouseInputActions::APOSTROPHE:
            SET_BUTTON_BIT(gConstantData.buttonSet1, ctx->mBool, APOSTROPHE_BIT);
            return true;
        case KeyboardMouseInputActions::ENTER:
            SET_BUTTON_BIT(gConstantData.buttonSet1, ctx->mBool, ENTER_BIT);
            return true;
        case KeyboardMouseInputActions::KP_4_LEFT:
            SET_BUTTON_BIT(gConstantData.buttonSet2, ctx->mBool, KP_4_LEFT_BIT);
            return true;
        case KeyboardMouseInputActions::KP_5_BEGIN:
            SET_BUTTON_BIT(gConstantData.buttonSet2, ctx->mBool, KP_5_BEGIN_BIT);
            return true;
        case KeyboardMouseInputActions::KP_6_RIGHT:
            SET_BUTTON_BIT(gConstantData.buttonSet2, ctx->mBool, KP_6_RIGHT_BIT);
            return true;

        case KeyboardMouseInputActions::SHIFT_L:
            SET_BUTTON_BIT(gConstantData.buttonSet2, ctx->mBool, SHIFT_L_BIT);
            return true;
        case KeyboardMouseInputActions::Z:
            SET_BUTTON_BIT(gConstantData.buttonSet2, ctx->mBool, Z_BIT);
            return true;
        case KeyboardMouseInputActions::X:
            SET_BUTTON_BIT(gConstantData.buttonSet2, ctx->mBool, X_BIT);
            return true;
        case KeyboardMouseInputActions::C:
            SET_BUTTON_BIT(gConstantData.buttonSet2, ctx->mBool, C_BIT);
            return true;
        case KeyboardMouseInputActions::V:
            SET_BUTTON_BIT(gConstantData.buttonSet2, ctx->mBool, V_BIT);
            return true;
        case KeyboardMouseInputActions::B:
            SET_BUTTON_BIT(gConstantData.buttonSet2, ctx->mBool, B_BIT);
            return true;
        case KeyboardMouseInputActions::N:
            SET_BUTTON_BIT(gConstantData.buttonSet2, ctx->mBool, N_BIT);
            return true;
        case KeyboardMouseInputActions::M:
            SET_BUTTON_BIT(gConstantData.buttonSet2, ctx->mBool, M_BIT);
            return true;
        case KeyboardMouseInputActions::COMMA:
            SET_BUTTON_BIT(gConstantData.buttonSet2, ctx->mBool, COMMA_BIT);
            return true;
        case KeyboardMouseInputActions::PERIOD:
            SET_BUTTON_BIT(gConstantData.buttonSet2, ctx->mBool, PERIOD_BIT);
            return true;
        case KeyboardMouseInputActions::FWRD_SLASH:
            SET_BUTTON_BIT(gConstantData.buttonSet2, ctx->mBool, FWRD_SLASH_BIT);
            return true;
        case KeyboardMouseInputActions::SHIFT_R:
            SET_BUTTON_BIT(gConstantData.buttonSet2, ctx->mBool, SHIFT_R_BIT);
            return true;
        case KeyboardMouseInputActions::KP_1_END:
            SET_BUTTON_BIT(gConstantData.buttonSet2, ctx->mBool, KP_1_END_BIT);
            return true;
        case KeyboardMouseInputActions::KP_2_DOWN:
            SET_BUTTON_BIT(gConstantData.buttonSet2, ctx->mBool, KP_2_DOWN_BIT);
            return true;
        case KeyboardMouseInputActions::KP_3_PAGE_DOWN:
            SET_BUTTON_BIT(gConstantData.buttonSet2, ctx->mBool, KP_3_PAGE_DOWN_BIT);
            return true;

        case KeyboardMouseInputActions::CTRL_L:
            SET_BUTTON_BIT(gConstantData.buttonSet2, ctx->mBool, CTRL_L_BIT);
            return true;
        case KeyboardMouseInputActions::ALT_L:
            SET_BUTTON_BIT(gConstantData.buttonSet2, ctx->mBool, ALT_L_BIT);
            return true;
        case KeyboardMouseInputActions::SPACE:
            SET_BUTTON_BIT(gConstantData.buttonSet2, ctx->mBool, SPACE_BIT);
            return true;
        case KeyboardMouseInputActions::ALT_R:
            SET_BUTTON_BIT(gConstantData.buttonSet2, ctx->mBool, ALT_R_BIT);
            return true;
        case KeyboardMouseInputActions::CTRL_R:
            SET_BUTTON_BIT(gConstantData.buttonSet2, ctx->mBool, CTRL_R_BIT);
            return true;
        case KeyboardMouseInputActions::LEFT:
            SET_BUTTON_BIT(gConstantData.buttonSet2, ctx->mBool, LEFT_BIT);
            return true;
        case KeyboardMouseInputActions::RIGHT:
            SET_BUTTON_BIT(gConstantData.buttonSet2, ctx->mBool, RIGHT_BIT);
            return true;
        case KeyboardMouseInputActions::UP:
            SET_BUTTON_BIT(gConstantData.buttonSet2, ctx->mBool, UP_BIT);
            return true;
        case KeyboardMouseInputActions::DOWN:
            SET_BUTTON_BIT(gConstantData.buttonSet2, ctx->mBool, DOWN_BIT);
            return true;
        case KeyboardMouseInputActions::KP_0_INSERT:
            SET_BUTTON_BIT(gConstantData.buttonSet2, ctx->mBool, KP_0_INSERT_BIT);
            return true;
        case KeyboardMouseInputActions::LEFT_CLICK:
            SET_BUTTON_BIT(gConstantData.buttonSet2, ctx->mBool, LEFT_CLICK_BIT);
            return true;
        case KeyboardMouseInputActions::MID_CLICK:
            SET_BUTTON_BIT(gConstantData.buttonSet2, ctx->mBool, MID_CLICK_BIT);
            return true;
        case KeyboardMouseInputActions::RIGHT_CLICK:
            SET_BUTTON_BIT(gConstantData.buttonSet2, ctx->mBool, RIGHT_CLICK_BIT);
            return true;
        case KeyboardMouseInputActions::SCROLL_UP:
            SET_BUTTON_BIT(gConstantData.buttonSet2, ctx->mBool, SCROLL_UP_BIT);
            return true;
        case KeyboardMouseInputActions::SCROLL_DOWN:
            SET_BUTTON_BIT(gConstantData.buttonSet3, ctx->mBool, SCROLL_DOWN_BIT);
            return true;
        }
        break;
    }
    }

    return true;
};

bool TouchChecker(InputActionContext* ctx)
{
    UNREF_PARAM(ctx);
#ifdef TOUCH_INPUT
    if (gConstantData.deviceType == InputDeviceType::INPUT_DEVICE_TOUCH)
    {
        uint32_t actionId = ctx->mActionId;
        // Map left click to touch input
        if (actionId == TouchInputActions::UI_MOUSE_LEFT)
        {
            actionId = TouchInputActions::TAP;
        }
        gGestureContexts[actionId].mCtx = *ctx;
        gGestureContexts[actionId].mCtx.mActionId = actionId;
        gGestureContexts[actionId].mTime = 0.0f;
        gGestureContexts[actionId].mPrinted = true;
    }
#endif

    return true;
}

IApp* gUnitTestApp = NULL;

void OnDeviceSwitch(void* pUserData)
{
    UNREF_PARAM(pUserData);
    waitQueueIdle(pGraphicsQueue);
    gConstantData.deviceType = gChosenDeviceType + 1;

    switch (gConstantData.deviceType)
    {
    case InputDeviceType::INPUT_DEVICE_GAMEPAD:
    {
        pInputDeviceTexture = pGamepad;

        addActionMappings(gControllerActionMappings, TF_ARRAY_COUNT(gControllerActionMappings), INPUT_ACTION_MAPPING_TARGET_ALL);

        InputActionDesc actionDesc = { ControllerInputActions::DUMP_PROFILE_DATA,
                                       [](InputActionContext* ctx)
                                       {
                                           dumpProfileData(((Renderer*)ctx->pUserData)->pName);
                                           return true;
                                       },
                                       pRenderer };
        addInputAction(&actionDesc);
        actionDesc = { ControllerInputActions::TOGGLE_FULLSCREEN,
                       [](InputActionContext* ctx)
                       {
                           WindowDesc* winDesc = ((IApp*)ctx->pUserData)->pWindow;
                           if (winDesc->fullScreen)
                               winDesc->borderlessWindow
                                   ? setBorderless(winDesc, getRectWidth(&winDesc->clientRect), getRectHeight(&winDesc->clientRect))
                                   : setWindowed(winDesc, getRectWidth(&winDesc->clientRect), getRectHeight(&winDesc->clientRect));
                           else
                               setFullscreen(winDesc);
                           return true;
                       },
                       gUnitTestApp };
        addInputAction(&actionDesc);

        actionDesc = { ControllerInputActions::LEFT_STICK, StickChecker };
        addInputAction(&actionDesc);
        actionDesc = { ControllerInputActions::RIGHT_STICK, StickChecker };
        addInputAction(&actionDesc);

        actionDesc = { ControllerInputActions::FLOAT_L2, KeyChecker };
        addInputAction(&actionDesc);
        actionDesc = { ControllerInputActions::FLOAT_R2, KeyChecker };
        addInputAction(&actionDesc);

        for (uint32_t i = ControllerInputActions::A; i < ControllerInputActions::DUMP_PROFILE_DATA; ++i)
        {
            actionDesc = { i, KeyChecker };
            addInputAction(&actionDesc);
        }

        break;
    }
    case InputDeviceType::INPUT_DEVICE_TOUCH:
    {
#ifdef TOUCH_INPUT
        addActionMappings(gTouchActionMappings, TF_ARRAY_COUNT(gTouchActionMappings), INPUT_ACTION_MAPPING_TARGET_ALL);
        InputActionDesc actionDesc;
        for (uint32_t i = 0; i < TouchInputActions::COUNT; ++i)
        {
            actionDesc = { i, TouchChecker };
            addInputAction(&actionDesc);
        }
#endif
        break;
    }
    case InputDeviceType::INPUT_DEVICE_KEYBOARD:
    case InputDeviceType::INPUT_DEVICE_MOUSE:
    {
        pInputDeviceTexture = pKeyboardMouse;

        addActionMappings(gKeyboardMouseActionMappings, TF_ARRAY_COUNT(gKeyboardMouseActionMappings), INPUT_ACTION_MAPPING_TARGET_ALL);

        InputActionDesc actionDesc = { KeyboardMouseInputActions::DUMP_PROFILE_DATA,
                                       [](InputActionContext* ctx)
                                       {
                                           dumpProfileData(((Renderer*)ctx->pUserData)->pName);
                                           return true;
                                       },
                                       pRenderer };
        addInputAction(&actionDesc);
        actionDesc = { KeyboardMouseInputActions::TOGGLE_FULLSCREEN,
                       [](InputActionContext* ctx)
                       {
                           WindowDesc* winDesc = ((IApp*)ctx->pUserData)->pWindow;
                           if (winDesc->fullScreen)
                               winDesc->borderlessWindow
                                   ? setBorderless(winDesc, getRectWidth(&winDesc->clientRect), getRectHeight(&winDesc->clientRect))
                                   : setWindowed(winDesc, getRectWidth(&winDesc->clientRect), getRectHeight(&winDesc->clientRect));
                           else
                               setFullscreen(winDesc);
                           return true;
                       },
                       gUnitTestApp };
        addInputAction(&actionDesc);

        for (uint32_t i = 0u; i < KeyboardMouseInputActions::DUMP_PROFILE_DATA; ++i)
        {
            actionDesc = { i, KeyChecker };
            addInputAction(&actionDesc);
        }

        break;
    }
    }
};

void OnLightsAndRumbleSwitch(void* pUserData)
{
    UNREF_PARAM(pUserData);
    // Reset controller events
    setRumbleEffect(0, 0, 0, 0);
    setLEDColor(0, 0xFF, 0xCB, 0x00); // TFI Yellow: #FFCB00
}

void DoRumbleAndLightsTestSequence(float deltaTime)
{
    static float total = 5;
    static int   doNext = 0;

    total += deltaTime;
    int32_t controllerID = gChosenDeviceType == 1 ? BUILTIN_DEVICE_HAPTICS : 0;
    if (total >= 5)
    {
        switch (doNext)
        {
        case 0:
            setRumbleEffect(controllerID, 1, 0, 1000);
            setLEDColor(0, 255, 0, 0);
            break;
        case 1:
            setRumbleEffect(controllerID, 0, .5f, 2000);
            setLEDColor(0, 0, 255, 0);
            break;
        case 2:
            setRumbleEffect(controllerID, .2f, 0, 3000);
            setLEDColor(0, 0, 0, 255);
            break;
        case 3:
            setRumbleEffect(controllerID, 0, 1, 4000);
            setLEDColor(0, 255, 0, 255);
            break;
        case 4:
            setRumbleEffect(controllerID, .3f, .3f, 5000);
            setLEDColor(0, 255, 255, 0);
            break;
        case 5:
            setRumbleEffect(controllerID, 1, 1, 8000);
            setLEDColor(0, 0, 255, 255);
            break;

        default:
            break;
        }

        doNext = (doNext + 1) % 6;
        total = 0;
    }
}

class Input: public IApp
{
public:
    Input()
    {
        gUnitTestApp = this;

        mSettings.mCentered = true;
        mSettings.mBorderlessWindow = false;
        mSettings.mForceLowDPI = true;

        gConstantData.deviceType = gChosenDeviceType + 1;
    }

    bool Init()
    {
        // FILE PATHS
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SHADER_BINARIES, "CompiledShaders");
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_GPU_CONFIG, "GPUCfg");
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_TEXTURES, "Textures");
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_FONTS, "Fonts");
        fsSetPathForResourceDir(pSystemFileIO, RM_DEBUG, RD_SCREENSHOTS, "Screenshots");
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SCRIPTS, "Scripts");
        fsSetPathForResourceDir(pSystemFileIO, RM_DEBUG, RD_DEBUG, "Debug");

        // window and renderer setup
        RendererDesc settings;
        memset(&settings, 0, sizeof(settings));
        initRenderer(GetName(), &settings, &pRenderer);
        // check for init success
        if (!pRenderer)
            return false;

        QueueDesc queueDesc = {};
        queueDesc.mType = QUEUE_TYPE_GRAPHICS;
        queueDesc.mFlag = QUEUE_FLAG_INIT_MICROPROFILE;
        addQueue(pRenderer, &queueDesc, &pGraphicsQueue);

        GpuCmdRingDesc cmdRingDesc = {};
        cmdRingDesc.pQueue = pGraphicsQueue;
        cmdRingDesc.mPoolCount = gDataBufferCount;
        cmdRingDesc.mCmdPerPoolCount = 1;
        cmdRingDesc.mAddSyncPrimitives = true;
        addGpuCmdRing(pRenderer, &cmdRingDesc, &gGraphicsCmdRing);

        addSemaphore(pRenderer, &pImageAcquiredSemaphore);

        initResourceLoaderInterface(pRenderer);

        TextureLoadDesc textureDesc = {};
        textureDesc.pFileName = "controller.tex";
        textureDesc.ppTexture = &pGamepad;
        addResource(&textureDesc, NULL);

        textureDesc.pFileName = "keyboard+mouse.tex";
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

        UIComponentDesc guiDesc = {};
        guiDesc.mStartPosition += vec2(mSettings.mWidth * 0.01f, mSettings.mHeight * 0.5f);
        uiCreateComponent(GetName(), &guiDesc, &pGuiWindow);

        static const char* enumNames[] = { "Gamepad", "Touch", "Keyboard+Mouse" };

        DropdownWidget deviceDropdown;
        deviceDropdown.pData = &gChosenDeviceType;
        deviceDropdown.pNames = enumNames;
        deviceDropdown.mCount = sizeof(enumNames) / sizeof(enumNames[0]);

        UIWidget* pRunScript = uiCreateComponentWidget(pGuiWindow, "Device Type: ", &deviceDropdown, WIDGET_TYPE_DROPDOWN);
        uiSetWidgetOnEditedCallback(pRunScript, nullptr, OnDeviceSwitch);
        luaRegisterWidget(pRunScript);

        CheckboxWidget lightsAndRumbleCheckbox;
        lightsAndRumbleCheckbox.pData = &gEnableRumbleAndLights;

        UIWidget* pLARWidget =
            uiCreateComponentWidget(pGuiWindow, "Enable Rumble and Lights Test Sequence:", &lightsAndRumbleCheckbox, WIDGET_TYPE_CHECKBOX);
        uiSetWidgetOnEditedCallback(pLARWidget, nullptr, OnLightsAndRumbleSwitch);
        luaRegisterWidget(pRunScript);

        // Initialize micro profiler and its UI.
        ProfilerDesc profiler = {};
        profiler.pRenderer = pRenderer;
        profiler.mWidthUI = mSettings.mWidth;
        profiler.mHeightUI = mSettings.mHeight;
        initProfiler(&profiler);

        // Gpu profiler can only be added after initProfiler.
        gGpuProfileToken = addGpuProfiler(pRenderer, pGraphicsQueue, "Graphics");

        InputSystemDesc inputDesc = {};
        inputDesc.pRenderer = pRenderer;
        inputDesc.pWindow = pWindow;
        inputDesc.pJoystickTexture = "circlepad.tex";
        if (!initInputSystem(&inputDesc))
            return false;

        // UI input
        InputActionCallback onUIInput = [](InputActionContext* ctx)
        {
            if (ctx->mActionId > UISystemInputActions::UI_ACTION_START_ID_)
                uiOnInput(ctx->mActionId, ctx->mBool, ctx->pPosition, &ctx->mFloat2);

            return true;
        };

        GlobalInputActionDesc globalInputActionDesc = { GlobalInputActionDesc::ANY_BUTTON_ACTION, onUIInput, this };
        setGlobalInputAction(&globalInputActionDesc);

        // Setup action mappings and callbacks
        OnDeviceSwitch(nullptr);

        waitForAllResourceLoads();

        return true;
    }

    void Exit()
    {
        waitQueueIdle(pGraphicsQueue);

        exitInputSystem();

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

        removeGpuCmdRing(pRenderer, &gGraphicsCmdRing);
        removeSemaphore(pRenderer, pImageAcquiredSemaphore);

        exitResourceLoaderInterface(pRenderer);
        removeQueue(pRenderer, pGraphicsQueue);
        exitRenderer(pRenderer);
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

        initScreenshotInterface(pRenderer, pGraphicsQueue);

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
        }

        if (pReloadDesc->mType & RELOAD_TYPE_SHADER)
        {
            removeDescriptorSets();
            removeRootSignatures();
            removeShaders();
        }

        exitScreenshotInterface();
    }

    void Update(float deltaTime)
    {
        updateInputSystem(deltaTime, mSettings.mWidth, mSettings.mHeight);
#ifdef TOUCH_INPUT
        if (gChosenDeviceType + 1 == INPUT_DEVICE_TOUCH)
        {
            for (uint32_t i = 0; i < TF_ARRAY_COUNT(gGestureContexts) - 1; ++i)
            {
                gGestureContexts[i].mTime += deltaTime;

                if (gGestureContexts[i].mTime > gGestureTextTimeout)
                {
                    gGestureContexts[i].mPrinted = false;
                }
            }
        }
#endif

        if (pInputDeviceTexture != pPrevInputDeviceTexture)
        {
            DescriptorData param = {};
            param.pName = "uTexture";
            param.ppTextures = &pInputDeviceTexture;
            updateDescriptorSet(pRenderer, 0, pDescriptorDevice, 1, &param);

            pPrevInputDeviceTexture = pInputDeviceTexture;
        }

        if (gEnableRumbleAndLights)
            DoRumbleAndLightsTestSequence(deltaTime);
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

        float yTxtOffset = 12.f;
        float xTxtOffset = 8.f;
        float yTxtOrig = yTxtOffset;

        gFrameTimeDraw.mFontColor = 0xff00ffff;
        gFrameTimeDraw.mFontID = gFontID;

        float2 txtSizePx = cmdDrawCpuProfile(cmd, float2(xTxtOffset, yTxtOrig), &gFrameTimeDraw);
        yTxtOrig += txtSizePx.y + 7 * yTxtOffset;

        txtSizePx = cmdDrawGpuProfile(cmd, float2(xTxtOffset, yTxtOrig), gGpuProfileToken, &gFrameTimeDraw);
        yTxtOrig += txtSizePx.y + yTxtOffset;
#ifdef TOUCH_INPUT
        yTxtOffset = 32.f;
        txtSizePx.y = 15.0f;
        if (gChosenDeviceType + 1 == INPUT_DEVICE_TOUCH)
        {
            for (uint32_t i = 0; i < TF_ARRAY_COUNT(gGestureContexts) - 1; ++i)
            {
                if (gGestureContexts[i].mPrinted)
                {
                    InputActionContext& ctx = gGestureContexts[i].mCtx;
                    const int           textSize = 256;
                    char                gestureText[textSize] = { 0 };
                    switch (gGestureContexts[i].mCtx.mActionId)
                    {
                    case TouchInputActions::TAP:
                        snprintf(gestureText, textSize, "TAP: Position: { %f : %f }", ctx.pPosition->getX(), ctx.pPosition->getY());
                        break;
                    case TouchInputActions::PAN:
                        snprintf(gestureText, textSize, "PAN");
                        break;
                    case TouchInputActions::DOUBLE_TAP:
                        snprintf(gestureText, textSize, "DOUBLE TAP: Position: { %f : %f }", ctx.pPosition->getX(), ctx.pPosition->getY());
                        break;
                    case TouchInputActions::SWIPE:
                        snprintf(gestureText, textSize, "SWIPE: Distance Traveled: { %f : %f } | Direction: { %f | %f }", ctx.mFloat4.x,
                                 ctx.mFloat4.y, ctx.mFloat4.z, ctx.mFloat4.w);
                        break;
                    case TouchInputActions::PINCH:
                        snprintf(gestureText, textSize, "PINCH: Velocity: %f | Scale: %f | Distance: { %f | %f }", ctx.mFloat4.x,
                                 ctx.mFloat4.y, ctx.mFloat4.z, ctx.mFloat4.w);
                        break;
                    case TouchInputActions::ROTATE:
                        snprintf(gestureText, textSize, "ROTATE: Velocity: %f | Rotation: %f | Distance: { %f | %f }", ctx.mFloat4.x,
                                 ctx.mFloat4.y, ctx.mFloat4.z, ctx.mFloat4.w);
                        break;
                    case TouchInputActions::LONG_PRESS:
                        if (ctx.mBool)
                            snprintf(gestureText, textSize, "LONG PRESS DETECTED: Position: { %f : %f }", ctx.pPosition->getX(),
                                     ctx.pPosition->getY());
                        else
                            snprintf(gestureText, textSize, "LONG PRESS CANCELED: Position: { %f : %f }", ctx.pPosition->getX(),
                                     ctx.pPosition->getY());
                        break;
                    default:
                        break;
                    }

                    gFrameTimeDraw.pText = gestureText;
                    cmdDrawTextWithFont(cmd, float2(xTxtOffset, yTxtOrig), &gFrameTimeDraw);
                    yTxtOrig += txtSizePx.y + yTxtOffset;
                }
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
        basicShader.mStages[0].pFileName = "basic.vert";
        basicShader.mStages[1].pFileName = "basic.frag";
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
        case InputDeviceType::INPUT_DEVICE_GAMEPAD:
            param.ppTextures = &pGamepad;
            updateDescriptorSet(pRenderer, 0, pDescriptorDevice, 1, &param);
            break;
        case InputDeviceType::INPUT_DEVICE_TOUCH:
            break;
        case InputDeviceType::INPUT_DEVICE_KEYBOARD:
        case InputDeviceType::INPUT_DEVICE_MOUSE:
            param.ppTextures = &pKeyboardMouse;
            updateDescriptorSet(pRenderer, 0, pDescriptorDevice, 1, &param);
            break;
        }

        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            DescriptorData uParam = {};
            uParam.pName = "InputData";
            uParam.ppBuffers = &pInputDataUniformBuffer[i];
            updateDescriptorSet(pRenderer, i, pDescriptorInputData, 1, &uParam);
        }
    }
};
DEFINE_APPLICATION_MAIN(Input)
