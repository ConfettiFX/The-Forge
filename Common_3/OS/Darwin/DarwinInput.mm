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

#if !defined(TARGET_IOS)
#import <Cocoa/Cocoa.h>
#endif
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import <GameController/GameController.h>
#import <CoreHaptics/CoreHaptics.h>

#include "../Input/InputCommon.h"

#include "../../Utilities/Interfaces/IMemory.h"

struct DarwinGamepad
{
    GCController*             pController;
    GCExtendedGamepad*        pExtPad;
    CHHapticEngine*           pEngines[2];
    id<CHHapticPatternPlayer> pPlayers[2];
    bool                      mPlayerActive[2];
    float3                    mInitialLight;
};

static DarwinGamepad gDarwinGamepads[MAX_GAMEPADS] = {};
static float         gRawMouseDelta[2] = {};
static Mutex         gRumbleMutex = {};

/************************************************************************/
// Touch
/************************************************************************/
#if defined(ENABLE_FORGE_TOUCH_INPUT)
#include "../Input/TouchInput.h"
#endif
/************************************************************************/
// Gamepad Rumble
/************************************************************************/
IOS14_API
static bool RumbleSetupPlayer(DarwinGamepad& dgpad, uint32_t locality)
{
    if (dgpad.pPlayers[locality])
    {
        return true;
    }
    NSError*                error;
    CHHapticEventParameter* eventParam = [[CHHapticEventParameter alloc] initWithParameterID:CHHapticEventParameterIDHapticIntensity
                                                                                       value:1.0f];
    CHHapticEvent*          event = [[CHHapticEvent alloc] initWithEventType:CHHapticEventTypeHapticContinuous
                                                         parameters:[NSArray arrayWithObjects:eventParam, nil]
                                                       relativeTime:0
                                                           duration:GCHapticDurationInfinite];
    CHHapticPattern*        pattern = [[CHHapticPattern alloc] initWithEvents:[NSArray arrayWithObject:event]
                                                            parameters:[[NSArray alloc] init]
                                                                 error:&error];
    if (error)
    {
        LOGF(eWARNING, "Could not create haptic player: %s", [error.localizedDescription UTF8String]);
        return false;
    }

    dgpad.pPlayers[locality] = [dgpad.pEngines[locality] createPlayerWithPattern:pattern error:&error];
    if (error)
    {
        LOGF(eWARNING, "Could not create haptic player: %s", [error.localizedDescription UTF8String]);
        return false;
    }
    dgpad.mPlayerActive[locality] = false;
    return true;
}

IOS14_API
static void RumbleAddDevice(DarwinGamepad& dgpad)
{
    GCDeviceHaptics* haptics = dgpad.pController.haptics;
    if (!haptics)
    {
        return;
    }

    GCHapticsLocality         motorLocalities[] = { GCHapticsLocalityLeftHandle, GCHapticsLocalityRightHandle };
    NSSet<GCHapticsLocality>* localities = [haptics supportedLocalities];
    if ([localities containsObject:GCHapticsLocalityHandles])
    {
        for (uint32_t l = 0; l < TF_ARRAY_COUNT(motorLocalities); ++l)
        {
            CHHapticEngine* engine = [haptics createEngineWithLocality:motorLocalities[l]];
            if (!engine)
            {
                continue;
            }
            NSError* error;
            [engine startAndReturnError:&error];
            if (error)
            {
                LOGF(eWARNING, "Could not start haptics engine %s with error %s", [motorLocalities[l] UTF8String],
                     [error.localizedDescription UTF8String]);
                continue;
            }

            dgpad.pEngines[l] = engine;
            RumbleSetupPlayer(dgpad, l);

            engine.stoppedHandler = ^(CHHapticEngineStoppedReason stoppedReason) {
                acquireMutex(&gRumbleMutex);
                dgpad.pEngines[l] = nil;
                dgpad.pPlayers[l] = nil;
                releaseMutex(&gRumbleMutex);
            };
            engine.resetHandler = ^{
                acquireMutex(&gRumbleMutex);
                dgpad.pPlayers[l] = nil;
                [dgpad.pEngines[l] startAndReturnError:nil];
                releaseMutex(&gRumbleMutex);
            };

            error = nil;
        }
    }
}

IOS14_API
static void RumbleRemoveDevice(DarwinGamepad& dgpad)
{
    GCDeviceHaptics* haptics = dgpad.pController.haptics;
    if (!haptics)
    {
        return;
    }

    acquireMutex(&gRumbleMutex);

    for (uint32_t l = 0; l < TF_ARRAY_COUNT(dgpad.pEngines); ++l)
    {
        if (dgpad.pPlayers[l])
        {
            [dgpad.pPlayers[l] cancelAndReturnError:nil];
            dgpad.pPlayers[l] = nil;
        }
        if (dgpad.pEngines[l])
        {
            [dgpad.pEngines[l] stopWithCompletionHandler:nil];
            dgpad.pEngines[l] = nil;
        }
    }

    releaseMutex(&gRumbleMutex);
}

IOS14_API
static void RumbleWriteLocal(DarwinGamepad& dgpad, uint32_t locality, float intensity)
{
    if (!dgpad.pEngines[locality])
    {
        return;
    }
    if (!RumbleSetupPlayer(dgpad, locality))
    {
        return;
    }
    if (!dgpad.mPlayerActive[locality])
    {
        [dgpad.pPlayers[locality] startAtTime:0 error:nil];
        dgpad.mPlayerActive[locality] = true;
    }
    if (intensity == 0.0f)
    {
        if (dgpad.mPlayerActive[locality])
        {
            [dgpad.pPlayers[locality] stopAtTime:0 error:nil];
        }
        dgpad.mPlayerActive[locality] = false;
        return;
    }

    NSError*                  error;
    CHHapticDynamicParameter* param = [[CHHapticDynamicParameter alloc] initWithParameterID:CHHapticDynamicParameterIDHapticIntensityControl
                                                                                      value:intensity
                                                                               relativeTime:0];
    [dgpad.pPlayers[locality] sendParameters:[NSArray arrayWithObject:param] atTime:0 error:&error];
    if (error)
    {
        LOGF(eWARNING, "Could not update haptic player: %s", [error.localizedDescription UTF8String]);
        return;
    }
}

IOS14_API
static void RumbleWrite(DarwinGamepad& dgpad, Gamepad& gpad)
{
    bool stopRumble = false;
    if (gpad.mRumbleHigh == 0.0f && gpad.mRumbleLow == 0.0f)
    {
        stopRumble = true;
    }
    // Dont keep setting zero rumble
    if (gpad.mRumbleStopped)
    {
        return;
    }

    RumbleWriteLocal(dgpad, 0, gpad.mRumbleLow);
    RumbleWriteLocal(dgpad, 1, gpad.mRumbleHigh);

    gpad.mRumbleStopped = stopRumble;
}
/************************************************************************/
// Gamepad
/************************************************************************/
static void GamepadUpdateState(InputPortIndex index)
{
    Gamepad&       gpad = gGamepads[index];
    DarwinGamepad& dgpad = gDarwinGamepads[index];
    GamepadUpdateLastState(index);

    if (!inputGamepadIsActive(index))
    {
        return;
    }

    GCExtendedGamepad* ext = dgpad.pExtPad;

    gpad.mButtons[GPAD_A - GPAD_BTN_FIRST] = ext.buttonA.pressed;
    gpad.mButtons[GPAD_B - GPAD_BTN_FIRST] = ext.buttonB.pressed;
    gpad.mButtons[GPAD_X - GPAD_BTN_FIRST] = ext.buttonX.pressed;
    gpad.mButtons[GPAD_Y - GPAD_BTN_FIRST] = ext.buttonY.pressed;
    gpad.mButtons[GPAD_UP - GPAD_BTN_FIRST] = ext.dpad.up.pressed;
    gpad.mButtons[GPAD_DOWN - GPAD_BTN_FIRST] = ext.dpad.down.pressed;
    gpad.mButtons[GPAD_LEFT - GPAD_BTN_FIRST] = ext.dpad.left.pressed;
    gpad.mButtons[GPAD_RIGHT - GPAD_BTN_FIRST] = ext.dpad.right.pressed;
    gpad.mButtons[GPAD_L1 - GPAD_BTN_FIRST] = ext.leftShoulder.pressed;
    gpad.mButtons[GPAD_R1 - GPAD_BTN_FIRST] = ext.rightShoulder.pressed;
    gpad.mButtons[GPAD_START - GPAD_BTN_FIRST] = ext.buttonMenu.pressed;
    gpad.mButtons[GPAD_BACK - GPAD_BTN_FIRST] = ext.buttonOptions.pressed;
    gpad.mButtons[GPAD_L3 - GPAD_BTN_FIRST] = ext.leftThumbstickButton.pressed;
    gpad.mButtons[GPAD_R3 - GPAD_BTN_FIRST] = ext.rightThumbstickButton.pressed;

    gpad.mAxis[GPAD_LX - GPAD_AXIS_FIRST] = ext.leftThumbstick.xAxis.value;
    gpad.mAxis[GPAD_LY - GPAD_AXIS_FIRST] = ext.leftThumbstick.yAxis.value;
    gpad.mAxis[GPAD_RX - GPAD_AXIS_FIRST] = ext.rightThumbstick.xAxis.value;
    gpad.mAxis[GPAD_RY - GPAD_AXIS_FIRST] = ext.rightThumbstick.yAxis.value;
    gpad.mAxis[GPAD_L2 - GPAD_AXIS_FIRST] = ext.leftTrigger.value;
    gpad.mAxis[GPAD_R2 - GPAD_AXIS_FIRST] = ext.rightTrigger.value;

    GamepadProcessStick(index, GPAD_LX);
    GamepadProcessStick(index, GPAD_RX);
    GamepadProcessTrigger(index, GPAD_L2);
    GamepadProcessTrigger(index, GPAD_R2);

    if (IOS14_RUNTIME)
    {
        RumbleWrite(dgpad, gpad);

        // Light bar
        if (dgpad.pController.light)
        {
            const float3& lightColor = gpad.mLightReset ? dgpad.mInitialLight : gpad.mLight;
            if (gpad.mLightUpdate || gpad.mLightReset)
            {
                GCDeviceLight* light = dgpad.pController.light;
                light.color = [[GCColor alloc] initWithRed:lightColor[0] green:lightColor[1] blue:lightColor[2]];
            }
            gpad.mLightUpdate = false;
            gpad.mLightReset = false;
        }
    }
}
/************************************************************************/
// Mouse, Keyboard
/************************************************************************/
IOS14_API
static inline FORGE_CONSTEXPR InputEnum GetKey(GCKeyCode key)
{
    if (GCKeyCodeKeyA == key)
        return K_A; /* a or A */
    if (GCKeyCodeKeyB == key)
        return K_B; /* b or B */
    if (GCKeyCodeKeyC == key)
        return K_C; /* c or C */
    if (GCKeyCodeKeyD == key)
        return K_D; /* d or D */
    if (GCKeyCodeKeyE == key)
        return K_E; /* e or E */
    if (GCKeyCodeKeyF == key)
        return K_F; /* f or F */
    if (GCKeyCodeKeyG == key)
        return K_G; /* g or G */
    if (GCKeyCodeKeyH == key)
        return K_H; /* h or H */
    if (GCKeyCodeKeyI == key)
        return K_I; /* i or I */
    if (GCKeyCodeKeyJ == key)
        return K_J; /* j or J */
    if (GCKeyCodeKeyK == key)
        return K_K; /* k or K */
    if (GCKeyCodeKeyL == key)
        return K_L; /* l or L */
    if (GCKeyCodeKeyM == key)
        return K_M; /* m or M */
    if (GCKeyCodeKeyN == key)
        return K_N; /* n or N */
    if (GCKeyCodeKeyO == key)
        return K_O; /* o or O */
    if (GCKeyCodeKeyP == key)
        return K_P; /* p or P */
    if (GCKeyCodeKeyQ == key)
        return K_Q; /* q or Q */
    if (GCKeyCodeKeyR == key)
        return K_R; /* r or R */
    if (GCKeyCodeKeyS == key)
        return K_S; /* s or S */
    if (GCKeyCodeKeyT == key)
        return K_T; /* t or T */
    if (GCKeyCodeKeyU == key)
        return K_U; /* u or U */
    if (GCKeyCodeKeyV == key)
        return K_V; /* v or V */
    if (GCKeyCodeKeyW == key)
        return K_W; /* w or W */
    if (GCKeyCodeKeyX == key)
        return K_X; /* x or X */
    if (GCKeyCodeKeyY == key)
        return K_Y; /* y or Y */
    if (GCKeyCodeKeyZ == key)
        return K_Z; /* z or Z */
    if (GCKeyCodeOne == key)
        return K_1; /* 1 or ! */
    if (GCKeyCodeTwo == key)
        return K_2; /* 2 or @ */
    if (GCKeyCodeThree == key)
        return K_3; /* 3 or # */
    if (GCKeyCodeFour == key)
        return K_4; /* 4 or $ */
    if (GCKeyCodeFive == key)
        return K_5; /* 5 or % */
    if (GCKeyCodeSix == key)
        return K_6; /* 6 or ^ */
    if (GCKeyCodeSeven == key)
        return K_7; /* 7 or & */
    if (GCKeyCodeEight == key)
        return K_8; /* 8 or * */
    if (GCKeyCodeNine == key)
        return K_9; /* 9 or ( */
    if (GCKeyCodeZero == key)
        return K_0; /* 0 or ) */
    if (GCKeyCodeReturnOrEnter == key)
        return K_ENTER; /* Return (Enter) */
    if (GCKeyCodeEscape == key)
        return K_ESCAPE; /* Escape */
    if (GCKeyCodeDeleteOrBackspace == key)
        return K_BACKSPACE; /* Delete (Backspace) */
    if (GCKeyCodeTab == key)
        return K_TAB; /* Tab */
    if (GCKeyCodeSpacebar == key)
        return K_SPACE; /* Spacebar */
    if (GCKeyCodeHyphen == key)
        return K_MINUS; /* - or _ */
    if (GCKeyCodeEqualSign == key)
        return K_EQUAL; /* = or + */
    if (GCKeyCodeOpenBracket == key)
        return K_LEFTBRACKET; /* [ or { */
    if (GCKeyCodeCloseBracket == key)
        return K_RIGHTBRACKET; /* ] or } */
    if (GCKeyCodeBackslash == key)
        return K_BACKSLASH; /* \ or | */
    if (GCKeyCodeNonUSPound == key)
        return INPUT_NONE; /* Non-US # or _ */
    if (GCKeyCodeSemicolon == key)
        return K_SEMICOLON; /* ; or : */
    if (GCKeyCodeQuote == key)
        return K_APOSTROPHE; /* ' or " */
    if (GCKeyCodeGraveAccentAndTilde == key)
        return K_GRAVE; /* Grave Accent and Tilde */
    if (GCKeyCodeComma == key)
        return K_COMMA; /* , or < */
    if (GCKeyCodePeriod == key)
        return K_PERIOD; /* . or > */
    if (GCKeyCodeSlash == key)
        return K_SLASH; /* / or ? */
    if (GCKeyCodeCapsLock == key)
        return K_CAPSLOCK; /* Caps Lock */
    /* Function keys */
    if (GCKeyCodeF1 == key)
        return K_F1; /* F1 */
    if (GCKeyCodeF2 == key)
        return K_F2; /* F2 */
    if (GCKeyCodeF3 == key)
        return K_F3; /* F3 */
    if (GCKeyCodeF4 == key)
        return K_F4; /* F4 */
    if (GCKeyCodeF5 == key)
        return K_F5; /* F5 */
    if (GCKeyCodeF6 == key)
        return K_F6; /* F6 */
    if (GCKeyCodeF7 == key)
        return K_F7; /* F7 */
    if (GCKeyCodeF8 == key)
        return K_F8; /* F8 */
    if (GCKeyCodeF9 == key)
        return K_F9; /* F9 */
    if (GCKeyCodeF10 == key)
        return K_F10; /* F10 */
    if (GCKeyCodeF11 == key)
        return K_F11; /* F11 */
    if (GCKeyCodeF12 == key)
        return K_F12; /* F12 */
    if (IOS17_RUNTIME)
    {
        if (GCKeyCodeF13 == key)
            return INPUT_NONE; /* F13 */
        if (GCKeyCodeF14 == key)
            return INPUT_NONE; /* F14 */
        if (GCKeyCodeF15 == key)
            return INPUT_NONE; /* F15 */
        if (GCKeyCodeF16 == key)
            return INPUT_NONE; /* F16 */
        if (GCKeyCodeF17 == key)
            return INPUT_NONE; /* F17 */
        if (GCKeyCodeF18 == key)
            return INPUT_NONE; /* F18 */
        if (GCKeyCodeF19 == key)
            return INPUT_NONE; /* F19 */
        if (GCKeyCodeF20 == key)
            return INPUT_NONE; /* F20 */
    }
    if (GCKeyCodePrintScreen == key)
        return K_PRINTSCREEN; /* Print Screen */
    if (GCKeyCodeScrollLock == key)
        return K_SCROLLLOCK; /* Scroll Lock */
    if (GCKeyCodePause == key)
        return K_PAUSE; /* Pause */
    if (GCKeyCodeInsert == key)
        return K_INS; /* Insert */
    if (GCKeyCodeHome == key)
        return K_HOME; /* Home */
    if (GCKeyCodePageUp == key)
        return K_PGUP; /* Page Up */
    if (GCKeyCodeDeleteForward == key)
        return K_DEL; /* Delete Forward */
    if (GCKeyCodeEnd == key)
        return K_END; /* End */
    if (GCKeyCodePageDown == key)
        return K_PGDN; /* Page Down */
    if (GCKeyCodeRightArrow == key)
        return K_RIGHTARROW; /* Right Arrow */
    if (GCKeyCodeLeftArrow == key)
        return K_LEFTARROW; /* Left Arrow */
    if (GCKeyCodeDownArrow == key)
        return K_DOWNARROW; /* Down Arrow */
    if (GCKeyCodeUpArrow == key)
        return K_UPARROW; /* Up Arrow */
    /* Keypad (numpad) keys */
    if (GCKeyCodeKeypadNumLock == key)
        return K_KP_NUMLOCK; /* Keypad NumLock or Clear */
    if (GCKeyCodeKeypadSlash == key)
        return K_KP_SLASH; /* Keypad / */
    if (GCKeyCodeKeypadAsterisk == key)
        return K_KP_STAR; /* Keypad * */
    if (GCKeyCodeKeypadHyphen == key)
        return K_KP_MINUS; /* Keypad - */
    if (GCKeyCodeKeypadPlus == key)
        return K_KP_PLUS; /* Keypad + */
    if (GCKeyCodeKeypadEnter == key)
        return K_KP_ENTER; /* Keypad Enter */
    if (GCKeyCodeKeypad1 == key)
        return K_KP_END; /* Keypad 1 or End */
    if (GCKeyCodeKeypad2 == key)
        return K_KP_DOWNARROW; /* Keypad 2 or Down Arrow */
    if (GCKeyCodeKeypad3 == key)
        return K_KP_PGDN; /* Keypad 3 or Page Down */
    if (GCKeyCodeKeypad4 == key)
        return K_KP_LEFTARROW; /* Keypad 4 or Left Arrow */
    if (GCKeyCodeKeypad5 == key)
        return K_KP_NUMPAD_5; /* Keypad 5 */
    if (GCKeyCodeKeypad6 == key)
        return K_KP_RIGHTARROW; /* Keypad 6 or Right Arrow */
    if (GCKeyCodeKeypad7 == key)
        return K_KP_HOME; /* Keypad 7 or Home */
    if (GCKeyCodeKeypad8 == key)
        return K_KP_UPARROW; /* Keypad 8 or Up Arrow */
    if (GCKeyCodeKeypad9 == key)
        return K_KP_PGUP; /* Keypad 9 or Page Up */
    if (GCKeyCodeKeypad0 == key)
        return K_KP_INS; /* Keypad 0 or Insert */
    if (GCKeyCodeKeypadPeriod == key)
        return K_KP_DEL; /* Keypad . or Delete */
    if (GCKeyCodeKeypadEqualSign == key)
        return K_KP_EQUALS; /* Keypad = */
    if (GCKeyCodeNonUSBackslash == key)
        return K_BACKSLASH; /* Non-US \ or | */
    if (GCKeyCodeApplication == key)
        return INPUT_NONE; /* Application */
    if (GCKeyCodePower == key)
        return INPUT_NONE; /* Power */
    if (GCKeyCodeInternational1 == key)
        return INPUT_NONE; /* International1 */
    if (GCKeyCodeInternational2 == key)
        return INPUT_NONE; /* International2 */
    if (GCKeyCodeInternational3 == key)
        return INPUT_NONE; /* International3 */
    if (GCKeyCodeInternational4 == key)
        return INPUT_NONE; /* International4 */
    if (GCKeyCodeInternational5 == key)
        return INPUT_NONE; /* International5 */
    if (GCKeyCodeInternational6 == key)
        return INPUT_NONE; /* International6 */
    if (GCKeyCodeInternational7 == key)
        return INPUT_NONE; /* International7 */
    if (GCKeyCodeInternational8 == key)
        return INPUT_NONE; /* International8 */
    if (GCKeyCodeInternational9 == key)
        return INPUT_NONE; /* International9 */
    /* LANG1: On Apple keyboard for Japanese, this is the kana switch (??????) key */
    /* On Korean keyboards, this is the Hangul/English toggle key. */
    if (GCKeyCodeLANG1 == key)
        return INPUT_NONE; /* LANG1 */
    /* LANG2: On Apple keyboards for Japanese, this is the alphanumeric (??????) key */
    /* On Korean keyboards, this is the Hanja conversion key. */
    if (GCKeyCodeLANG2 == key)
        return INPUT_NONE; /* LANG2 */
    /* LANG3: Defines the Katakana key for Japanese USB word-processing keyboards. */
    if (GCKeyCodeLANG3 == key)
        return INPUT_NONE; /* LANG3 */
    /* LANG4: Defines the Hiragana key for Japanese USB word-processing keyboards. */
    if (GCKeyCodeLANG4 == key)
        return INPUT_NONE; /* LANG4 */
    /* LANG5: Defines the Zenkaku/Hankaku key for Japanese USB word-processing keyboards. */
    if (GCKeyCodeLANG5 == key)
        return INPUT_NONE; /* LANG5 */
    /* LANG6-9: Reserved for language-specific functions, such as Front End Processors and Input Method Editors. */
    if (GCKeyCodeLANG6 == key)
        return INPUT_NONE; /* LANG6 */
    if (GCKeyCodeLANG7 == key)
        return INPUT_NONE; /* LANG7 */
    if (GCKeyCodeLANG8 == key)
        return INPUT_NONE; /* LANG8 */
    if (GCKeyCodeLANG9 == key)
        return INPUT_NONE; /* LANG9 */
    if (GCKeyCodeLeftControl == key)
        return K_LCTRL; /* Left Control */
    if (GCKeyCodeLeftShift == key)
        return K_LSHIFT; /* Left Shift */
    if (GCKeyCodeLeftAlt == key)
        return K_LALT; /* Left Alt */
    if (GCKeyCodeLeftGUI == key)
        return INPUT_NONE; /* Left GUI */
    if (GCKeyCodeRightControl == key)
        return K_RCTRL; /* Right Control */
    if (GCKeyCodeRightShift == key)
        return K_RSHIFT; /* Right Shift */
    if (GCKeyCodeRightAlt == key)
        return K_RALT; /* Right Alt */
    if (GCKeyCodeRightGUI == key)
        return INPUT_NONE; /* Right GUI */

    return INPUT_NONE;
}

static const uint32_t             MAX_MICE = 8;
static uint32_t                   activeMouseCount = 0;
IOS14_API static GCMouseInput*    pMouseInputs[MAX_MICE] = {};
IOS14_API static GCKeyboardInput* pKeyboardInput = {};
#if !defined(ENABLE_FORGE_TOUCH_INPUT)
static NSView*     pMainView = {};
static WindowDesc* pWindowRef = {};
#endif
/************************************************************************/
// Platform implementation
/************************************************************************/
void platformInitInput(WindowDesc* winDesc)
{
#if defined(ENABLE_FORGE_TOUCH_INPUT)
    ResetTouchEvents();
    gVirtualJoystickLeft.mActive = true;
    gVirtualJoystickRight.mActive = true;
#else
    pWindowRef = winDesc;
    void*   view = pWindowRef->handle.window;
    NSView* mainView = (__bridge NSView*)view;
    pMainView = mainView;
#endif

    if (IOS14_RUNTIME)
    {
        initMutex(&gRumbleMutex);

        // Mouse
        for (InputPortIndex c = 0; c < MAX_MICE; c++)
        {
            pMouseInputs[c] = nil;
        }

        [[NSNotificationCenter defaultCenter]
            addObserverForName:GCMouseDidConnectNotification
                        object:nil
                         queue:[NSOperationQueue mainQueue]
                    usingBlock:^(NSNotification* note) {
                        GCMouse*       mouse = note.object;
                        InputPortIndex portIndex = PORT_INDEX_INVALID;
                        for (InputPortIndex c = 0; c < MAX_MICE; c++)
                        {
                            if (pMouseInputs[c] == nil)
                            {
                                portIndex = c;
                                pMouseInputs[c] = mouse.mouseInput;
                                activeMouseCount++;
                                break;
                            }
                        }
                        if (portIndex == PORT_INDEX_INVALID)
                        {
                            return;
                        }
                        [pMouseInputs[portIndex] setMouseMovedHandler:^(GCMouseInput* _Nonnull mouse, float deltaX, float deltaY) {
                            gRawMouseDelta[0] += deltaX;
                            gRawMouseDelta[1] += deltaY;
                        }];
                    }];
        [[NSNotificationCenter defaultCenter] addObserverForName:GCMouseDidDisconnectNotification
                                                          object:nil
                                                           queue:[NSOperationQueue mainQueue]
                                                      usingBlock:^(NSNotification* note) {
                                                          GCMouse* mouse = note.object;
                                                          for (InputPortIndex c = 0; c < MAX_MICE; ++c)
                                                          {
                                                              if (pMouseInputs[c] == mouse.mouseInput)
                                                              {
                                                                  pMouseInputs[c] = nil;
                                                                  activeMouseCount--;
                                                                  break;
                                                              }
                                                          }
                                                      }];
        // Keyboard
        [[NSNotificationCenter defaultCenter]
            addObserverForName:GCKeyboardDidConnectNotification
                        object:nil
                         queue:[NSOperationQueue mainQueue]
                    usingBlock:^(NSNotification* note) {
                        GCKeyboard* kb = [GCKeyboard coalescedKeyboard];
                        pKeyboardInput = [kb keyboardInput];
                        [[kb keyboardInput] setKeyChangedHandler:^(GCKeyboardInput* _Nonnull keyboard,
                                                                   GCControllerButtonInput* _Nonnull key, GCKeyCode keyCode, BOOL pressed) {
                            gInputValues[GetKey(keyCode)] = pressed;
                        }];
                    }];
        [[NSNotificationCenter defaultCenter] addObserverForName:GCKeyboardDidDisconnectNotification
                                                          object:nil
                                                           queue:[NSOperationQueue mainQueue]
                                                      usingBlock:^(NSNotification* note) {
                                                          pKeyboardInput = nil;
                                                      }];
        // Gamepads
        [[NSNotificationCenter defaultCenter]
            addObserverForName:GCControllerDidConnectNotification
                        object:nil
                         queue:[NSOperationQueue mainQueue]
                    usingBlock:^(NSNotification* note) {
                        GCController* controller = note.object;
                        ASSERT(controller);
                        InputPortIndex portIndex = PORT_INDEX_INVALID;
                        for (uint32_t c = 0; c < MAX_GAMEPADS; ++c)
                        {
                            if (!inputGamepadIsActive(c))
                            {
                                portIndex = c;
                                break;
                            }
                        }
                        if (PORT_INDEX_INVALID == portIndex)
                        {
                            return;
                        }

                        if (!controller.extendedGamepad)
                        {
                            return;
                        }

                        static char controllerNames[MAX_GAMEPADS][FS_MAX_PATH] = {};
                        strncpy(controllerNames[portIndex], [controller.vendorName UTF8String], FS_MAX_PATH);

                        DarwinGamepad& dgpad = gDarwinGamepads[portIndex];
                        GamepadResetState(portIndex);

                        GCExtendedGamepad* ext = controller.extendedGamepad;
                        gGamepads[portIndex].mActive = true;
                        gGamepads[portIndex].pName = controllerNames[portIndex];
                        dgpad.pController = controller;
                        dgpad.pExtPad = ext;
                        if (controller.light)
                        {
                            dgpad.mInitialLight = { controller.light.color.red, controller.light.color.green, controller.light.color.blue };
                        }
                        RumbleAddDevice(dgpad);

                        if (gGamepadAddedCb)
                        {
                            gGamepadAddedCb(portIndex);
                        }
                    }];
        [[NSNotificationCenter defaultCenter] addObserverForName:GCControllerDidDisconnectNotification
                                                          object:nil
                                                           queue:[NSOperationQueue mainQueue]
                                                      usingBlock:^(NSNotification* note) {
                                                          InputPortIndex portIndex = PORT_INDEX_INVALID;
                                                          for (uint32_t c = 0; c < MAX_GAMEPADS; ++c)
                                                          {
                                                              DarwinGamepad& dgpad = gDarwinGamepads[c];
                                                              if (dgpad.pController == note.object)
                                                              {
                                                                  portIndex = c;
                                                                  RumbleRemoveDevice(dgpad);
                                                                  dgpad.pController = nil;
                                                                  dgpad.pExtPad = nil;
                                                                  gGamepads[portIndex].pName = gGamepadDisconnectedName;
                                                                  GamepadResetState(portIndex);
                                                                  if (gGamepadRemovedCb)
                                                                  {
                                                                      gGamepadRemovedCb(portIndex);
                                                                  }
                                                                  break;
                                                              }
                                                          }
                                                      }];
    }

    InputInitCommon();
}

void platformExitInput()
{
    GamepadDefault();
    for (InputPortIndex portIndex = 0; portIndex < MAX_GAMEPADS; ++portIndex)
    {
        // Stop rumbles
        GamepadUpdateState(portIndex);
    }
    if (IOS14_RUNTIME)
    {
        for (uint32_t c = 0; c < MAX_GAMEPADS; ++c)
        {
            DarwinGamepad& dgpad = gDarwinGamepads[c];
            RumbleRemoveDevice(dgpad);
            dgpad.pController = nil;
            dgpad.pExtPad = nil;
        }

        exitMutex(&gRumbleMutex);
    }
#if !defined(ENABLE_FORGE_TOUCH_INPUT)
    pMainView = nil;
#endif
}

void platformUpdateInput(uint32_t width, uint32_t height, float dt)
{
    memcpy(gLastInputValues, gInputValues, sizeof(gInputValues) - K_COUNT * sizeof(gInputValues[0]));
    gInputValues[MOUSE_DX] = 0;
    gInputValues[MOUSE_DY] = 0;

    // Gamepads
    for (uint32_t i = 0; i < MAX_GAMEPADS; ++i)
    {
        GamepadUpdateState(i);
    }

    // Cursor position
#if defined(ENABLE_FORGE_TOUCH_INPUT)
    ProcessTouchEvents(width, height, dt);
    CGPoint cursor = { (float)gCursorPos[0], (float)gCursorPos[1] };
#else
    // Mouse position relative to window
    NSPoint cursor = [pMainView.window mouseLocationOutsideOfEventStream];
    cursor.y = pMainView.bounds.size.height - cursor.y;
    cursor.x *= [pMainView.window backingScaleFactor];
    cursor.y *= [pMainView.window backingScaleFactor];
#endif
    gInputValues[MOUSE_X] = cursor.x;
    gInputValues[MOUSE_Y] = cursor.y;

    if (IOS14_RUNTIME)
    {
        if (activeMouseCount)
        {
            float wheelDelta = 0.0f;
            bool  leftButtonPressed = false;
            bool  rightButtonPressed = false;
            bool  middleButtonPressed = false;
            for (InputPortIndex c = 0; c < MAX_MICE; c++)
            {
                GCMouseInput* pMouseInput = pMouseInputs[c];
                if (pMouseInput != nil)
                {
                    leftButtonPressed = leftButtonPressed || pMouseInput.leftButton.pressed;
                    rightButtonPressed = rightButtonPressed || pMouseInput.rightButton.pressed;
                    middleButtonPressed = middleButtonPressed || pMouseInput.middleButton.pressed;
                    wheelDelta += pMouseInput.scroll.yAxis.value;
                }
            }
            // Ignore click on window title bar, ...
            gInputValues[MOUSE_1] = isCursorInsideTrackingArea() && leftButtonPressed;
            gInputValues[MOUSE_2] = rightButtonPressed;
            gInputValues[MOUSE_3] = middleButtonPressed;

            gInputValues[MOUSE_WHEEL_UP] = false;
            gInputValues[MOUSE_WHEEL_DOWN] = false;
            if (wheelDelta > 0.0f)
            {
                gInputValues[MOUSE_WHEEL_UP] = true;
            }
            else if (wheelDelta < 0.0f)
            {
                gInputValues[MOUSE_WHEEL_DOWN] = true;
            }
        }
    }

    // Raw mouse delta
    gInputValues[MOUSE_DX] = gRawMouseDelta[0];
    gInputValues[MOUSE_DY] = gRawMouseDelta[1];
    gRawMouseDelta[0] = 0.0f;
    gRawMouseDelta[1] = 0.0f;
    gDeltaTime = dt;

#if !defined(ENABLE_FORGE_TOUCH_INPUT)
    extern bool gCaptureCursorOnMouseDown;
    if (gCaptureCursorOnMouseDown)
    {
#if defined(ENABLE_FORGE_UI)
        extern bool uiIsFocused();
        const bool  capture = !uiIsFocused();
#else
        const bool capture = true;
#endif
        captureCursor(pWindowRef, capture && inputGetValue(0, MOUSE_1));
    }
#endif
}

void platformResetInputState()
{
    memcpy(gLastInputValues + K_FIRST - 1, gInputValues + K_FIRST - 1, K_COUNT * sizeof(gInputValues[0]));
    gCharacterBufferCount = 0;
}

#if defined(ENABLE_FORGE_TOUCH_INPUT)
void platformTouchBeganEvent(const int32_t id, const int32_t pos[2])
{
    gInputValues[MOUSE_1] = true;

    TouchEvent event = {};
    event.mPhase = TOUCH_BEGAN;
    event.mId = id;
    event.mPos[0] = pos[0];
    event.mPos[1] = pos[1];
    AddTouchEvent(event);
}

void platformTouchEndedEvent(const int32_t id, const int32_t pos[2])
{
    gInputValues[MOUSE_1] = false;

    TouchEvent event = {};
    event.mPhase = TOUCH_ENDED;
    event.mId = id;
    event.mPos[0] = pos[0];
    event.mPos[1] = pos[1];
    AddTouchEvent(event);
}

void platformTouchMovedEvent(const int32_t id, const int32_t pos[2])
{
    TouchEvent event = {};
    event.mPhase = TOUCH_MOVED;
    event.mId = id;
    event.mPos[0] = pos[0];
    event.mPos[1] = pos[1];
    AddTouchEvent(event);
}
#endif

void platformKeyChar(char32_t c)
{
    if (gCharacterBufferCount >= TF_ARRAY_COUNT(gCharacterBuffer))
    {
        return;
    }
    gCharacterBuffer[gCharacterBufferCount++] = c;
}
