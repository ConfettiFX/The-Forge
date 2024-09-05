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
#include "../../Utilities/Interfaces/IThread.h"
#include "../../Utilities/Threading/Atomics.h"
#include "../../Utilities/Math/MathTypes.h"
#include "../Interfaces/IInput.h"

#include <XInput.h>
#include <WinDNS.h> // QWORD

#include "../Input/InputCommon.h"
#include "../Input/HID/HIDInput.h"

#include "../../Utilities/Interfaces/IMemory.h"

static const InputEnum gScanCodeToInput[256][2] = {
    { INPUT_NONE, INPUT_NONE },        // 0x00
    { K_ESCAPE, INPUT_NONE },          // 0x01
    { K_1, INPUT_NONE },               // 0x02
    { K_2, INPUT_NONE },               // 0x03
    { K_3, INPUT_NONE },               // 0x04
    { K_4, INPUT_NONE },               // 0x05
    { K_5, INPUT_NONE },               // 0x06
    { K_6, INPUT_NONE },               // 0x07
    { K_7, INPUT_NONE },               // 0x08
    { K_8, INPUT_NONE },               // 0x09
    { K_9, INPUT_NONE },               // 0x0A
    { K_0, INPUT_NONE },               // 0x0B
    { K_MINUS, INPUT_NONE },           // 0x0C
    { K_EQUAL, INPUT_NONE },           // 0x0D
    { K_BACKSPACE, INPUT_NONE },       // 0x0E
    { K_TAB, INPUT_NONE },             // 0x0F
    { K_Q, INPUT_NONE },               // 0x10
    { K_W, INPUT_NONE },               // 0x11
    { K_E, INPUT_NONE },               // 0x12
    { K_R, INPUT_NONE },               // 0x13
    { K_T, INPUT_NONE },               // 0x14
    { K_Y, INPUT_NONE },               // 0x15
    { K_U, INPUT_NONE },               // 0x16
    { K_I, INPUT_NONE },               // 0x17
    { K_O, INPUT_NONE },               // 0x18
    { K_P, INPUT_NONE },               // 0x19
    { K_LEFTBRACKET, INPUT_NONE },     // 0x1A
    { K_RIGHTBRACKET, INPUT_NONE },    // 0x1B
    { K_ENTER, K_KP_ENTER },           // 0x1C
    { K_LCTRL, K_RCTRL },              // 0x1D
    { K_A, INPUT_NONE },               // 0x1E
    { K_S, INPUT_NONE },               // 0x1F
    { K_D, INPUT_NONE },               // 0x20
    { K_F, INPUT_NONE },               // 0x21
    { K_G, INPUT_NONE },               // 0x22
    { K_H, INPUT_NONE },               // 0x23
    { K_J, INPUT_NONE },               // 0x24
    { K_K, INPUT_NONE },               // 0x25
    { K_L, INPUT_NONE },               // 0x26
    { K_SEMICOLON, INPUT_NONE },       // 0x27
    { K_APOSTROPHE, INPUT_NONE },      // 0x28
    { K_GRAVE, INPUT_NONE },           // 0x29
    { K_LSHIFT, INPUT_NONE },          // 0x2A
    { K_BACKSLASH, INPUT_NONE },       // 0x2B
    { K_Z, INPUT_NONE },               // 0x2C
    { K_X, INPUT_NONE },               // 0x2D
    { K_C, INPUT_NONE },               // 0x2E
    { K_V, INPUT_NONE },               // 0x2F
    { K_B, INPUT_NONE },               // 0x30
    { K_N, INPUT_NONE },               // 0x31
    { K_M, INPUT_NONE },               // 0x32
    { K_COMMA, INPUT_NONE },           // 0x33
    { K_PERIOD, INPUT_NONE },          // 0x34
    { K_SLASH, K_KP_SLASH },           // 0x35
    { K_RSHIFT, K_RSHIFT },            // 0x36
    { K_KP_STAR, K_PRINTSCREEN },      // 0x37
    { K_LALT, K_RALT },                // 0x38
    { K_SPACE, INPUT_NONE },           // 0x39
    { K_CAPSLOCK, INPUT_NONE },        // 0x3A
    { K_F1, INPUT_NONE },              // 0x3B
    { K_F2, INPUT_NONE },              // 0x3C
    { K_F3, INPUT_NONE },              // 0x3D
    { K_F4, INPUT_NONE },              // 0x3E
    { K_F5, INPUT_NONE },              // 0x3F
    { K_F6, INPUT_NONE },              // 0x40
    { K_F7, INPUT_NONE },              // 0x41
    { K_F8, INPUT_NONE },              // 0x42
    { K_F9, INPUT_NONE },              // 0x43
    { K_F10, INPUT_NONE },             // 0x44
    { K_PAUSE, K_KP_NUMLOCK },         // 0x45
    { K_SCROLLLOCK, INPUT_NONE },      // 0x46
    { K_KP_HOME, K_HOME },             // 0x47
    { K_KP_UPARROW, K_UPARROW },       // 0x48
    { K_KP_PGUP, K_PGUP },             // 0x49
    { K_KP_MINUS, INPUT_NONE },        // 0x4A
    { K_KP_LEFTARROW, K_LEFTARROW },   // 0x4B
    { K_KP_NUMPAD_5, INPUT_NONE },     // 0x4C
    { K_KP_RIGHTARROW, K_RIGHTARROW }, // 0x4D
    { K_KP_PLUS, INPUT_NONE },         // 0x4E
    { K_KP_END, K_END },               // 0x4F
    { K_KP_DOWNARROW, K_DOWNARROW },   // 0x50
    { K_KP_PGDN, K_PGDN },             // 0x51
    { K_KP_INS, K_INS },               // 0x52
    { K_KP_DEL, K_DEL },               // 0x53
    { K_PRINTSCREEN, INPUT_NONE },     // 0x54
    { INPUT_NONE, INPUT_NONE },        // 0x55
    { INPUT_NONE, INPUT_NONE },        // 0x56
    { K_F11, INPUT_NONE },             // 0x57
    { K_F12, INPUT_NONE },             // 0x58
    { INPUT_NONE, INPUT_NONE },        // 0x59
    { INPUT_NONE, INPUT_NONE },        // 0x5A
    { INPUT_NONE, K_LWIN },            // 0x5B
    { INPUT_NONE, K_RWIN },            // 0x5C
    { INPUT_NONE, K_MENU },            // 0x5D
    { INPUT_NONE, INPUT_NONE },        // 0x5E
    { INPUT_NONE, INPUT_NONE },        // 0x5F
    { INPUT_NONE, INPUT_NONE },        // 0x60
    { INPUT_NONE, INPUT_NONE },        // 0x61
    { INPUT_NONE, INPUT_NONE },        // 0x62
    { INPUT_NONE, INPUT_NONE },        // 0x63
    { K_F13, INPUT_NONE },             // 0x64
    { K_F14, INPUT_NONE },             // 0x65
    { K_F15, INPUT_NONE },             // 0x66
    { INPUT_NONE, INPUT_NONE },        // 0x67
    { INPUT_NONE, INPUT_NONE },        // 0x68
    { INPUT_NONE, INPUT_NONE },        // 0x69
    { INPUT_NONE, INPUT_NONE },        // 0x6A
    { INPUT_NONE, INPUT_NONE },        // 0x6B
    { INPUT_NONE, INPUT_NONE },        // 0x6C
    { INPUT_NONE, INPUT_NONE },        // 0x6D
    { INPUT_NONE, INPUT_NONE },        // 0x6E
    { INPUT_NONE, INPUT_NONE },        // 0x6F
    { INPUT_NONE, INPUT_NONE },        // 0x70
    { INPUT_NONE, INPUT_NONE },        // 0x71
    { INPUT_NONE, INPUT_NONE },        // 0x72
    { INPUT_NONE, INPUT_NONE },        // 0x73
    { INPUT_NONE, INPUT_NONE },        // 0x74
    { INPUT_NONE, INPUT_NONE },        // 0x75
    { INPUT_NONE, INPUT_NONE },        // 0x76
    { INPUT_NONE, INPUT_NONE },        // 0x77
    { INPUT_NONE, INPUT_NONE },        // 0x78
    { INPUT_NONE, INPUT_NONE },        // 0x79
    { INPUT_NONE, INPUT_NONE },        // 0x7A
    { INPUT_NONE, INPUT_NONE },        // 0x7B
    { INPUT_NONE, INPUT_NONE },        // 0x7C
    { INPUT_NONE, INPUT_NONE },        // 0x7D
};

struct XGamepad
{
    XINPUT_CAPABILITIES mCaps;
    InputPortIndex      mPortIndex;
    bool                mActive;
};

static XGamepad gXGamepads[MAX_GAMEPADS] = {};
static int32_t  gCursorPos[2] = {};
static int32_t  gRawMouseDelta[2] = {};

static WindowDesc* pWindow = {};
/************************************************************************/
// Gamepad
/************************************************************************/

InputPortIndex GamepadFindEmptySlot()
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

InputPortIndex GamepadAddHIDController(const HIDDeviceInfo* pDeviceInfo)
{
    UNREF_PARAM(pDeviceInfo);
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
    if (portIndex < 0 || portIndex >= MAX_GAMEPADS)
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

static void GamepadRefresh(uint32_t index)
{
    static const char* xinputControllers[MAX_GAMEPADS] = {
        "XInput Gamepad 0", "XInput Gamepad 1", "XInput Gamepad 2", "XInput Gamepad 3",
        "XInput Gamepad 4", "XInput Gamepad 5", "XInput Gamepad 6", "XInput Gamepad 7",
    };
    XGamepad&           xgpad = gXGamepads[index];
    bool                previousStatus = xgpad.mActive;
    XINPUT_CAPABILITIES caps;
    if (ERROR_SUCCESS == XInputGetCapabilities(index, XINPUT_FLAG_GAMEPAD, &caps))
    {
        // New XInput controller was connected
        if (!previousStatus)
        {
            // Try to find slot for new controller in the gamepad array
            InputPortIndex portIndex = GamepadFindEmptySlot();
            // If all slots full, cant accomodate new controller
            if (PORT_INDEX_INVALID != portIndex)
            {
                xgpad = {};
                xgpad.mPortIndex = portIndex;
                xgpad.mActive = true;
                gGamepads[xgpad.mPortIndex].mActive = true;
                gGamepads[xgpad.mPortIndex].pName = xinputControllers[index];

                if (gGamepadAddedCb)
                {
                    gGamepadAddedCb(portIndex);
                }
            }
        }
        xgpad.mCaps = caps;
    }
    else
    {
        // New XInput controller was disconnected
        if (xgpad.mActive)
        {
            xgpad.mActive = false;
            // Reset all controls
            GamepadResetState(xgpad.mPortIndex);
            gGamepads[xgpad.mPortIndex].mActive = false;
            gGamepads[xgpad.mPortIndex].pName = gGamepadDisconnectedName;
            if (gGamepadRemovedCb)
            {
                gGamepadRemovedCb(index);
            }
        }
    }
}

static void GamepadUpdateState(uint32_t index)
{
    XGamepad& xgpad = gXGamepads[index];
    if (!xgpad.mActive)
    {
        return;
    }

    // Port index is the index in the gGamepads array
    // 1:1 map not possible between the arrays as we also have HID controllers
    // Index passed to XInput only considers the XInput controllers so we have to map the XInput index correctly so we can use both XInput
    // and HID controllers simultaneously
    InputPortIndex portIndex = xgpad.mPortIndex;
    Gamepad&       gpad = gGamepads[portIndex];

    XINPUT_STATE state = {};
    GamepadUpdateLastState(portIndex);
    bool active = XInputGetState(index, &state) == ERROR_SUCCESS;
    if (!active)
    {
        GamepadResetState(portIndex);
        return;
    }

    const WORD buttons = state.Gamepad.wButtons;
    gpad.mButtons[GPAD_UP - GPAD_BTN_FIRST] = ((buttons & XINPUT_GAMEPAD_DPAD_UP) != 0);
    gpad.mButtons[GPAD_DOWN - GPAD_BTN_FIRST] = ((buttons & XINPUT_GAMEPAD_DPAD_DOWN) != 0);
    gpad.mButtons[GPAD_LEFT - GPAD_BTN_FIRST] = ((buttons & XINPUT_GAMEPAD_DPAD_LEFT) != 0);
    gpad.mButtons[GPAD_RIGHT - GPAD_BTN_FIRST] = ((buttons & XINPUT_GAMEPAD_DPAD_RIGHT) != 0);
    gpad.mButtons[GPAD_A - GPAD_BTN_FIRST] = ((buttons & XINPUT_GAMEPAD_A) != 0);
    gpad.mButtons[GPAD_B - GPAD_BTN_FIRST] = ((buttons & XINPUT_GAMEPAD_B) != 0);
    gpad.mButtons[GPAD_X - GPAD_BTN_FIRST] = ((buttons & XINPUT_GAMEPAD_X) != 0);
    gpad.mButtons[GPAD_Y - GPAD_BTN_FIRST] = ((buttons & XINPUT_GAMEPAD_Y) != 0);
    gpad.mButtons[GPAD_START - GPAD_BTN_FIRST] = ((buttons & XINPUT_GAMEPAD_START) != 0);
    gpad.mButtons[GPAD_BACK - GPAD_BTN_FIRST] = ((buttons & XINPUT_GAMEPAD_BACK) != 0);
    gpad.mButtons[GPAD_L3 - GPAD_BTN_FIRST] = ((buttons & XINPUT_GAMEPAD_LEFT_THUMB) != 0);
    gpad.mButtons[GPAD_R3 - GPAD_BTN_FIRST] = ((buttons & XINPUT_GAMEPAD_RIGHT_THUMB) != 0);
    gpad.mButtons[GPAD_L1 - GPAD_BTN_FIRST] = ((buttons & XINPUT_GAMEPAD_LEFT_SHOULDER) != 0);
    gpad.mButtons[GPAD_R1 - GPAD_BTN_FIRST] = ((buttons & XINPUT_GAMEPAD_RIGHT_SHOULDER) != 0);

    gpad.mAxis[GPAD_LX - GPAD_AXIS_FIRST] = state.Gamepad.sThumbLX / float(INT16_MAX);
    gpad.mAxis[GPAD_LY - GPAD_AXIS_FIRST] = state.Gamepad.sThumbLY / float(INT16_MAX);
    gpad.mAxis[GPAD_RX - GPAD_AXIS_FIRST] = state.Gamepad.sThumbRX / float(INT16_MAX);
    gpad.mAxis[GPAD_RY - GPAD_AXIS_FIRST] = state.Gamepad.sThumbRY / float(INT16_MAX);
    gpad.mAxis[GPAD_L2 - GPAD_AXIS_FIRST] = state.Gamepad.bLeftTrigger / float(UINT8_MAX);
    gpad.mAxis[GPAD_R2 - GPAD_AXIS_FIRST] = state.Gamepad.bRightTrigger / float(UINT8_MAX);

    static constexpr float XINPUT_MAX_RUMBLE = 65535.0f;
    bool                   stopRumble = false;
    if (gpad.mRumbleHigh == 0.0f && gpad.mRumbleLow == 0.0f)
    {
        stopRumble = true;
    }
    // Dont keep setting zero rumble
    if (!gpad.mRumbleStopped)
    {
        XINPUT_VIBRATION rumble = {};
        rumble.wLeftMotorSpeed = (WORD)(gpad.mRumbleLow * XINPUT_MAX_RUMBLE);
        rumble.wRightMotorSpeed = (WORD)(gpad.mRumbleHigh * XINPUT_MAX_RUMBLE);
        XInputSetState(index, &rumble);
    }
    gpad.mRumbleStopped = stopRumble;

    // Apply deadzones, ...
    GamepadPostProcess(portIndex);
}
/************************************************************************/
// Raw mouse
/************************************************************************/
static ThreadHandle  gRawInputThread = {};
static bool          gRequestExitRawInputThread = {};
static volatile long gRelativeMouseLock = {};
static HWND          gRawInputHwnd = {};
static ATOM          gRawInputClass = {};

inline void WaitInterlockedCompareExchange(volatile long* destination, int32_t value, int comperand)
{
    // the first (*destination != comperand) is required to prevent deadlocks with out interlocked exchange works in a tight loop
    while ((*destination != comperand) || (InterlockedCompareExchange(destination, value, comperand) != comperand))
    {
        threadSleep(0);
    }
}

static void RelativeMouseLock() { WaitInterlockedCompareExchange(&gRelativeMouseLock, 1, 0); }

static void RelativeMouseUnlock() { InterlockedCompareExchange(&gRelativeMouseLock, 0, 1); }

static void ProcessRawMouseInput(const HANDLE hDevice, const RAWMOUSE& mouse)
{
    UNREF_PARAM(hDevice);
    // Ignore mouse wheel. Handled by event WM_MOUSEWHEEL
    if (mouse.usButtonFlags & RI_MOUSE_WHEEL)
    {
        return;
    }

    const bool absoluteMouseMovement = (mouse.usFlags & MOUSE_MOVE_ABSOLUTE);
    if (absoluteMouseMovement)
    {
        // #TODO
    }
    else if (mouse.lLastX || mouse.lLastY)
    {
        RelativeMouseLock();
        gRawMouseDelta[0] += mouse.lLastX;
        gRawMouseDelta[1] += -mouse.lLastY;
        RelativeMouseUnlock();
    }
}

void ProcessBufferedRawInput()
{
    // Allocate a 1KB buffer for raw mouse input.
    static RAWINPUT eventBuffer[1024 / sizeof(RAWINPUT)];
    UINT            bytes = sizeof(eventBuffer);

    // Loop through reading raw input until no events are left,
    while (true)
    {
        // Fill up buffer,
        UINT count = ::GetRawInputBuffer(eventBuffer, &bytes, sizeof(RAWINPUTHEADER));
        if (count <= 0)
        {
            break;
        }

        // Process all the events,
        const RAWINPUT* itr = eventBuffer;
        while (true)
        {
            // Process mouse event.
            if (itr->header.dwType == RIM_TYPEMOUSE)
            {
                ProcessRawMouseInput(itr->header.hDevice, itr->data.mouse);
            }

            // Go to next event.
            --count;
            if (count <= 0)
            {
                break;
            }
            itr = NEXTRAWINPUTBLOCK(itr);
        }
    }
}

static void RawInputThread(void*)
{
#define FORGE_RAWINPUT_CLASS L"ForgeRawInput"
    if (!gRawInputClass)
    {
        WNDCLASSEXW wce = {};
        wce.cbSize = sizeof(wce);
        wce.lpfnWndProc = DefWindowProcW;
        wce.hInstance = GetModuleHandle(NULL);
        wce.lpszClassName = FORGE_RAWINPUT_CLASS;
        gRawInputClass = RegisterClassExW(&wce);
        ASSERT(gRawInputClass);
        if (!gRawInputClass) //-V547
        {
            return;
        }
    }

    gRawInputHwnd = CreateWindowExW(0, FORGE_RAWINPUT_CLASS, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, GetModuleHandle(NULL), NULL);

#ifndef HID_USAGE_PAGE_GENERIC
#define HID_USAGE_PAGE_GENERIC ((USHORT)0x01)
#endif
#ifndef HID_USAGE_GENERIC_MOUSE
#define HID_USAGE_GENERIC_MOUSE ((USHORT)0x02)
#endif
    RAWINPUTDEVICE Rid[1] = {};
    Rid[0].usUsagePage = HID_USAGE_PAGE_GENERIC;
    Rid[0].usUsage = HID_USAGE_GENERIC_MOUSE;
    Rid[0].dwFlags = RIDEV_INPUTSINK;
    Rid[0].hwndTarget = gRawInputHwnd;
    ::RegisterRawInputDevices(Rid, 1, sizeof(Rid[0]));

    for (;;)
    {
        MSG msg;
        __try
        {
            while (::PeekMessageW(&msg, gRawInputHwnd, 0, WM_INPUT - 1, PM_NOYIELD | PM_REMOVE))
            {
                ::TranslateMessage(&msg);
                ::DispatchMessageW(&msg);
            }

            ProcessBufferedRawInput();

            while (::PeekMessageW(&msg, gRawInputHwnd, WM_INPUT + 1, 0xFFFFFFFF, PM_NOYIELD | PM_REMOVE))
            {
                ::TranslateMessage(&msg);
                ::DispatchMessageW(&msg);
            }
        }
        __except (EXCEPTION_CONTINUE_EXECUTION)
        {
            LOGF(eINFO, "RawInput: ignoring external crash\n");
            break;
        }

        if (!::PeekMessageW(&msg, gRawInputHwnd, 0, 0, PM_NOYIELD | PM_NOREMOVE))
        {
            ::WaitMessage();
        }

        if (gRequestExitRawInputThread)
        {
            break;
        }
    }

    CloseWindow(gRawInputHwnd);
    gRawInputHwnd = {};
}
/************************************************************************/
// Keyboard
/************************************************************************/
static void ProcessKeyEvent(const MSG* msg)
{
    const WORD     keyInfo = HIWORD(msg->lParam);
    const uint8_t  scanCode = keyInfo & 0xff;
    const bool     isExtendedKey = keyInfo & KF_EXTENDED;
    const bool     isMenuKeyDown = keyInfo & KF_ALTDOWN;
    const uint32_t virtualKey = (uint32_t)(msg->wParam);

    if (isMenuKeyDown && (virtualKey == VK_TAB))
    {
        return;
    }

    bool keyDown = false;
    if (WM_KEYDOWN == msg->message || WM_SYSKEYDOWN == msg->message)
    {
        keyDown = true;
    }

    ASSERT(scanCode < TF_ARRAY_COUNT(gScanCodeToInput));
    InputEnum key = gScanCodeToInput[scanCode][isExtendedKey];
    gInputValues[key] = keyDown ? 1.0f : 0.0f;
}

static void ProcessChar(const MSG* msg)
{
    if (gCharacterBufferCount >= TF_ARRAY_COUNT(gCharacterBuffer))
    {
        return;
    }

    int32_t        characterCode = (int32_t)msg->wParam;
    // WM_CHAR - utf16 - Convert to utf32
    static wchar_t highSurrogateChar = 0;
    const wchar_t  surrogateChar = static_cast<wchar_t>(characterCode);
    if (IS_HIGH_SURROGATE(surrogateChar))
    {
        highSurrogateChar = surrogateChar;
        return;
    }
    else if (IS_LOW_SURROGATE(surrogateChar))
    {
        ASSERT(IS_SURROGATE_PAIR(highSurrogateChar, surrogateChar));

        // WinNls.h
        characterCode = (highSurrogateChar << 10) + surrogateChar - 0x35fdc00;
        ASSERT(0x10000 <= characterCode && characterCode <= 0x10FFFF);
        highSurrogateChar = 0;
    }

    gCharacterBuffer[gCharacterBufferCount++] = characterCode;
}
/************************************************************************/
// Platform implementation
/************************************************************************/
void platformInitInput(WindowDesc* window)
{
    pWindow = window;
    gRelativeMouseLock = {};
    gRequestExitRawInputThread = {};

    ThreadDesc threadDesc = {};
    threadDesc.pFunc = RawInputThread;
    snprintf(threadDesc.mThreadName, TF_ARRAY_COUNT(threadDesc.mThreadName), "RawInput");
    initThread(&threadDesc, &gRawInputThread);

    int32_t hidRet = hidInit(&pWindow->handle);
    if (hidRet)
    {
        LOGF(eWARNING, "hidInit failed with error %d. Only XInput controllers will work", hidRet);
    }

    InputInitCommon();
}

void platformExitInput()
{
    GamepadDefault();
    for (uint32_t index = 0; index < MAX_GAMEPADS; ++index)
    {
        // Update one more time to stop rumbles
        GamepadUpdateState(index);
    }
    // Update one more time to stop rumbles
    hidUpdate();
    hidExit();

    gRequestExitRawInputThread = true;
    if (gRawInputHwnd)
    {
        // Wake up WaitMessage
        PostMessage(gRawInputHwnd, 0, 0, 0);
        joinThread(gRawInputThread);
    }
}

void platformUpdateLastInputState()
{
    memcpy(gLastInputValues, gInputValues, sizeof(gInputValues));
    gInputValues[MOUSE_WHEEL_UP] = 0;
    gInputValues[MOUSE_WHEEL_DOWN] = 0;
    gLastInputValues[MOUSE_DX] = 0;
    gLastInputValues[MOUSE_DY] = 0;
    gCharacterBufferCount = 0;
}

void platformUpdateInput(float deltaTime)
{
    for (uint32_t index = 0; index < MAX_GAMEPADS; ++index)
    {
        GamepadRefresh(index);
        GamepadUpdateState(index);
    }

    hidUpdate();

    gInputValues[MOUSE_X] = (float)gCursorPos[0];
    gInputValues[MOUSE_Y] = (float)gCursorPos[1];
    // Read raw mouse delta
    RelativeMouseLock();
    gInputValues[MOUSE_DX] = (float)gRawMouseDelta[0];
    gInputValues[MOUSE_DY] = (float)gRawMouseDelta[1];
    gRawMouseDelta[0] = 0;
    gRawMouseDelta[1] = 0;
    RelativeMouseUnlock();

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

void platformInputEvent(const MSG* msg)
{
    switch (msg->message)
    {
    case WM_MOUSEMOVE:
    {
        gCursorPos[0] = LOWORD(msg->lParam);
        gCursorPos[1] = HIWORD(msg->lParam);
        break;
    }
    case WM_MOUSEWHEEL:
    {
        int wheelDelta = GET_WHEEL_DELTA_WPARAM(msg->wParam);
        if (wheelDelta > 0)
        {
            gInputValues[MOUSE_WHEEL_UP] = 1;
        }
        else
        {
            gInputValues[MOUSE_WHEEL_DOWN] = 1;
        }
        break;
    }
    case WM_LBUTTONDOWN:
    {
        gInputValues[MOUSE_1] = 1;
        break;
    }
    case WM_RBUTTONDOWN:
    {
        gInputValues[MOUSE_2] = 1;
        break;
    }
    case WM_MBUTTONDOWN:
    {
        gInputValues[MOUSE_3] = 1;
        break;
    }
    case WM_XBUTTONDOWN:
    {
        gInputValues[MOUSE_4] = 1;
        break;
    }
    case WM_LBUTTONUP:
    {
        gInputValues[MOUSE_1] = 0;
        break;
    }
    case WM_RBUTTONUP:
    {
        gInputValues[MOUSE_2] = 0;
        break;
    }
    case WM_MBUTTONUP:
    {
        gInputValues[MOUSE_3] = 0;
        break;
    }
    case WM_XBUTTONUP:
    {
        gInputValues[MOUSE_4] = 0;
        break;
    }
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYUP:
    {
        ProcessKeyEvent(msg);
        break;
    }
    case WM_CHAR:
    {
        ProcessChar(msg);
        break;
    }
    default:
    {
        hidHandleMessage(msg);
        break;
    }
    }
}
