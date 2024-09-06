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

#if defined(__INTELLISENSE__)
#include "../InputCommon.h"
#include "../../../OS/Input/HID/HIDInput.h"
#endif

#define PS5_REP_USB_IN                        0x01
#define PS5_REP_USB_OUT                       0x02
#define PS5_REP_BLUETOOTH_IN                  0x31
#define PS5_REP_BLUETOOTH_OUT                 0x31

#define PS5_PACKET_SIZE_USB_IN                64
#define PS5_PACKET_SIZE_USB_OUT               63
#define PS5_PACKET_SIZE_BLUETOOTH_IN          78
#define PS5_PACKET_SIZE_BLUETOOTH_OUT         78

#define PS5_FEATURE_REPORT_CALIBRATION        0x05
#define PS5_FEATURE_REPORT_PAIRING_INFO       0x09
#define PS5_FEATURE_REPORT_FIRMWARE_INFO      0x20

#define PS5_FEATURE_REPORT_SIZE_CALIBRATION   71
#define PS5_FEATURE_REPORT_SIZE_PAIRING_INFO  20
#define PS5_FEATURE_REPORT_SIZE_FIRMWARE_INFO 64

// Joystick
#define PS5_JX                                0
#define PS5_JY                                1

// Sensor
#define PS5_SX                                0
#define PS5_SY                                1
#define PS5_SZ                                2

// Triggers/Rumble
#define PS5_TL                                0
#define PS5_TR                                1

// Color
#define PS5_CR                                0
#define PS5_CG                                1
#define PS5_CB                                2

#define PS5_EFFECTS_PRE_RUMBLE                (1 << 0)
#define PS5_EFFECTS_LED_RESET                 (1 << 1)
#define PS5_EFFECTS_LED                       (1 << 2)
#define PS5_EFFECTS_LIGHTS_PAD                (1 << 3)
#define PS5_EFFECTS_LIGHTS_MIC                (1 << 4)

#define PS5_MIC_OFF                           0
#define PS5_MIC_SOLID                         1
#define PS5_MIC_PULSE                         2
#define PS5_MIC_LAST                          PS5_MIC_PULSE

struct PS5ParsedInput
{
    // stored after being converted to an int and having the deadzone
    //   subtracted out
    int8_t   leftJoystick[2];
    int8_t   rightJoystick[2];
    uint8_t  trigger[2];
    //  0 : dpad up
    //  1 : dpad right
    //  2 : dpad down
    //  3 : dpad left
    //  4 : []
    //  5 : X
    //  6 : O
    //  7 : A
    //  8 : L Shoulder
    //  9 : R Shoulder
    // 10 : Share
    // 11 : Menu
    // 12 : L Stick
    // 13 : R Stick
    // 14 : Home/PS
    // 15 : Mic Button
    uint16_t buttons;
    //  0 : TouchPad Button
    //  1 : Touch 0
    //  2 : Touch 1
    uint8_t  touch;
    // 0-3 : Battery level
    //   4 : Is Wired Bool
    //   5 : Battery Fully Charged
    //   6 : Headphones
    //   7 : USB
    uint8_t  misc;
    uint16_t touch0[2];
    uint16_t touch1[2];
    int16_t  gyro[3];
    int16_t  accel[3];
    // Can be useful for smoothing sensors, not currently used though
    uint32_t sensorTime;
};

#define PS5_DP_UP 0x0001
#define PS5_DP_RT 0x0002
#define PS5_DP_DN 0x0004
#define PS5_DP_LT 0x0008
#define PS5_SQUAR 0x0010
#define PS5_CROSS 0x0020
#define PS5_CIRCL 0x0040
#define PS5_TRIAN 0x0080
#define PS5_L_SHD 0x0100
#define PS5_R_SHD 0x0200
#define PS5_SHARE 0x0400
#define PS5_MENU_ 0x0800
#define PS5_L_STK 0x1000
#define PS5_R_STK 0x2000
#define PS5_HOME_ 0x4000
#define PS5_MICPH 0x8000

#define PS5_TCHBT 0x01
#define PS5_TCH_0 0x02
#define PS5_TCH_1 0x04

#define PS5_B_LVL 0x0F
#define PS5_WIRED 0x10
#define PS5_B_CHG 0x20
#define PS5_HDPHN 0x40
#define PS5_USB__ 0x80

struct PS5Controller
{
    uint8_t bluetooth : 1;                 // 1b
    uint8_t btLightPending : 1;            // 1b
    uint8_t useStandardColors : 1;         // 1b
    uint8_t useStandardTrackpadLights : 1; // 1b
    uint8_t ledColor[3];                   // rgb
    uint8_t ledTouchpad;
    uint8_t ledMicrophone;
    uint8_t rumble[2]; // lr
};
// if exceeded, increase HID_CONTROLLER_BUFFER_SIZE
COMPILE_ASSERT(sizeof(PS5Controller) <= HID_CONTROLLER_BUFFER_SIZE);

struct PS5CInputReport
{
    // ensures sensors sit on word boundaries
    // if the packet is over usb this would be the id, but over bluetooth has additional data
    uint8_t __pad0;

    uint8_t leftJoystick[2];
    uint8_t rightJoystick[2];
    uint8_t trigger[2];
    uint8_t reportNum;
    uint8_t buttons[4];

    uint8_t __pad1[4]; // some form of timestamp or sequence data?

    int16_t  gyro[3];  // is signed
    int16_t  accel[3]; // is signed
    uint32_t sensorTime;
    uint8_t  batteryTemp;
    struct
    {
        uint8_t count;
        uint8_t x;
        uint8_t xhi_ylo;
        uint8_t y;
    } touch[2];

    uint8_t __pad2[8];
    uint8_t __pad3[4]; // some form of timestamp or sequence data?

    uint8_t batteryLevel;
    uint8_t wiresConnected; // 0x1 is headphones, 0x8 is usb

    uint8_t __pad4[9];

    // Bluetooth also has an extended report with some extra data and
    //   a crc hash but we don't care
};
// this patch of the block needs strict alignment
COMPILE_ASSERT(offsetof(PS5CInputReport, __pad2) == 41);
COMPILE_ASSERT(sizeof(PS5CInputReport) == PS5_PACKET_SIZE_USB_IN);

// Can be received when using BT predominantly
struct PS5CInputReportSimple
{
    // ensures sensors sit on word boundaries
    // if the packet is over usb this would be the id, but over bluetooth has additional data
    uint8_t __pad0;

    uint8_t leftJoystick[2];
    uint8_t rightJoystick[2];
    uint8_t buttons[3];
    uint8_t trigger[2];
};

#define PS5_DEADZONE_OFFSET (uint8_t)(0x7F * ((double)0.05))
#define PS5_UPPER_OFFSET    (0x7F + PS5_DEADZONE_OFFSET)
#define PS5_LOWER_OFFSET    (0x7F - PS5_DEADZONE_OFFSET)
#define PS5_CENTERING       (0x7F)
// +1 to deal with the offset in negative values, plus positive values are
//   shifted by one to align with negative values
// NOTE: with a hard coded precision of 0.15 this maps cleanly, but other
//   values may not
#define PS5_SCALING         (float)((0xFF - (PS5_CENTERING + 1 + PS5_DEADZONE_OFFSET)))
#define PS5_TRIG_SCALING    (float)(0xFF)
#define PS5_TOUCHX_SCALING  (float)(1919.0f)
#define PS5_TOUCHY_SCALING  (float)(1079.0f)
#define PS5_SENSOR_SCALING  (float)(0x7FFF)
// having positive map to 128 >= bits and negative only to 127 leads to a
//   precision mismatch. Additionally, 1 / 109 was not quite reaching 1,
//   leaving .999994 due to floating point error
#define PS5_JOY_TO_INT(v)                                                                 \
    ((v) > PS5_UPPER_OFFSET   ? (int8_t)((v) - (PS5_CENTERING + 1 + PS5_DEADZONE_OFFSET)) \
     : (v) < PS5_LOWER_OFFSET ? (int8_t)((v) - (PS5_CENTERING - PS5_DEADZONE_OFFSET))     \
                              : 0)
#define PS5_BALANCE_SENSOR(v) ((v) < 0 ? (v) + 1 : (v))

struct PS5CEffectsState
{
    uint8_t flagsHapticsEnable;
    uint8_t flagsLedsEnable;
    uint8_t rumbleRight; // high freq
    uint8_t rumbleLeft;  // low freq

    uint8_t volHeadphone;
    uint8_t volSpeaker;
    uint8_t volMicrophone;
    uint8_t flagsAudioEnable;

    uint8_t ledMicrophone;
    uint8_t flagsAudioMute;
    uint8_t triggerHapticsRight[11];
    uint8_t triggerHapticsLeft[11];

    uint8_t __pad0[6];
    uint8_t flagsLed;
    uint8_t __pad1[2];
    uint8_t ledAnimations;
    uint8_t ledBightness;
    uint8_t lightsTrackpad;

    uint8_t ledR;
    uint8_t ledG;
    uint8_t ledB;
};
COMPILE_ASSERT(sizeof(PS5CEffectsState) == 47);

// Info

bool HIDIsSupportedPS5Controller(HIDDeviceInfo* devInfo)
{
    // Currently have no controllers disguising themselves as a PS5 controller
    //   that are causing issues
    return devInfo->mType == CONTROLLER_TYPE_PS5;
}

// Effects

static PS5CEffectsState* PrepEffectsPacket(PS5Controller* con, uint8_t* data, int* outSize, int* outOffset)
{
    ASSERT(data && outSize && outOffset);

    if (con->bluetooth)
    {
        data[0] = PS5_REP_BLUETOOTH_OUT;
        data[1] = 0x02;

        *outSize = 78;
        *outOffset = 2;
    }
    else
    {
        data[0] = PS5_REP_USB_OUT;

        *outSize = 48;
        *outOffset = 1;
    }

    return (PS5CEffectsState*)(data + *outOffset);
}

static void HIDSendEffectsPS5Controller(HIDController* controller, int effectsToSend)
{
    PS5Controller* con = (PS5Controller*)controller->mData;

    uint8_t data[PS5_PACKET_SIZE_BLUETOOTH_OUT];
    memset(data, 0, sizeof data);
    int size = 0;
    int offset = 0;

    PS5CEffectsState* effects = PrepEffectsPacket(con, data, &size, &offset);

    // wait on light changes until after bt has finished its sequence
    if (con->btLightPending && (effectsToSend & (PS5_EFFECTS_LED | PS5_EFFECTS_LIGHTS_PAD)))
        return;

    // Present rumble state must always be part of the packet
    uint8_t doRumble = con->rumble[PS5_TL] || con->rumble[PS5_TR];
    // if 1
    //   enable rumble, disable rumble audio
    // if 0
    //   disable rumble, enable rumble audio
    effects->flagsHapticsEnable |= doRumble << 0;
    effects->flagsHapticsEnable |= doRumble << 1;

    effects->rumbleLeft = con->rumble[PS5_TL];
    effects->rumbleRight = con->rumble[PS5_TR];

    if (effectsToSend & PS5_EFFECTS_PRE_RUMBLE)
    {
        // disable rumble audio
        effects->flagsHapticsEnable |= 1 << 1;
    }
    if (effectsToSend & PS5_EFFECTS_LED_RESET)
    {
        // flag for reset
        effects->flagsLedsEnable |= 1 << 3;
    }
    if (effectsToSend & PS5_EFFECTS_LED)
    {
        // enable setting color
        effects->flagsLedsEnable |= 1 << 2;

        effects->ledR = con->ledColor[PS5_CR];
        effects->ledG = con->ledColor[PS5_CG];
        effects->ledB = con->ledColor[PS5_CB];
    }
    if (effectsToSend & PS5_EFFECTS_LIGHTS_PAD)
    {
        // enable setting touchpad light
        effects->flagsLedsEnable |= 1 << 4;

        effects->lightsTrackpad = con->ledTouchpad;
    }
    if (effectsToSend & PS5_EFFECTS_LIGHTS_MIC)
    {
        // enable setting mic light
        effects->flagsLedsEnable |= 1 << 0;

        effects->ledMicrophone = con->ledMicrophone;
    }

    if (con->bluetooth)
    {
        uint8_t  hdr = 0xA2;
        uint32_t crc = crc32(0, &hdr, 1);
        crc = crc32(crc, data, (uint32_t)(size - sizeof crc));
        memcpy(data + (size - sizeof crc), &crc, sizeof crc);
    }

    hid_write(controller->pDevice, data, size);
}

// Lights

static void HIDUpdateBTLightStatusPS5Controller(HIDController* controller, uint32_t pDeviceTime)
{
    const uint32_t btLightMaxBlinkPeriod = 10200000;
    if (pDeviceTime >= btLightMaxBlinkPeriod)
    {
        PS5Controller* con = (PS5Controller*)controller->mData;
        con->btLightPending = 0;

        HIDSendEffectsPS5Controller(controller, PS5_EFFECTS_LED_RESET);
        HIDSendEffectsPS5Controller(controller, PS5_EFFECTS_LED | PS5_EFFECTS_LIGHTS_PAD);
    }
}

static void HIDSetLightsPS5Controller(HIDController* controller, uint8_t r, uint8_t g, uint8_t b)
{
    PS5Controller* con = (PS5Controller*)controller->mData;

    con->ledColor[PS5_CR] = r;
    con->ledColor[PS5_CG] = g;
    con->ledColor[PS5_CB] = b;

    con->useStandardColors = 0;

    HIDSendEffectsPS5Controller(controller, PS5_EFFECTS_LED);
}

// Haptics
static void HIDDoRumblePS5Controller(HIDController* controller, uint16_t left, uint16_t right)
{
    PS5Controller* con = (PS5Controller*)controller->mData;

    HIDSendEffectsPS5Controller(controller, PS5_EFFECTS_PRE_RUMBLE);

    con->rumble[PS5_TR] = right >> 8;
    con->rumble[PS5_TL] = left >> 8;

    HIDSendEffectsPS5Controller(controller, 0);
}

#if 0
// #TODO
static void HIDDoRumbleTriggersPS5Controller(HIDController * controller, uint16_t left, uint16_t right)
{
    // Not Set Up
}
#endif
// Misc

// Input

static PS5ParsedInput ParseFullInputReport(PS5CInputReport* rep)
{
    PS5ParsedInput data;
    memset(&data, 0, sizeof data);

    data.leftJoystick[PS5_JX] = PS5_JOY_TO_INT(rep->leftJoystick[PS5_JX]);
    data.leftJoystick[PS5_JY] = PS5_JOY_TO_INT(rep->leftJoystick[PS5_JY]);
    data.rightJoystick[PS5_JX] = PS5_JOY_TO_INT(rep->rightJoystick[PS5_JX]);
    data.rightJoystick[PS5_JY] = PS5_JOY_TO_INT(rep->rightJoystick[PS5_JY]);

    data.trigger[PS5_TL] = rep->trigger[PS5_TL];
    data.trigger[PS5_TR] = rep->trigger[PS5_TR];

    uint8_t dpadState = rep->buttons[0] & 0xF;

    // dpad buttons
    switch (dpadState)
    {
    case 0:
        data.buttons |= 0b0001;
        break;
    case 1:
        data.buttons |= 0b0011;
        break;
    case 2:
        data.buttons |= 0b0010;
        break;
    case 3:
        data.buttons |= 0b0110;
        break;
    case 4:
        data.buttons |= 0b0100;
        break;
    case 5:
        data.buttons |= 0b1100;
        break;
    case 6:
        data.buttons |= 0b1000;
        break;
    case 7:
        data.buttons |= 0b1001;
        break;
    case 8:
        data.buttons |= 0b0000;
        break;
    default:
        ASSERT(0);
        break;
    }

    // shape buttons
    // button in report should be in same order as in mapped input (4 : [], 5 : X, 6 : O, 7 : A)
    data.buttons |= rep->buttons[0] & 0xF0;

    // shoulder buttons
    data.buttons |= (uint16_t)(rep->buttons[1] & 0x03) << 8;
    // share, menu, and stick buttons
    data.buttons |= (uint16_t)(rep->buttons[1] & 0xF0) << (-2 + 8);
    // home button
    data.buttons |= (uint16_t)(rep->buttons[2] & 0x01) << (6 + 8);
    // mic button
    data.buttons |= (uint16_t)(rep->buttons[2] & 0x04) << (5 + 8);

    // touchpad button
    data.touch |= (rep->buttons[2] & 0x02) >> 1;
    // bits 0-6 are a counter of the touch #, bit 7 is high when there is nothing and low
    //   when there is a touch
    data.touch |= ((rep->touch[0].count & 0x80) ^ 0x80) >> 6;
    data.touch |= ((rep->touch[1].count & 0x80) ^ 0x80) >> 5;

    // Get battery level and state
    data.misc |= (rep->batteryLevel & 0x3F);
    // Headphones
    data.misc |= (rep->wiresConnected & 0x01) << 6;
    // USB
    data.misc |= (rep->wiresConnected & 0x08) << 4;

    // Touch 0 x and y
    data.touch0[PS5_JX] = ((uint16_t)(rep->touch[0].xhi_ylo & 0x0F) << 8) | rep->touch[0].x;
    data.touch0[PS5_JY] = ((uint16_t)rep->touch[0].y << 4) | ((rep->touch[0].xhi_ylo & 0xF0) >> 4);
    // Touch 1 x and y
    data.touch1[PS5_JX] = ((uint16_t)(rep->touch[1].xhi_ylo & 0x0F) << 8) | rep->touch[1].x;
    data.touch1[PS5_JY] = ((uint16_t)rep->touch[1].y << 4) | ((rep->touch[1].xhi_ylo & 0xF0) >> 4);

    data.gyro[PS5_SX] = PS5_BALANCE_SENSOR(rep->gyro[PS5_SX]);
    data.gyro[PS5_SY] = PS5_BALANCE_SENSOR(rep->gyro[PS5_SY]);
    data.gyro[PS5_SZ] = PS5_BALANCE_SENSOR(rep->gyro[PS5_SZ]);
    data.accel[PS5_SX] = PS5_BALANCE_SENSOR(rep->accel[PS5_SX]);
    data.accel[PS5_SY] = PS5_BALANCE_SENSOR(rep->accel[PS5_SY]);
    data.accel[PS5_SZ] = PS5_BALANCE_SENSOR(rep->accel[PS5_SZ]);
    data.sensorTime = rep->sensorTime;

    return data;
}

static PS5ParsedInput ParseSimpleInputReport(PS5CInputReportSimple* rep)
{
    PS5ParsedInput data;
    memset(&data, 0, sizeof data);

    uint8_t dpadState = rep->buttons[0] & 0xF;

    // Store dpad values
    switch (dpadState)
    {
    case 0:
        data.buttons |= 0b0001;
        break;
    case 1:
        data.buttons |= 0b0011;
        break;
    case 2:
        data.buttons |= 0b0010;
        break;
    case 3:
        data.buttons |= 0b0110;
        break;
    case 4:
        data.buttons |= 0b0100;
        break;
    case 5:
        data.buttons |= 0b1100;
        break;
    case 6:
        data.buttons |= 0b1000;
        break;
    case 7:
        data.buttons |= 0b1001;
        break;
    case 8:
        data.buttons |= 0b0000;
        break;
    default:
        ASSERT(0);
        break;
    }

    // store shape buttons
    // button in report should be in same order as in mapped input (4 : [], 5 : X, 6 : O, 7 : A)
    data.buttons |= rep->buttons[0] & 0xF0;

    // store more misc buttons (should also be in same order (bits 6 and 7 don't have data last checked))
    data.buttons |= rep->buttons[1] & 0x3F;
    // get Home
    data.buttons |= (rep->buttons[2] & 0x01) << 6;
    // and Mic buttons
    data.buttons |= (rep->buttons[2] & 0x04) << 5;

    // Touchpad button
    data.touch |= (rep->buttons[2] & 0x02) >> 1;

    data.leftJoystick[PS5_JX] = PS5_JOY_TO_INT(rep->leftJoystick[PS5_JX]);
    data.leftJoystick[PS5_JY] = PS5_JOY_TO_INT(rep->leftJoystick[PS5_JY]);
    data.rightJoystick[PS5_JX] = PS5_JOY_TO_INT(rep->rightJoystick[PS5_JX]);
    data.rightJoystick[PS5_JY] = PS5_JOY_TO_INT(rep->rightJoystick[PS5_JY]);

    data.trigger[PS5_TL] = rep->trigger[PS5_TL];
    data.trigger[PS5_TR] = rep->trigger[PS5_TR];

    return data;
}

static void HIDParsePS5BasicInputReport(Gamepad& gpad, const PS5ParsedInput& curr)
{
    // Buttons
    gpad.mButtons[GPAD_UP - GPAD_BTN_FIRST] = (bool)(curr.buttons & PS5_DP_UP);
    gpad.mButtons[GPAD_DOWN - GPAD_BTN_FIRST] = (bool)(curr.buttons & PS5_DP_DN);
    gpad.mButtons[GPAD_LEFT - GPAD_BTN_FIRST] = (bool)(curr.buttons & PS5_DP_LT);
    gpad.mButtons[GPAD_RIGHT - GPAD_BTN_FIRST] = (bool)(curr.buttons & PS5_DP_RT);
    gpad.mButtons[GPAD_A - GPAD_BTN_FIRST] = (bool)(curr.buttons & PS5_CROSS);
    gpad.mButtons[GPAD_B - GPAD_BTN_FIRST] = (bool)(curr.buttons & PS5_CIRCL);
    gpad.mButtons[GPAD_X - GPAD_BTN_FIRST] = (bool)(curr.buttons & PS5_SQUAR);
    gpad.mButtons[GPAD_Y - GPAD_BTN_FIRST] = (bool)(curr.buttons & PS5_TRIAN);
    gpad.mButtons[GPAD_L1 - GPAD_BTN_FIRST] = (bool)(curr.buttons & PS5_L_SHD);
    gpad.mButtons[GPAD_R1 - GPAD_BTN_FIRST] = (bool)(curr.buttons & PS5_R_SHD);
    gpad.mButtons[GPAD_L3 - GPAD_BTN_FIRST] = (bool)(curr.buttons & PS5_L_STK);
    gpad.mButtons[GPAD_R3 - GPAD_BTN_FIRST] = (bool)(curr.buttons & PS5_R_STK);
    gpad.mButtons[GPAD_START - GPAD_BTN_FIRST] = (bool)(curr.buttons & PS5_MENU_);
    gpad.mButtons[GPAD_BACK - GPAD_BTN_FIRST] = (bool)(curr.touch & PS5_TCHBT);

    // Left Joystick
    gpad.mAxis[GPAD_LX - GPAD_AXIS_FIRST] = curr.leftJoystick[PS5_JX] / PS5_SCALING;
    gpad.mAxis[GPAD_LY - GPAD_AXIS_FIRST] = curr.leftJoystick[PS5_JY] / -PS5_SCALING; // Invert Y
    // Right Joystick
    gpad.mAxis[GPAD_RX - GPAD_AXIS_FIRST] = curr.rightJoystick[PS5_JX] / PS5_SCALING;
    gpad.mAxis[GPAD_RY - GPAD_AXIS_FIRST] = curr.rightJoystick[PS5_JY] / -PS5_SCALING; // Invert Y
    // Left Trigger
    gpad.mAxis[GPAD_L2 - GPAD_AXIS_FIRST] = curr.trigger[PS5_TL] / PS5_TRIG_SCALING;
    // Right Trigger
    gpad.mAxis[GPAD_R2 - GPAD_AXIS_FIRST] = curr.trigger[PS5_TR] / PS5_TRIG_SCALING;
}

static void HIDParsePS5CInputReport(PS5Controller* con, PS5CInputReport* currRep, InputPortIndex portIndex)
{
    UNREF_PARAM(con);
    PS5ParsedInput curr = ParseFullInputReport(currRep);
    Gamepad&       gpad = gGamepads[portIndex];
    GamepadUpdateLastState(portIndex);
    HIDParsePS5BasicInputReport(gpad, curr);
    GamepadPostProcess(portIndex);
}

static void HIDParsePS5ControllerSimpleReport(PS5Controller* con, PS5CInputReportSimple* currRep, InputPortIndex portIndex)
{
    UNREF_PARAM(con);
    PS5ParsedInput curr = ParseSimpleInputReport(currRep);
    Gamepad&       gpad = gGamepads[portIndex];
    GamepadUpdateLastState(portIndex);
    HIDParsePS5BasicInputReport(gpad, curr);
    GamepadPostProcess(portIndex);
}

// General

static int HIDUpdatePS5Controller(HIDController* controller)
{
    PS5Controller* con = (PS5Controller*)controller->mData;
    hid_device*    dev = (hid_device*)controller->pDevice;

    uint8_t data[PS5_PACKET_SIZE_BLUETOOTH_IN];
    int32_t size;

    // it seems that we are getting usb style packets while on bluetooth,
    //   so type resolution needs to be handled latter
    for (; (size = hid_read_timeout(dev, data, PS5_PACKET_SIZE_BLUETOOTH_IN, 0)) > 0;)
    {
        ASSERT(size <= PS5_PACKET_SIZE_BLUETOOTH_IN);

        if (*data == PS5_REP_USB_IN)
        {
            if (size == 10 || size == 78)
            {
                // Handle the occasional simplified packet
                HIDParsePS5ControllerSimpleReport(con, (PS5CInputReportSimple*)(data), controller->mPortIndex); //-V641
            }
            else
            {
                // Normal USB input
                HIDParsePS5CInputReport(con, (PS5CInputReport*)(data), controller->mPortIndex); //-V::641, 1032
            }
        }
        else if (*data == PS5_REP_BLUETOOTH_IN)
        {
            PS5CInputReport* report = (PS5CInputReport*)(data + 1);
            if (con->btLightPending)
            {
                HIDUpdateBTLightStatusPS5Controller(controller, report->sensorTime);
            }

            HIDParsePS5CInputReport(con, report, controller->mPortIndex);
        }
    }

    // Controller should be disconnected if this is true
    if (size < 0)
    {
        return -1;
    }

    return 0;
}

static void HIDClosePS5Controller(HIDController* controller)
{
    hid_close((hid_device*)controller->pDevice);
    controller->pDevice = NULL;
}

int HIDOpenPS5Controller(HIDDeviceInfo* devInfo, HIDController* controller)
{
    hid_device* pDevice = hid_open_path(devInfo->mLogicalSystemPath);
    if (!pDevice)
    {
        LOGF(LogLevel::eWARNING, "HID Open failed on device %s", devInfo->mLogicalSystemPath);
        LOGF(LogLevel::eWARNING, "   hid err str: %ls", hid_error(pDevice));
        return -1;
    }

    // store the device
    controller->pDevice = pDevice;
    controller->Update = HIDUpdatePS5Controller;
    controller->Close = HIDClosePS5Controller;

    controller->SetLights = HIDSetLightsPS5Controller;
    controller->DoRumble = HIDDoRumblePS5Controller;

    // initialized the opaque buffer
    PS5Controller* con = (PS5Controller*)&controller->mData; //-V641
    memset(con, 0, sizeof *con);

    uint8_t data[PS5_PACKET_SIZE_BLUETOOTH_IN];
    // Get the current state
    int32_t size = hid_read_timeout(pDevice, data, PS5_PACKET_SIZE_BLUETOOTH_IN, 16);

    con->bluetooth = size != PS5_PACKET_SIZE_USB_IN ? 1 : 0;
    con->btLightPending = con->bluetooth;
    con->useStandardColors = 0;
    con->useStandardTrackpadLights = 0;

    // Pairing info needs to be requested to get full reports over bluetooth
    uint8_t pairingInfo[PS5_FEATURE_REPORT_SIZE_PAIRING_INFO];
    *pairingInfo = PS5_FEATURE_REPORT_PAIRING_INFO;
    uint8_t devID[6];
    memset(devID, 0, sizeof(devID));
    if (hid_get_feature_report(pDevice, pairingInfo, PS5_FEATURE_REPORT_SIZE_PAIRING_INFO) == PS5_FEATURE_REPORT_SIZE_PAIRING_INFO)
    {
        devID[0] = pairingInfo[6];
        devID[1] = pairingInfo[5];
        devID[2] = pairingInfo[4];
        devID[3] = pairingInfo[3];
        devID[4] = pairingInfo[2];
        devID[5] = pairingInfo[1];

        LOGF(LogLevel::eINFO, "Device ID is: %.2X-%.2X-%.2X-%.2X-%.2X-%.2X", devID[0], devID[1], devID[2], devID[3], devID[4], devID[5]);
    }

    return 0;
}
