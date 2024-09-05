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

#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <X11/Xutil.h>

#include "../Interfaces/IOperatingSystem.h"
#include "../../Utilities/Interfaces/ILog.h"
#include "../../Utilities/Interfaces/IThread.h"
#include "../../Utilities/Threading/Atomics.h"
#include "../../Utilities/Math/MathTypes.h"
#include "../Interfaces/IInput.h"

#include "../Input/InputCommon.h"
#include "../Input/HID/HIDInput.h"

#include "../../Utilities/Interfaces/IMemory.h"

static WindowDesc* pWindow = {};
static int32_t     gPrevCursorPos[2] = {};
static int32_t     gCursorPos[2] = {};
static int32_t     gRawMouseDelta[2] = {};
/************************************************************************/
// Gamepad
/************************************************************************/

static InputPortIndex GamepadFindEmptySlot()
{
    for (uint32_t portIndex = 0; portIndex < MAX_GAMEPADS; ++portIndex)
    {
        if (!inputGamepadIsActive(portIndex))
        {
            return portIndex;
        }
    }

    return PORT_INDEX_INVALID;
}

InputPortIndex GamepadAddHIDController(const HIDDeviceInfo* pDeviceInfo)
{
    InputPortIndex portIndex = GamepadFindEmptySlot();
    if (PORT_INDEX_INVALID == portIndex)
    {
        return portIndex;
    }

    GamepadResetState(portIndex);
    gGamepads[portIndex].mActive = true;
    gGamepads[portIndex].pName = pDeviceInfo->mName;
    if (gGamepadAddedCb)
    {
        gGamepadAddedCb(portIndex);
    }

    return portIndex;
}

void GamepadRemoveHIDController(InputPortIndex portIndex)
{
    if (portIndex < 0 || portIndex >= (InputPortIndex)MAX_GAMEPADS)
    {
        return;
    }

    if (!inputGamepadIsActive(portIndex))
    {
        return;
    }

    GamepadResetState(portIndex);
    gGamepads[portIndex].mActive = false;
    gGamepads[portIndex].pName = gGamepadDisconnectedName;
    if (gGamepadRemovedCb)
    {
        gGamepadRemovedCb(portIndex);
    }
}
/************************************************************************/
// Keyboard
/************************************************************************/
static inline FORGE_CONSTEXPR InputEnum GetKey(KeySym key)
{
    switch (key)
    {
    case XK_Escape:
        return K_ESCAPE;
    case XK_F1:
        return K_F1;
    case XK_F2:
        return K_F2;
    case XK_F3:
        return K_F3;
    case XK_F4:
        return K_F4;
    case XK_F5:
        return K_F5;
    case XK_F6:
        return K_F6;
    case XK_F7:
        return K_F7;
    case XK_F8:
        return K_F8;
    case XK_F9:
        return K_F9;
    case XK_F10:
        return K_F10;
    case XK_F11:
        return K_F11;
    case XK_F12:
        return K_F12;
    case XK_Print:
        return K_PRINTSCREEN;
    case XK_Scroll_Lock:
        return K_SCROLLLOCK;
    case XK_Pause:
        return K_PAUSE;

    case XK_space:
        return K_SPACE;

    case XK_apostrophe:
        return K_APOSTROPHE;
    case XK_comma:
        return K_COMMA;
    case XK_minus:
        return K_MINUS;
    case XK_period:
        return K_PERIOD;
    case XK_slash:
        return K_SLASH;

    case XK_0:
        return K_0;
    case XK_1:
        return K_1;
    case XK_2:
        return K_2;
    case XK_3:
        return K_3;
    case XK_4:
        return K_4;
    case XK_5:
        return K_5;
    case XK_6:
        return K_6;
    case XK_7:
        return K_7;
    case XK_8:
        return K_8;
    case XK_9:
        return K_9;

    case XK_semicolon:
        return K_SEMICOLON;
    case XK_less:
        return K_COMMA;
    case XK_equal:
        return K_EQUAL;

    case XK_a:
        return K_A;
    case XK_b:
        return K_B;
    case XK_c:
        return K_C;
    case XK_d:
        return K_D;
    case XK_e:
        return K_E;
    case XK_f:
        return K_F;
    case XK_g:
        return K_G;
    case XK_h:
        return K_H;
    case XK_i:
        return K_I;
    case XK_j:
        return K_J;
    case XK_k:
        return K_K;
    case XK_l:
        return K_L;
    case XK_m:
        return K_M;
    case XK_n:
        return K_N;
    case XK_o:
        return K_O;
    case XK_p:
        return K_P;
    case XK_q:
        return K_Q;
    case XK_r:
        return K_R;
    case XK_s:
        return K_S;
    case XK_t:
        return K_T;
    case XK_u:
        return K_U;
    case XK_v:
        return K_V;
    case XK_w:
        return K_W;
    case XK_x:
        return K_X;
    case XK_y:
        return K_Y;
    case XK_z:
        return K_Z;

    case XK_bracketleft:
        return K_LEFTBRACKET;
    case XK_backslash:
        return K_BACKSLASH;
    case XK_bracketright:
        return K_RIGHTBRACKET;

    case XK_grave:
        return K_GRAVE;

    case XK_Left:
        return K_LEFTARROW;
    case XK_Right:
        return K_RIGHTARROW;
    case XK_Up:
        return K_UPARROW;
    case XK_Down:
        return K_DOWNARROW;
    case XK_Insert:
        return K_INS;
    case XK_Home:
        return K_HOME;
    case XK_Delete:
        return K_DEL;
    case XK_End:
        return K_END;
    case XK_Page_Up:
        return K_PGUP;
    case XK_Page_Down:
        return K_PGDN;

    case XK_Num_Lock:
        return K_KP_NUMLOCK;
    case XK_KP_Divide:
        return K_KP_SLASH;
    case XK_KP_Multiply:
        return K_KP_STAR;
    case XK_KP_Subtract:
        return K_KP_MINUS;
    case XK_KP_Add:
        return K_KP_PLUS;
    case XK_KP_Enter:
        return K_KP_ENTER;
    case XK_KP_Insert:
        return K_KP_INS;
    case XK_KP_End:
        return K_KP_END;
    case XK_KP_Down:
        return K_KP_DOWNARROW;
    case XK_KP_Page_Down:
        return K_KP_PGDN;
    case XK_KP_Left:
        return K_KP_LEFTARROW;
    case XK_KP_Begin:
        return INPUT_NONE;
    case XK_KP_Right:
        return K_KP_RIGHTARROW;
    case XK_KP_Home:
        return K_KP_HOME;
    case XK_KP_Up:
        return K_KP_UPARROW;
    case XK_KP_Page_Up:
        return K_KP_PGUP;
    case XK_KP_Delete:
        return K_KP_DEL;

    case XK_BackSpace:
        return K_BACKSPACE;
    case XK_Tab:
        return K_TAB;
    case XK_Return:
        return K_ENTER;
    case XK_Caps_Lock:
        return K_CAPSLOCK;
    case XK_Shift_L:
        return K_LSHIFT;
    case XK_Control_L:
        return K_LCTRL;
    case XK_Super_L:
        return K_LWIN;
    case XK_Alt_L:
        return K_LALT;
    case XK_Alt_R:
        return K_RALT;
    case XK_Super_R:
        return K_RWIN;
    case XK_Menu:
        return K_MENU;
    case XK_Control_R:
        return K_RCTRL;
    case XK_Shift_R:
        return K_RSHIFT;

    case XK_dead_circumflex:
        return INPUT_NONE;
    case XK_ssharp:
        return INPUT_NONE;
    case XK_dead_acute:
        return INPUT_NONE;
    case XK_ISO_Level3_Shift:
        return INPUT_NONE;
    case XK_plus:
        return INPUT_NONE;
    case XK_numbersign:
        return INPUT_NONE;
    case XK_udiaeresis:
        return INPUT_NONE;
    case XK_adiaeresis:
        return INPUT_NONE;
    case XK_odiaeresis:
        return INPUT_NONE;
    case XK_section:
        return INPUT_NONE;
    case XK_aring:
        return INPUT_NONE;
    case XK_dead_diaeresis:
        return INPUT_NONE;
    case XK_twosuperior:
        return INPUT_NONE;
    case XK_parenright:
        return INPUT_NONE;
    case XK_dollar:
        return INPUT_NONE;
    case XK_ugrave:
        return INPUT_NONE;
    case XK_asterisk:
        return INPUT_NONE;
    case XK_colon:
        return INPUT_NONE;
    case XK_exclam:
        return INPUT_NONE;
    default:
        return INPUT_NONE;
    }
    return INPUT_NONE;
}

static void AddKeyChar(char32_t c)
{
    if (gCharacterBufferCount >= TF_ARRAY_COUNT(gCharacterBuffer))
    {
        return;
    }

    gCharacterBuffer[gCharacterBufferCount++] = c;
}
/************************************************************************/
// Platform
/************************************************************************/
void platformInitInput(WindowDesc* winDesc)
{
    pWindow = winDesc;
    int32_t hidRet = hidInit(&winDesc->handle);
    if (hidRet)
    {
        LOGF(eWARNING, "hidInit failed with error %d. Game ontrollers will not work", hidRet);
    }

    InputInitCommon();
}

void platformExitInput()
{
    GamepadDefault();
    // Update one more time to stop rumbles
    hidUpdate();
    hidExit();
}

void platformUpdateLastInputState()
{
    memcpy(gLastInputValues, gInputValues, sizeof(gInputValues));
    gInputValues[MOUSE_WHEEL_UP] = false;
    gInputValues[MOUSE_WHEEL_DOWN] = false;
    gLastInputValues[MOUSE_DX] = 0;
    gLastInputValues[MOUSE_DY] = 0;
    gRawMouseDelta[0] = 0;
    gRawMouseDelta[1] = 0;
    gCharacterBufferCount = 0;
}

void platformUpdateInput(float deltaTime)
{
    hidUpdate();

    gInputValues[MOUSE_X] = gCursorPos[0];
    gInputValues[MOUSE_Y] = gCursorPos[1];
    gInputValues[MOUSE_DX] = gRawMouseDelta[0];
    gInputValues[MOUSE_DY] = -gRawMouseDelta[1];
    gDeltaTime = deltaTime;

    extern bool gCaptureCursorOnMouseDown;
    if (gCaptureCursorOnMouseDown)
    {
#if defined(ENABLE_FORGE_UI)
        extern bool uiIsFocused();
        const bool  capture = !uiIsFocused();
#else
        const bool capture = true;
#endif
        captureCursor(pWindow, capture && inputGetValue(0, MOUSE_1));
    }
}

void platformInputEvent(const XEvent* event)
{
    switch (event->type)
    {
    case MotionNotify:
    {
        const XMotionEvent& motionEvent = event->xmotion;
        gCursorPos[0] = motionEvent.x;
        gCursorPos[1] = motionEvent.y;
        gRawMouseDelta[0] += gCursorPos[0] - gPrevCursorPos[0];
        gRawMouseDelta[1] += gCursorPos[1] - gPrevCursorPos[1];
        gPrevCursorPos[0] = gCursorPos[0];
        gPrevCursorPos[1] = gCursorPos[1];
        break;
    }
    case ButtonPress:
    case ButtonRelease:
    {
        const XButtonEvent& btn = event->xbutton;
        const bool          pressed = event->type == ButtonPress;
        if (Button1 == btn.button)
        {
            gInputValues[MOUSE_1] = pressed;
        }
        else if (Button2 == btn.button)
        {
            gInputValues[MOUSE_3] = pressed;
        }
        else if (Button3 == btn.button)
        {
            gInputValues[MOUSE_2] = pressed;
        }
        else if (pressed && Button4 == btn.button)
        {
            gInputValues[MOUSE_WHEEL_UP] = pressed;
        }
        else if (pressed && Button5 == btn.button)
        {
            gInputValues[MOUSE_WHEEL_DOWN] = pressed;
        }
        break;
    }
    case KeyPress:
    case KeyRelease:
    {
        XKeyEvent  keyEvent = event->xkey;
        KeySym     keySym = XkbKeycodeToKeysym(keyEvent.display, keyEvent.keycode, 0, 0);
        const bool pressed = event->type == KeyPress;
        gInputValues[GetKey(keySym)] = pressed;

        if (pressed)
        {
            char    buf[32];
            int32_t len = XLookupString(&keyEvent, buf, 32, 0, 0);
            for (int32_t c = 0; c < len; ++c)
            {
                AddKeyChar(buf[c]);
            }
        }
        break;
    }
    default:
        break;
    }
}