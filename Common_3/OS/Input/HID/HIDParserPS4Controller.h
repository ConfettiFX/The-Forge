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

#define PS4_USB_INPUT_REPORT_SIZE     64
#define PS4_BT_INPUT_REPORT_SIZE      78

#define PS4_BASIC_INPUT_REPORT_ID     0x01
#define PS4_EXT_INPUT_REPORT_ID_FIRST 0x11
#define PS4_EXT_INPUT_REPORT_ID_LAST  0x19
#define PS4_REPORT_DISCONNECT         0xE2

#define PS4_USB_EFFECTS_ID            0x05
#define PS4_BT_EFFECTS_ID             0x11

#define PS4_FEATURE_REPORT_DEVICE_ID  0x12

// Joystick
#define PS4_JX                        0
#define PS4_JY                        1

// Sensor
#define PS4_SX                        0
#define PS4_SY                        1
#define PS4_SZ                        2

// Triggers/Rumble
#define PS4_TL                        0
#define PS4_TR                        1

// Color
#define PS4_CR                        0
#define PS4_CG                        1
#define PS4_CB                        2

struct PS4ParsedInput
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
    // 15 : TouchPad Button
    uint16_t buttons;
    //  6 : Touch 0
    //  7 : Touch 1
    uint8_t  touch;
    // 0-3 : Battery level
    //   4 : Is Wired Bool
    //   5 : Headphones
    //   6 : Microphone
    uint8_t  misc;
    uint16_t touch0[2];
    uint16_t touch1[2];
    int16_t  gyro[3];
    int16_t  accel[3];
};

#define PS4_DP_UP 0x0001
#define PS4_DP_RT 0x0002
#define PS4_DP_DN 0x0004
#define PS4_DP_LT 0x0008
#define PS4_SQUAR 0x0010
#define PS4_CROSS 0x0020
#define PS4_CIRCL 0x0040
#define PS4_TRIAN 0x0080
#define PS4_L_SHD 0x0100
#define PS4_R_SHD 0x0200
#define PS4_SHARE 0x0400
#define PS4_MENU_ 0x0800
#define PS4_L_STK 0x1000
#define PS4_R_STK 0x2000
#define PS4_HOME_ 0x4000
#define PS4_TCHBT 0x8000

#define PS4_TCH_0 0x40
#define PS4_TCH_1 0x80

struct PS4Controller
{
    uint8_t bluetooth : 1;      // 1b
    uint8_t official : 1;       // 1b
    uint8_t dongle : 1;         // 1b
    uint8_t effectsEnabled : 1; // 1b

    uint8_t rumble[2]; // high freq
    uint8_t ledColor[3];
};
// if exceeded, increase HID_CONTROLLER_BUFFER_SIZE
COMPILE_ASSERT(sizeof(PS4Controller) <= HID_CONTROLLER_BUFFER_SIZE);

struct PS4CInputReport
{
    // if the packet is over usb this would be the id, but over bluetooth has additional data
    // needs to be commented out for proper alignment
    // uint8_t __pad0;

    uint8_t leftJoystick[2];
    uint8_t rightJoystick[2];
    uint8_t buttons[3];
    uint8_t trigger[2];

    uint8_t __pad1[3];

    int16_t gyro[3];  // is signed
    int16_t accel[3]; // is signed

    uint8_t __pad2[5];

    // 0-3 appears to be power level
    // 4 is usb
    // 5 seems to be if headphones are plugged in
    // 6 seems to be if the headset supports a mic
    // 7 blind guess but it probably is true if a peripheral is plugged into the EXT port
    uint8_t batteryLevel;

    uint8_t __pad3[4];

    struct
    {
        uint8_t count;
        uint8_t x;
        uint8_t xhi_ylo;
        uint8_t y;
    } touch[2];
};
COMPILE_ASSERT(sizeof(PS4CInputReport) == 42);
COMPILE_ASSERT(offsetof(PS4CInputReport, gyro) == 12);

static double PS4_DEADZONE = 0.05;
#define PS4_DEADZONE_OFFSET (uint8_t)(0x7F * PS4_DEADZONE)
#define PS4_UPPER_OFFSET    (0x7F + PS4_DEADZONE_OFFSET)
#define PS4_LOWER_OFFSET    (0x7F - PS4_DEADZONE_OFFSET)
#define PS4_CENTERING       (0x7F)
// +1 to deal with the offset in negative values, plus positive values are
//   shifted by one to align with negative values
// NOTE: with a hard coded precision of 0.15 this maps cleanly, but other
//   values may not
#define PS4_SCALING         (float)((0xFF - (PS4_CENTERING + 1 + PS4_DEADZONE_OFFSET)))
#define PS4_TRIG_SCALING    (float)(0xFF)
#define PS4_TOUCHX_SCALING  (float)(1919.0f)
#define PS4_TOUCHY_SCALING  (float)(941.0f)
#define PS4_SENSOR_SCALING  (float)(0x7FFF)
// having positive map to 128 >= bits and negative only to 127 leads to a
//   precision mismatch. Additionally, 1 / 109 was not quite reaching 1,
//   leaving .999994 due to floating point error
#define PS4_JOY_TO_INT(v)                                                                 \
    ((v) > PS4_UPPER_OFFSET   ? (int8_t)((v) - (PS4_CENTERING + 1 + PS4_DEADZONE_OFFSET)) \
     : (v) < PS4_LOWER_OFFSET ? (int8_t)((v) - (PS4_CENTERING - PS4_DEADZONE_OFFSET))     \
                              : 0)
#define PS4_BALANCE_SENSOR(v) ((v) < 0 ? (v) + 1 : (v))

struct PS4CEffectsState
{
    uint8_t rumbleRight; // high freq
    uint8_t rumbleLeft;  // low freq
    uint8_t ledR;
    uint8_t ledG;
    uint8_t ledB;
    uint8_t ledDelayOn;
    uint8_t ledDelayOff;
    uint8_t __pad0[8];
    uint8_t volLeft;
    uint8_t volRight;
    uint8_t volMic;
    uint8_t volSpeaker;
};
COMPILE_ASSERT(sizeof(PS4CEffectsState) == 19);

// Info

bool HIDIsSupportedPS4Controller(HIDDeviceInfo* devInfo)
{
    // Add filters to exclude 3rd party controllers that are causing issues here
    return devInfo->mType == CONTROLLER_TYPE_PS4;
}

// Effects

static void HIDSendEffectsPS4Controller(HIDController* controller)
{
    PS4Controller* con = (PS4Controller*)controller->mData;

    if (!con->effectsEnabled)
        return;

    uint8_t data[PS4_BT_INPUT_REPORT_SIZE];
    memset(data, 0, sizeof data);
    int32_t size = 0;
    int     offset = 0;

    if (con->bluetooth)
    {
        data[0] = PS4_BT_EFFECTS_ID;
        data[1] = 0xC0 | 0x04;
        data[3] = 0x03;

        size = 78;
        offset = 6;
    }
    else
    {
        data[0] = PS4_USB_EFFECTS_ID;
        data[1] = 0x07;

        size = 32;
        offset = 4;
    }

    PS4CEffectsState* effects = (PS4CEffectsState*)(data + offset);

    effects->rumbleRight = con->rumble[PS4_TR];
    effects->rumbleLeft = con->rumble[PS4_TL];
    effects->ledR = con->ledColor[PS4_CR];
    effects->ledG = con->ledColor[PS4_CG];
    effects->ledB = con->ledColor[PS4_CB];

    if (con->bluetooth)
    {
        uint8_t  hdr = 0xA2;
        uint32_t crc = crc32(0, &hdr, 1);
        crc = crc32(crc, data, (uint32_t)(size - sizeof crc));
        memcpy(data + (size - sizeof crc), &crc, sizeof crc);
    }

    hid_write(controller->pDevice, data, size);
}

static void HIDSetLightsPS4Controller(HIDController* controller, uint8_t r, uint8_t g, uint8_t b)
{
    PS4Controller* con = (PS4Controller*)controller->mData;

    con->ledColor[PS4_CR] = r;
    con->ledColor[PS4_CG] = g;
    con->ledColor[PS4_CB] = b;

    HIDSendEffectsPS4Controller(controller);
}

static void HIDDoRumblePS4Controller(HIDController* controller, uint16_t left, uint16_t right)
{
    PS4Controller* con = (PS4Controller*)controller->mData;

    con->rumble[PS4_TR] = right >> 8;
    con->rumble[PS4_TL] = left >> 8;

    HIDSendEffectsPS4Controller(controller);
}

// Input

static PS4ParsedInput ParseFullInputReport(PS4CInputReport* rep)
{
    PS4ParsedInput data;
    memset(&data, 0, sizeof data);

    data.leftJoystick[PS4_JX] = PS4_JOY_TO_INT(rep->leftJoystick[PS4_JX]);
    data.leftJoystick[PS4_JY] = PS4_JOY_TO_INT(rep->leftJoystick[PS4_JY]);
    data.rightJoystick[PS4_JX] = PS4_JOY_TO_INT(rep->rightJoystick[PS4_JX]);
    data.rightJoystick[PS4_JY] = PS4_JOY_TO_INT(rep->rightJoystick[PS4_JY]);

    data.trigger[PS4_TL] = rep->trigger[PS4_TL];
    data.trigger[PS4_TR] = rep->trigger[PS4_TR];

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
    // home and touchpad buttons
    data.buttons |= (uint16_t)(rep->buttons[2] & 0x03) << (6 + 8);

    // bits 0-6 are a counter of the touch #, bit 7 is high when there is nothing and low
    //   when there is a touch
    data.touch |= ((rep->touch[0].count & 0x80) ^ 0x80) >> 1;
    data.touch |= ((rep->touch[1].count & 0x80) ^ 0x80);

    // Get battery level and state
    data.misc |= (rep->batteryLevel & 0x7F);

    // Touch 0 x and y
    data.touch0[PS4_JX] = ((uint16_t)(rep->touch[0].xhi_ylo & 0x0F) << 8) | rep->touch[0].x;
    data.touch0[PS4_JY] = ((uint16_t)rep->touch[0].y << 4) | ((rep->touch[0].xhi_ylo & 0xF0) >> 4);
    // Touch 1 x and y
    data.touch1[PS4_JX] = ((uint16_t)(rep->touch[1].xhi_ylo & 0x0F) << 8) | rep->touch[1].x;
    data.touch1[PS4_JY] = ((uint16_t)rep->touch[1].y << 4) | ((rep->touch[1].xhi_ylo & 0xF0) >> 4);

    data.gyro[PS4_SX] = PS4_BALANCE_SENSOR(rep->gyro[PS4_SX]);
    data.gyro[PS4_SY] = PS4_BALANCE_SENSOR(rep->gyro[PS4_SY]);
    data.gyro[PS4_SZ] = PS4_BALANCE_SENSOR(rep->gyro[PS4_SZ]);
    data.accel[PS4_SX] = PS4_BALANCE_SENSOR(rep->accel[PS4_SX]);
    data.accel[PS4_SY] = PS4_BALANCE_SENSOR(rep->accel[PS4_SY]);
    data.accel[PS4_SZ] = PS4_BALANCE_SENSOR(rep->accel[PS4_SZ]);

    return data;
}

static void HIDParsePS4ControllerInputReport(PS4Controller* con, PS4CInputReport* currRep, InputPortIndex portIndex)
{
    UNREF_PARAM(con);
    Gamepad&       gpad = gGamepads[portIndex];
    PS4ParsedInput curr = ParseFullInputReport(currRep);

    GamepadUpdateLastState(portIndex);

    // Buttons
    gpad.mButtons[GPAD_UP - GPAD_BTN_FIRST] = (bool)(curr.buttons & PS4_DP_UP);
    gpad.mButtons[GPAD_DOWN - GPAD_BTN_FIRST] = (bool)(curr.buttons & PS4_DP_DN);
    gpad.mButtons[GPAD_LEFT - GPAD_BTN_FIRST] = (bool)(curr.buttons & PS4_DP_LT);
    gpad.mButtons[GPAD_RIGHT - GPAD_BTN_FIRST] = (bool)(curr.buttons & PS4_DP_RT);
    gpad.mButtons[GPAD_A - GPAD_BTN_FIRST] = (bool)(curr.buttons & PS4_CROSS);
    gpad.mButtons[GPAD_B - GPAD_BTN_FIRST] = (bool)(curr.buttons & PS4_CIRCL);
    gpad.mButtons[GPAD_X - GPAD_BTN_FIRST] = (bool)(curr.buttons & PS4_SQUAR);
    gpad.mButtons[GPAD_Y - GPAD_BTN_FIRST] = (bool)(curr.buttons & PS4_TRIAN);
    gpad.mButtons[GPAD_L1 - GPAD_BTN_FIRST] = (bool)(curr.buttons & PS4_L_SHD);
    gpad.mButtons[GPAD_R1 - GPAD_BTN_FIRST] = (bool)(curr.buttons & PS4_R_SHD);
    gpad.mButtons[GPAD_L3 - GPAD_BTN_FIRST] = (bool)(curr.buttons & PS4_L_STK);
    gpad.mButtons[GPAD_R3 - GPAD_BTN_FIRST] = (bool)(curr.buttons & PS4_R_STK);
    gpad.mButtons[GPAD_START - GPAD_BTN_FIRST] = (bool)(curr.buttons & PS4_MENU_);
    gpad.mButtons[GPAD_BACK - GPAD_BTN_FIRST] = (bool)(curr.touch & PS4_TCHBT);

    // Left Joystick
    gpad.mAxis[GPAD_LX - GPAD_AXIS_FIRST] = curr.leftJoystick[PS4_JX] / PS4_SCALING;
    gpad.mAxis[GPAD_LY - GPAD_AXIS_FIRST] = curr.leftJoystick[PS4_JY] / -PS4_SCALING; // Invert Y
    // Right Joystick
    gpad.mAxis[GPAD_RX - GPAD_AXIS_FIRST] = curr.rightJoystick[PS4_JX] / PS4_SCALING;
    gpad.mAxis[GPAD_RY - GPAD_AXIS_FIRST] = curr.rightJoystick[PS4_JY] / -PS4_SCALING; // Invert Y
    // Left Trigger
    gpad.mAxis[GPAD_L2 - GPAD_AXIS_FIRST] = curr.trigger[PS4_TL] / PS4_TRIG_SCALING;
    // Right Trigger
    gpad.mAxis[GPAD_R2 - GPAD_AXIS_FIRST] = curr.trigger[PS4_TR] / PS4_TRIG_SCALING;

    GamepadPostProcess(portIndex);
}

// General

static int HIDUpdatePS4Controller(HIDController* controller)
{
    PS4Controller* con = (PS4Controller*)controller->mData;
    hid_device*    dev = (hid_device*)controller->pDevice;

    uint8_t data[PS4_BT_INPUT_REPORT_SIZE];
    int32_t size;

    for (; (size = hid_read_timeout(dev, data, PS4_BT_INPUT_REPORT_SIZE, 0)) > 0;)
    {
        if (*data == PS4_BASIC_INPUT_REPORT_ID)
        {
            ASSERT(size <= PS4_USB_INPUT_REPORT_SIZE);
            HIDParsePS4ControllerInputReport(con, (PS4CInputReport*)(data + 1), controller->mPortIndex); //-V1032
        }
        else
        {
            ASSERT(*data >= PS4_EXT_INPUT_REPORT_ID_FIRST && *data <= PS4_EXT_INPUT_REPORT_ID_LAST);
            ASSERT(size <= PS4_BT_INPUT_REPORT_SIZE);
            HIDParsePS4ControllerInputReport(con, (PS4CInputReport*)(data + 3), controller->mPortIndex);
        }
    }

    // Controller should be disconnected if this is true
    if (size < 0)
    {
        return -1;
    }

    return 0;
}

static void HIDClosePS4Controller(HIDController* controller)
{
    hid_close((hid_device*)controller->pDevice);
    controller->pDevice = NULL;
}

int HIDOpenPS4Controller(HIDDeviceInfo* devInfo, HIDController* controller)
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
    controller->Update = HIDUpdatePS4Controller;
    controller->Close = HIDClosePS4Controller;

    controller->SetLights = HIDSetLightsPS4Controller;
    controller->DoRumble = HIDDoRumblePS4Controller;

    // initialized the opaque buffer
    PS4Controller* con = (PS4Controller*)controller->mData;
    memset(con, 0, sizeof *con); //-V1086

    con->official = devInfo->mVendorID == HID_VENDOR_SONY;
    con->dongle = con->official && devInfo->mProductID == HID_PID_DS4_DONGLE;
    con->effectsEnabled = true;

    // test if usb connection
    uint8_t data[PS4_BT_INPUT_REPORT_SIZE];
    memset(data, 0, sizeof data);
    data[0] = PS4_FEATURE_REPORT_DEVICE_ID;
    hid_get_feature_report(pDevice, data, PS4_USB_INPUT_REPORT_SIZE);

    if (con->official && !con->dongle)
    {
        // test if usb connection
        uint8_t usbData[PS4_USB_INPUT_REPORT_SIZE];
        memset(usbData, 0, sizeof usbData);
        usbData[0] = PS4_FEATURE_REPORT_DEVICE_ID;
        int32_t size = hid_get_feature_report(pDevice, usbData, PS4_USB_INPUT_REPORT_SIZE);

        // is usb
        if (size >= 7)
        {
            uint8_t devID[6];

            devID[0] = usbData[6];
            devID[1] = usbData[5];
            devID[2] = usbData[4];
            devID[3] = usbData[3];
            devID[4] = usbData[2];
            devID[5] = usbData[1];

            LOGF(LogLevel::eINFO, "Device ID is: %.2X-%.2X-%.2X-%.2X-%.2X-%.2X", devID[0], devID[1], devID[2], devID[3], devID[4],
                 devID[5]);
        }
        // is bluetooth
        else
        {
            con->bluetooth = 1;
        }
    }

    // default blue ps color
    con->ledColor[PS4_CR] = 0x00u;
    con->ledColor[PS4_CG] = 0x00u;
    con->ledColor[PS4_CB] = 0x40u;
    HIDSendEffectsPS4Controller(controller);

    return 0;
}
