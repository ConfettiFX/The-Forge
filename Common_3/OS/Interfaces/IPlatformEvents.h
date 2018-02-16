/*
 * Copyright (c) 2018 Confetti Interactive Inc.
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
#include "IOperatingSystem.h"

// Platform independent key codes
#define INVALID_KEY 0

#define KEY_0 int('0')
#define KEY_1 int('1')
#define KEY_2 int('2')
#define KEY_3 int('3')
#define KEY_4 int('4')
#define KEY_5 int('5')
#define KEY_6 int('6')
#define KEY_7 int('7')
#define KEY_8 int('8')
#define KEY_9 int('9')

#if defined(_DURANGO)

#define BUTTON_MENU		0x4
#define BUTTON_A		0x10
#define BUTTON_B		0x20
#define BUTTON_X		0x40
#define BUTTON_Y		0x80
#define BUTTON_UP		0x100
#define BUTTON_DOWN		0x200
#define BUTTON_LEFT		0x400
#define BUTTON_RIGHT	0x800

#elif defined(_WIN32)

#define KEY_LEFT      VK_LEFT
#define KEY_RIGHT     VK_RIGHT
#define KEY_UP        VK_UP
#define KEY_DOWN      VK_DOWN
#define KEY_CTRL      VK_CONTROL
#define KEY_SHIFT     VK_SHIFT
#define KEY_ENTER     VK_RETURN
#define KEY_SPACE     VK_SPACE
#define KEY_TAB       VK_TAB
#define KEY_ESCAPE    VK_ESCAPE
#define KEY_BACKSPACE VK_BACK
#define KEY_HOME      VK_HOME
#define KEY_END       VK_END
#define KEY_INSERT    VK_INSERT
#define KEY_DELETE    VK_DELETE
#define KEY_CAPITAL   VK_CAPITAL
#define KEY_ALT		  VK_MENU

#define KEY_F1  VK_F1
#define KEY_F2  VK_F2
#define KEY_F3  VK_F3
#define KEY_F4  VK_F4
#define KEY_F5  VK_F5
#define KEY_F6  VK_F6
#define KEY_F7  VK_F7
#define KEY_F8  VK_F8
#define KEY_F9  VK_F9
#define KEY_F10 VK_F10
#define KEY_F11 VK_F11
#define KEY_F12 VK_F12

#define KEY_NUMPAD0 VK_NUMPAD0
#define KEY_NUMPAD1 VK_NUMPAD1
#define KEY_NUMPAD2 VK_NUMPAD2
#define KEY_NUMPAD3 VK_NUMPAD3
#define KEY_NUMPAD4 VK_NUMPAD4
#define KEY_NUMPAD5 VK_NUMPAD5
#define KEY_NUMPAD6 VK_NUMPAD6
#define KEY_NUMPAD7 VK_NUMPAD7
#define KEY_NUMPAD8 VK_NUMPAD8
#define KEY_NUMPAD9 VK_NUMPAD9

#define KEY_ADD        VK_ADD
#define KEY_SUBTRACT   VK_SUBTRACT
#define KEY_MULTIPLY   VK_MULTIPLY
#define KEY_DIVIDE     VK_DIVIDE
#define KEY_SEPARATOR  VK_SEPARATOR
#define KEY_DECIMAL    VK_DECIMAL
#define KEY_PAUSE      VK_PAUSE

#define KEY_A int('A')
#define KEY_B int('B')
#define KEY_C int('C')
#define KEY_D int('D')
#define KEY_E int('E')
#define KEY_F int('F')
#define KEY_G int('G')
#define KEY_H int('H')
#define KEY_I int('I')
#define KEY_J int('J')
#define KEY_K int('K')
#define KEY_L int('L')
#define KEY_M int('M')
#define KEY_N int('N')
#define KEY_O int('O')
#define KEY_P int('P')
#define KEY_Q int('Q')
#define KEY_R int('R')
#define KEY_S int('S')
#define KEY_T int('T')
#define KEY_U int('U')
#define KEY_V int('V')
#define KEY_W int('W')
#define KEY_X int('X')
#define KEY_Y int('Y')
#define KEY_Z int('Z')

#define KEY_COMMA int(',')
#define KEY_PERIOD int('.')

#define BUTTON_MENU		0x0010
#define BUTTON_A		0x1000
#define BUTTON_B		0x2000
#define BUTTON_X		0x4000
#define BUTTON_Y		0x8000
#define BUTTON_UP		0x0001
#define BUTTON_DOWN		0x0002
#define BUTTON_LEFT		0x0004
#define BUTTON_RIGHT	0x0008

#elif defined(LINUX)

#define KEY_LEFT      XK_Left
#define KEY_RIGHT     XK_Right
#define KEY_UP        XK_Up
#define KEY_DOWN      XK_Down
#define KEY_CTRL      XK_Control_R
#define KEY_SHIFT     XK_Shift_R
#define KEY_ENTER     XK_Return
#define KEY_SPACE     XK_space
#define KEY_TAB       XK_Tab
#define KEY_ESCAPE    XK_Escape
#define KEY_BACKSPACE XK_BackSpace
#define KEY_HOME      XK_Home
#define KEY_END       XK_End
#define KEY_INSERT    XK_Insert
#define KEY_DELETE    XK_Delete

#define KEY_F1  XK_F1
#define KEY_F2  XK_F2
#define KEY_F3  XK_F3
#define KEY_F4  XK_F4
#define KEY_F5  XK_F5
#define KEY_F6  XK_F6
#define KEY_F7  XK_F7
#define KEY_F8  XK_F8
#define KEY_F9  XK_F9
#define KEY_F10 XK_F10
#define KEY_F11 XK_F11
#define KEY_F12 XK_F12

#define KEY_NUMPAD0 XK_KP_0
#define KEY_NUMPAD1 XK_KP_1
#define KEY_NUMPAD2 XK_KP_2
#define KEY_NUMPAD3 XK_KP_3
#define KEY_NUMPAD4 XK_KP_4
#define KEY_NUMPAD5 XK_KP_5
#define KEY_NUMPAD6 XK_KP_6
#define KEY_NUMPAD7 XK_KP_7
#define KEY_NUMPAD8 XK_KP_8
#define KEY_NUMPAD9 XK_KP_9

#define KEY_ADD        XK_KP_Add
#define KEY_SUBTRACT   XK_KP_Subtract
#define KEY_MULTIPLY   XK_KP_Multiply
#define KEY_DIVIDE     XK_KP_Divide
#define KEY_SEPARATOR  XK_KP_Separator
#define KEY_DECIMAL    XK_KP_Decimal
#define KEY_PAUSE      XK_Pause

#define KEY_A int('a')
#define KEY_B int('b')
#define KEY_C int('c')
#define KEY_D int('d')
#define KEY_E int('e')
#define KEY_F int('f')
#define KEY_G int('g')
#define KEY_H int('h')
#define KEY_I int('i')
#define KEY_J int('j')
#define KEY_K int('k')
#define KEY_L int('l')
#define KEY_M int('m')
#define KEY_N int('n')
#define KEY_O int('o')
#define KEY_P int('p')
#define KEY_Q int('q')
#define KEY_R int('r')
#define KEY_S int('s')
#define KEY_T int('t')
#define KEY_U int('u')
#define KEY_V int('v')
#define KEY_W int('w')
#define KEY_X int('x')
#define KEY_Y int('y')
#define KEY_Z int('z')

#elif defined(_ANDROID)

#define KEY_LEFT      0
#define KEY_RIGHT     1
#define KEY_UP        2
#define KEY_DOWN      3
#define KEY_CTRL      4
#define KEY_SHIFT     5
#define KEY_ENTER     6
#define KEY_SPACE     7
#define KEY_TAB       8
#define KEY_ESCAPE    9
#define KEY_BACKSPACE 10
#define KEY_HOME      11
#define KEY_END       12
#define KEY_INSERT    13
#define KEY_DELETE    14

#define KEY_F1  15
#define KEY_F2  16
#define KEY_F3  17
#define KEY_F4  18
#define KEY_F5  19
#define KEY_F6  20
#define KEY_F7  21
#define KEY_F8  22
#define KEY_F9  23
#define KEY_F10 24
#define KEY_F11 25
#define KEY_F12 26

#define KEY_A 0
#define KEY_B 0
#define KEY_C 0
#define KEY_D 0
#define KEY_E 0
#define KEY_F 0
#define KEY_G 0
#define KEY_H 0
#define KEY_I 0
#define KEY_J 0
#define KEY_K 0
#define KEY_L 0
#define KEY_M 0
#define KEY_N 0
#define KEY_O 0
#define KEY_P 0
#define KEY_Q 0
#define KEY_R 0
#define KEY_S 0
#define KEY_T 0
#define KEY_U 0
#define KEY_V 0
#define KEY_W 0
#define KEY_X 0
#define KEY_Y 0
#define KEY_Z 0

#elif defined(_iOS)

#define KEY_LEFT      0x7B
#define KEY_RIGHT     0x7C
#define KEY_UP        0x7E
#define KEY_DOWN      0x7D
#define KEY_CTRL      0x3B
#define KEY_SHIFT     0x38
#define KEY_ENTER     0x24
#define KEY_SPACE     0x31
#define KEY_TAB       0x30
#define KEY_ESCAPE    0x35
#define KEY_BACKSPACE 0x33
#define KEY_HOME      0x73
#define KEY_END       0x77
#define KEY_INSERT    0x72
#define KEY_DELETE    0x33

#define KEY_F1  0x7A
#define KEY_F2  0x78
#define KEY_F3  0x63
#define KEY_F4  0x76
#define KEY_F5  0x60
#define KEY_F6  0x61
#define KEY_F7  0x62
#define KEY_F8  0x64
#define KEY_F9  0x65
#define KEY_F10 0x6D
#define KEY_F11 0x67
#define KEY_F12 0x6F

#define KEY_NUMPAD0 0x52
#define KEY_NUMPAD1 0x53
#define KEY_NUMPAD2 0x54
#define KEY_NUMPAD3 0x55
#define KEY_NUMPAD4 0x56
#define KEY_NUMPAD5 0x57
#define KEY_NUMPAD6 0x58
#define KEY_NUMPAD7 0x59
#define KEY_NUMPAD8 0x5B
#define KEY_NUMPAD9 0x5C

#define KEY_ADD        0x45
#define KEY_SUBTRACT   0x4E
#define KEY_MULTIPLY   0x43
#define KEY_DIVIDE     0x4B
#define KEY_SEPARATOR  0x2B
#define KEY_DECIMAL    0x41
//#define KEY_PAUSE      ????

#define KEY_A 0x00
#define KEY_B 0x0B
#define KEY_C 0x08
#define KEY_D 0x02
#define KEY_E 0x0E
#define KEY_F 0x03
#define KEY_G 0x05
#define KEY_H 0x04
#define KEY_I 0x22
#define KEY_J 0x26
#define KEY_K 0x28
#define KEY_L 0x25
#define KEY_M 0x2E
#define KEY_N 0x2D
#define KEY_O 0x1F
#define KEY_P 0x23
#define KEY_Q 0x0C
#define KEY_R 0x0F
#define KEY_S 0x01
#define KEY_T 0x11
#define KEY_U 0x20
#define KEY_V 0x09
#define KEY_W 0x0D
#define KEY_X 0x07
#define KEY_Y 0x10
#define KEY_Z 0x06

#elif	defined(SN_TARGET_PS3) || defined(ORBIS) //	platform defines
//TODO: PS3: Igor: replace with a valid key codes when it will be known hoe the keyboard input should be handled.
#define KEY_LEFT      0x7B
#define KEY_RIGHT     0x7C
#define KEY_UP        0x7E
#define KEY_DOWN      0x7D
#define KEY_CTRL      0x3B
#define KEY_SHIFT     0x38
#define KEY_ENTER     0x24
#define KEY_SPACE     0x31
#define KEY_TAB       0x30
#define KEY_ESCAPE    0x35
#define KEY_BACKSPACE 0x33
#define KEY_HOME      0x73
#define KEY_END       0x77
#define KEY_INSERT    0x72
#define KEY_DELETE    0x33
#define KEY_PAUSE      0x002A

#define KEY_F1  0x7A
#define KEY_F2  0x78
#define KEY_F3  0x63
#define KEY_F4  0x76
#define KEY_F5  0x60
#define KEY_F6  0x61
#define KEY_F7  0x62
#define KEY_F8  0x64
#define KEY_F9  0x65
#define KEY_F10 0x6D
#define KEY_F11 0x67
#define KEY_F12 0x6F

#define KEY_NUMPAD0 0x52
#define KEY_NUMPAD1 0x53
#define KEY_NUMPAD2 0x54
#define KEY_NUMPAD3 0x55
#define KEY_NUMPAD4 0x56
#define KEY_NUMPAD5 0x57
#define KEY_NUMPAD6 0x58
#define KEY_NUMPAD7 0x59
#define KEY_NUMPAD8 0x5B
#define KEY_NUMPAD9 0x5C

#define KEY_ADD        0x45
#define KEY_SUBTRACT   0x4E
#define KEY_MULTIPLY   0x43
#define KEY_DIVIDE     0x4B
#define KEY_SEPARATOR  0x2B
#define KEY_DECIMAL    0x41
//#define KEY_PAUSE      ????

#define KEY_A 0x00
#define KEY_B 0x0B
#define KEY_C 0x08
#define KEY_D 0x02
#define KEY_E 0x0E
#define KEY_F 0x03
#define KEY_G 0x05
#define KEY_H 0x04
#define KEY_I 0x22
#define KEY_J 0x26
#define KEY_K 0x28
#define KEY_L 0x25
#define KEY_M 0x2E
#define KEY_N 0x2D
#define KEY_O 0x1F
#define KEY_P 0x23
#define KEY_Q 0x0C
#define KEY_R 0x0F
#define KEY_S 0x01
#define KEY_T 0x11
#define KEY_U 0x20
#define KEY_V 0x09
#define KEY_W 0x0D
#define KEY_X 0x07
#define KEY_Y 0x10
#define KEY_Z 0x06

#elif defined(__APPLE__) //	platform defines

typedef struct tagRECT
{
	long    left;
	long    top;
	long    right;
	long    bottom;
} RECT, *PRECT;

#define KEY_LEFT      kVK_LeftArrow
#define KEY_RIGHT     kVK_RightArrow
#define KEY_UP        kVK_UpArrow
#define KEY_DOWN      kVK_DownArrow
#define KEY_CTRL      kVK_Control
#define KEY_SHIFT     kVK_Shift
#define KEY_ENTER     kVK_Return
#define KEY_SPACE     kVK_Space
#define KEY_TAB       kVK_Tab
#define KEY_ESCAPE    kVK_Escape
#define KEY_BACKSPACE kVK_Delete
#define KEY_HOME      kVK_Home
#define KEY_END       kVK_End
//#define KEY_INSERT    0x72
#define KEY_DELETE    kVK_Delete
#define KEY_PAUSE      0

#define KEY_F1  kVK_F1
#define KEY_F2  kVK_F2
#define KEY_F3  kVK_F3
#define KEY_F4  kVK_F4
#define KEY_F5  kVK_F5
#define KEY_F6  kVK_F6
#define KEY_F7  kVK_F7
#define KEY_F8  kVK_F8
#define KEY_F9  kVK_F9
#define KEY_F10 kVK_F10
#define KEY_F11 kVK_F11
#define KEY_F12 kVK_F12

//#define KEY_NUMPAD0 0x52
//#define KEY_NUMPAD1 0x53
//#define KEY_NUMPAD2 0x54
//#define KEY_NUMPAD3 0x55
//#define KEY_NUMPAD4 0x56
//#define KEY_NUMPAD5 0x57
//#define KEY_NUMPAD6 0x58
//#define KEY_NUMPAD7 0x59
//#define KEY_NUMPAD8 0x5B
//#define KEY_NUMPAD9 0x5C
//
//#define KEY_ADD        0x45
//#define KEY_SUBTRACT   0x4E
//#define KEY_MULTIPLY   0x43
//#define KEY_DIVIDE     0x4B
//#define KEY_SEPARATOR  0x2B
//#define KEY_DECIMAL    0x41
////#define KEY_PAUSE      ????
//
#define KEY_A kVK_ANSI_A
#define KEY_B kVK_ANSI_B
#define KEY_C kVK_ANSI_C
#define KEY_D kVK_ANSI_D
#define KEY_E kVK_ANSI_E
#define KEY_F kVK_ANSI_F
#define KEY_G kVK_ANSI_G
#define KEY_H kVK_ANSI_H
#define KEY_I kVK_ANSI_I
#define KEY_J kVK_ANSI_J
#define KEY_K kVK_ANSI_K
#define KEY_L kVK_ANSI_L
#define KEY_M kVK_ANSI_M
#define KEY_N kVK_ANSI_N
#define KEY_O kVK_ANSI_O
#define KEY_P kVK_ANSI_P
#define KEY_Q kVK_ANSI_Q
#define KEY_R kVK_ANSI_R
#define KEY_S kVK_ANSI_S
#define KEY_T kVK_ANSI_T
#define KEY_U kVK_ANSI_U
#define KEY_V kVK_ANSI_V
#define KEY_W kVK_ANSI_W
#define KEY_X kVK_ANSI_X
#define KEY_Y kVK_ANSI_Y
#define KEY_Z kVK_ANSI_Z

// TODO: Implement proper gamepad input for macOS.
#define BUTTON_MENU     0x0
#define BUTTON_A        0x0
#define BUTTON_B        0x0
#define BUTTON_X        0x0
#define BUTTON_Y        0x0
#define BUTTON_UP       0x0
#define BUTTON_DOWN     0x0
#define BUTTON_LEFT     0x0
#define BUTTON_RIGHT    0x0

#else	//	platform defines
#error "Platform is not supported! Make sure you've made appropriate define. Or just add empty section if don't need for this platform."
#endif	//	platform defines


typedef enum MouseButton
{
	MOUSE_LEFT,
	MOUSE_RIGHT,
	MOUSE_MIDDLE,
	MOUSE_BUTTON_COUNT
} MouseButton;

typedef struct WindowResizeEventData
{
	RectDesc rect;
	struct WindowsDesc* pWindow;
} WindowResizeEventData;

typedef struct KeyboardCharEventData
{
	unsigned int unicode;
} KeyboardCharEventData;

typedef struct KeyboardButtonEventData
{
	unsigned int key;
	bool pressed;
} KeyboardButtonEventData;

typedef struct JoystickButtonEventData
{
	unsigned int button;
	bool pressed;
} JoystickButtonEventData;

typedef struct MouseMoveEventData
{
	int x;
	int y;
	int deltaX;
	int deltaY;
	bool captured;
} MouseMoveEventData;

typedef struct RawMouseMoveEventData
{
	int x;
	int y;
	bool captured;
} RawMouseMoveEventData;

typedef struct MouseButtonEventData
{
	int x;
	int y;
	MouseButton button;
	bool pressed;
} MouseButtonEventData;

typedef struct MouseWheelEventData
{
	int x;
	int y;
	int scroll;
} MouseWheelEventData;

#define MAX_MULTI_TOUCHES 5
typedef struct TouchData
{
    int x;
    float screenX;
    int y;
    float screenY;
    int deltaX;
    float screenDeltaX;
    int deltaY;
    float screenDeltaY;
    bool pressed;
} TouchData;

typedef struct TouchEventData
{
    uint32_t touchesRecorded;
    TouchData touchData[MAX_MULTI_TOUCHES];
} TouchEventData;

typedef void(*WindowResizeEventHandler)(const WindowResizeEventData* data);
void registerWindowResizeEvent(WindowResizeEventHandler callback);
void unregisterWindowResizeEvent(WindowResizeEventHandler callback);

typedef bool(*KeyboardCharEventHandler)(const KeyboardCharEventData* data);
void registerKeyboardCharEvent(KeyboardCharEventHandler callback);
void unregisterKeyboardCharEvent(KeyboardCharEventHandler callback);

typedef bool(*KeyboardButtonEventHandler)(const KeyboardButtonEventData* data);
void registerKeyboardButtonEvent(KeyboardButtonEventHandler callback);
void unregisterKeyboardButtonEvent(KeyboardButtonEventHandler callback);

typedef bool(*MouseMoveEventHandler)(const MouseMoveEventData* data);
void registerMouseMoveEvent(MouseMoveEventHandler callback);
void unregisterMouseMoveEvent(MouseMoveEventHandler callback);

typedef bool(*RawMouseMoveEventHandler)(const RawMouseMoveEventData* data);
void registerRawMouseMoveEvent(RawMouseMoveEventHandler callback);
void unregisterRawMouseMoveEvent(RawMouseMoveEventHandler callback);

typedef bool(*MouseButtonEventHandler)(const MouseButtonEventData* data);
void registerMouseButtonEvent(MouseButtonEventHandler callback);
void unregisterMouseButtonEvent(MouseButtonEventHandler callback);

typedef bool(*MouseWheelEventHandler)(const MouseWheelEventData* data);
void registerMouseWheelEvent(MouseWheelEventHandler callback);
void unregisterMouseWheelEvent(MouseWheelEventHandler callback);

typedef bool(*JoystickButtonEventHandler)(const JoystickButtonEventData* data);
void registerJoystickButtonEvent(JoystickButtonEventHandler callback);
void unregisterJoystickButtonEvent(JoystickButtonEventHandler callback);

bool requestMouseCapture(bool allowCapture);

typedef bool(*TouchEventHandler)(const TouchEventData* data);
void registerTouchEvent(TouchEventHandler callback);
void unregisterTouchEvent(TouchEventHandler callback);

typedef bool(*TouchMoveEventHandler)(const TouchEventData* data);
void registerTouchMoveEvent(TouchEventHandler callback);
void unregisterTouchMoveEvent(TouchEventHandler callback);
