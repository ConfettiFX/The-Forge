
#include "../GainputHIDTypes.h"
#include "../GainputHIDWhitelist.h"
#include "../../../hidapi/hidapi.h"
#include "../../../../../../Utilities/Interfaces/ILog.h"

#include "../../../../include/gainput/gainput.h"
#include "../../../../include/gainput/GainputInputDeltaState.h"

#define PS4_USB_INPUT_REPORT_SIZE        64
#define PS4_BT_INPUT_REPORT_SIZE         78

#define PS4_BASIC_INPUT_REPORT_ID      0x01
#define PS4_EXT_INPUT_REPORT_ID_FIRST  0x11
#define PS4_EXT_INPUT_REPORT_ID_LAST   0x19
#define PS4_REPORT_DISCONNECT          0xE2

#define PS4_USB_EFFECTS_ID             0x05
#define PS4_BT_EFFECTS_ID              0x11

#define PS4_FEATURE_REPORT_DEVICE_ID   0x12

// Joystick
#define jX 0
#define jY 1

// Sensor
#define sX 0
#define sY 1
#define sZ 2

// Triggers/Rumble
#define tL 0
#define tR 1

// Color
#define cR 0
#define cG 1
#define cB 2

struct PS4ParsedInput
{
    // stored after being converted to an int and having the deadzone
    //   subtracted out
    int8_t  leftJoystick[2];
    int8_t  rightJoystick[2];
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
    uint8_t touch;
    // 0-3 : Battery level
    //   4 : Is Wired Bool
    //   5 : Headphones
    //   6 : Microphone
    uint8_t misc;
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
    uint8_t playerNum : 4; // 4b
    uint8_t bluetooth : 1; // 1b
    uint8_t official : 1; // 1b
    uint8_t dongle : 1; // 1b
    uint8_t effectsEnabled : 1; // 1b

    uint8_t rumble[2]; // high freq
    uint8_t ledColor[3];

    PS4ParsedInput last;
};
// if exceeded, increase HID_CONTROLLER_BUFFER_SIZE
COMPILE_ASSERT(sizeof(PS4Controller) <= HID_CONTROLLER_BUFFER_SIZE);

struct PS4CInputReport
{
    // if the packet is over usb this would be the id, but over bluetooth has additional data
    // needs to be commented out for proper alignment
    //uint8_t __pad0;

    uint8_t leftJoystick[2];
    uint8_t rightJoystick[2];
    uint8_t buttons[3];
    uint8_t trigger[2];

    uint8_t __pad1[3];

    int16_t gyro[3]; // is signed
    int16_t accel[3]; // is signed

    uint8_t __pad2[5];

    // 0-3 appears to be power level
    // 4 is usb
    // 5 seems to be if headphones are plugged in
    // 6 seems to be if the headset supports a mic
    // 7 blind guess but it probably is true if a peripheral is plugged into the EXT port
    uint8_t  batteryLevel;

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

static double DEADZONE = 0.05;
#define DEADZONE_OFFSET (uint8_t)(0x7F * DEADZONE)
#define UPPER_OFFSET (0x7F + DEADZONE_OFFSET)
#define LOWER_OFFSET (0x7F - DEADZONE_OFFSET)
#define CENTERING (0x7F)
// +1 to deal with the offset in negative values, plus positive values are
//   shifted by one to align with negative values
// NOTE: with a hard coded precision of 0.15 this maps cleanly, but other
//   values may not
#define SCALING (float)((0xFF - (CENTERING + 1 + DEADZONE_OFFSET)))
#define TRIG_SCALING (float)(0xFF)
#define TOUCHX_SCALING (float)(1919.0f)
#define TOUCHY_SCALING (float)(941.0f)
#define SENSOR_SCALING (float)(0x7FFF)
// having positive map to 128 >= bits and negative only to 127 leads to a
//   precision mismatch. Additionally, 1 / 109 was not quite reaching 1,
//   leaving .999994 due to floating point error
#define JOY_TO_INT(v) ( \
    v > UPPER_OFFSET ? \
        (int8_t)(v - (CENTERING + 1 + DEADZONE_OFFSET)) : \
    v < LOWER_OFFSET ? \
        (int8_t)(v - (CENTERING - DEADZONE_OFFSET)) : \
        0)
#define BALANCE_SENSOR(v) (v < 0 ? v + 1 : v)

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
    return devInfo->type == ctPS4;
}


// Effects

static uint32_t crc32(uint32_t crc, const void *data, int len)
{
    // TODO: use LUT
    for (int i = 0; i < len; ++i)
    {
        uint32_t chunk = (uint8_t)crc ^ ((const uint8_t*)data)[i];
        for (int j = 0; j < 8; ++j)
            chunk = (chunk & 1 ? 0 : (uint32_t)0xEDB88320L) ^ chunk >> 1;
        crc = (chunk ^ (uint32_t)0xFF000000L) ^ crc >> 8;
    }
    return crc;
}

static void HIDSendEffectsPS4Controller(HIDController * controller)
{
    PS4Controller* con = (PS4Controller *)controller->data;

    if (!con->effectsEnabled)
        return;

    uint8_t data[PS4_BT_INPUT_REPORT_SIZE];
    memset(data, 0, sizeof data);
    int32_t size = 0;
    int offset = 0;

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

    effects->rumbleRight = con->rumble[tR];
    effects->rumbleLeft = con->rumble[tL];
    effects->ledR = con->ledColor[cR];
    effects->ledG = con->ledColor[cG];
    effects->ledB = con->ledColor[cB];

    if (con->bluetooth)
    {
        uint8_t hdr = 0xA2;
        uint32_t crc = crc32(0, &hdr, 1);
        crc = crc32(crc, data, (uint32_t)(size - sizeof crc));
        memcpy(data + (size - sizeof crc), &crc, sizeof crc);
    }

    hid_write(controller->hidDev, data, size);
}


static void HIDSetPlayerPS4Controller(HIDController * controller, uint8_t num)
{
    PS4Controller* con = (PS4Controller *)controller->data;

    con->playerNum = num % 0xF;
}

static void HIDSetLightsPS4Controller(HIDController * controller, uint8_t r, uint8_t g, uint8_t b)
{
    PS4Controller* con = (PS4Controller *)controller->data;

    con->ledColor[cR] = r;
    con->ledColor[cG] = g;
    con->ledColor[cB] = b;

    HIDSendEffectsPS4Controller(controller);
}

static void HIDDoRumblePS4Controller(HIDController * controller, uint16_t left, uint16_t right)
{
    PS4Controller* con = (PS4Controller *)controller->data;

    con->rumble[tR] = right >> 8;
    con->rumble[tL] = left >> 8;

    HIDSendEffectsPS4Controller(controller);
}


// Input

static PS4ParsedInput ParseFullInputReport(PS4CInputReport* rep)
{
    PS4ParsedInput data;
    memset(&data, 0, sizeof data);

    data.leftJoystick[jX] = JOY_TO_INT(rep->leftJoystick[jX]);
    data.leftJoystick[jY] = JOY_TO_INT(rep->leftJoystick[jY]);
    data.rightJoystick[jX] = JOY_TO_INT(rep->rightJoystick[jX]);
    data.rightJoystick[jY] = JOY_TO_INT(rep->rightJoystick[jY]);

    data.trigger[tL] = rep->trigger[tL];
    data.trigger[tR] = rep->trigger[tR];


    uint8_t dpadState = rep->buttons[0] & 0xF;

    // dpad buttons
    switch (dpadState)
    {
    case 0: data.buttons |= 0b0001; break;
    case 1: data.buttons |= 0b0011; break;
    case 2: data.buttons |= 0b0010; break;
    case 3: data.buttons |= 0b0110; break;
    case 4: data.buttons |= 0b0100; break;
    case 5: data.buttons |= 0b1100; break;
    case 6: data.buttons |= 0b1000; break;
    case 7: data.buttons |= 0b1001; break;
    case 8: data.buttons |= 0b0000; break;
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
    data.touch0[jX] = ((uint16_t)(rep->touch[0].xhi_ylo & 0x0F) << 8) | rep->touch[0].x;
    data.touch0[jY] = ((uint16_t)rep->touch[0].y << 4) | ((rep->touch[0].xhi_ylo & 0xF0) >> 4);
    // Touch 1 x and y
    data.touch1[jX] = ((uint16_t)(rep->touch[1].xhi_ylo & 0x0F) << 8) | rep->touch[1].x;
    data.touch1[jY] = ((uint16_t)rep->touch[1].y << 4) | ((rep->touch[1].xhi_ylo & 0xF0) >> 4);


    data.gyro[sX] = BALANCE_SENSOR(rep->gyro[sX]);
    data.gyro[sY] = BALANCE_SENSOR(rep->gyro[sY]);
    data.gyro[sZ] = BALANCE_SENSOR(rep->gyro[sZ]);
    data.accel[sX] = BALANCE_SENSOR(rep->accel[sX]);
    data.accel[sY] = BALANCE_SENSOR(rep->accel[sY]);
    data.accel[sZ] = BALANCE_SENSOR(rep->accel[sZ]);

    return data;
}

static void HIDParsePS4ControllerInputReport(PS4Controller* con,
    PS4CInputReport* currRep, uint32_t manID, gainput::InputDeltaState * state)
{
    PS4ParsedInput curr = ParseFullInputReport(currRep);


    // Axes

    // Left Joystick
    bool leftActive = curr.leftJoystick[jX] || con->last.leftJoystick[jX] ||
        curr.leftJoystick[jY] || con->last.leftJoystick[jY];
    if (leftActive)
    {
        // Left Joystick X
        state->AddChange(manID, gainput::PadButtonLeftStickX,
            con->last.leftJoystick[jX] / SCALING, curr.leftJoystick[jX] / SCALING);
        // Left Joystick Y
        state->AddChange(manID, gainput::PadButtonLeftStickY,
            con->last.leftJoystick[jY] / -SCALING, curr.leftJoystick[jY] / -SCALING);
    }

    // Right Joystick
    bool rightActive = curr.rightJoystick[jX] || con->last.rightJoystick[jX] ||
        curr.rightJoystick[jY] || con->last.rightJoystick[jY];
    if (rightActive)
    {
        // Right Joystick X
        state->AddChange(manID, gainput::PadButtonRightStickX,
            con->last.rightJoystick[jX] / SCALING, curr.rightJoystick[jX] / SCALING);
        // Right Joystick Y
        state->AddChange(manID, gainput::PadButtonRightStickY,
            con->last.rightJoystick[jY] / -SCALING, curr.rightJoystick[jY] / -SCALING);
    }

    // Left Trigger
    if (curr.trigger[tL] || con->last.trigger[tL])
        state->AddChange(manID, gainput::PadButtonAxis4,
            con->last.trigger[tL] / TRIG_SCALING, curr.trigger[tL] / TRIG_SCALING);

    // Right Trigger
    if (curr.trigger[tR] || con->last.trigger[tR])
        state->AddChange(manID, gainput::PadButtonAxis5,
            con->last.trigger[tR] / TRIG_SCALING, curr.trigger[tR] / TRIG_SCALING);


    // Buttons

    uint16_t buttonsChanged = curr.buttons ^ con->last.buttons;

    if (buttonsChanged)
    {
        // dpad
        if (buttonsChanged & PS4_DP_UP)
            state->AddChange(manID, gainput::PadButtonUp, (bool)(con->last.buttons & PS4_DP_UP), (bool)(curr.buttons & PS4_DP_UP));
        if (buttonsChanged & PS4_DP_RT)
            state->AddChange(manID, gainput::PadButtonRight, (bool)(con->last.buttons & PS4_DP_RT), (bool)(curr.buttons & PS4_DP_RT));
        if (buttonsChanged & PS4_DP_DN)
            state->AddChange(manID, gainput::PadButtonDown, (bool)(con->last.buttons & PS4_DP_DN), (bool)(curr.buttons & PS4_DP_DN));
        if (buttonsChanged & PS4_DP_LT)
            state->AddChange(manID, gainput::PadButtonLeft, (bool)(con->last.buttons & PS4_DP_LT), (bool)(curr.buttons & PS4_DP_LT));
        // shape buttons
        if (buttonsChanged & PS4_SQUAR)
            state->AddChange(manID, gainput::PadButtonX, (bool)(con->last.buttons & PS4_SQUAR), (bool)(curr.buttons & PS4_SQUAR));
        if (buttonsChanged & PS4_CROSS)
            state->AddChange(manID, gainput::PadButtonA, (bool)(con->last.buttons & PS4_CROSS), (bool)(curr.buttons & PS4_CROSS));
        if (buttonsChanged & PS4_CIRCL)
            state->AddChange(manID, gainput::PadButtonB, (bool)(con->last.buttons & PS4_CIRCL), (bool)(curr.buttons & PS4_CIRCL));
        if (buttonsChanged & PS4_TRIAN)
            state->AddChange(manID, gainput::PadButtonY, (bool)(con->last.buttons & PS4_TRIAN), (bool)(curr.buttons & PS4_TRIAN));
        // Shoulders
        if (buttonsChanged & PS4_L_SHD)
            state->AddChange(manID, gainput::PadButtonL1, (bool)(con->last.buttons & PS4_L_SHD), (bool)(curr.buttons & PS4_L_SHD));
        if (buttonsChanged & PS4_R_SHD)
            state->AddChange(manID, gainput::PadButtonR1, (bool)(con->last.buttons & PS4_R_SHD), (bool)(curr.buttons & PS4_R_SHD));
        // Share
        if (buttonsChanged & PS4_SHARE)
            state->AddChange(manID, gainput::PadButtonSelect, (bool)(con->last.buttons & PS4_SHARE), (bool)(curr.buttons & PS4_SHARE));
        // Menu
        if (buttonsChanged & PS4_MENU_)
            state->AddChange(manID, gainput::PadButtonStart, (bool)(con->last.buttons & PS4_MENU_), (bool)(curr.buttons & PS4_MENU_));
        // Sticks
        if (buttonsChanged & PS4_L_STK)
            state->AddChange(manID, gainput::PadButtonL3, (bool)(con->last.buttons & PS4_L_STK), (bool)(curr.buttons & PS4_L_STK));
        if (buttonsChanged & PS4_R_STK)
            state->AddChange(manID, gainput::PadButtonR3, (bool)(con->last.buttons & PS4_R_STK), (bool)(curr.buttons & PS4_R_STK));
        // PS Button
        if (buttonsChanged & PS4_HOME_)
            state->AddChange(manID, gainput::PadButtonHome, (bool)(con->last.buttons & PS4_HOME_), (bool)(curr.buttons & PS4_HOME_));
        // Touch Pad Button
        if (buttonsChanged & PS4_TCHBT)
            state->AddChange(manID, gainput::PadButton17, (bool)(con->last.buttons & PS4_TCHBT), (bool)(curr.buttons & PS4_TCHBT));
    }


    // Touch

    uint8_t touchDiff = con->last.touch ^ curr.touch;

    // Touch 0
    if (touchDiff & PS4_TCH_0)
        state->AddChange(manID, gainput::PadButton19, (bool)(con->last.touch & PS4_TCH_0), (bool)(curr.touch & PS4_TCH_0));
    // Touch 0 Movement
    else if (curr.touch & PS4_TCH_0)
    {
        state->AddChange(manID, gainput::PadButtonAxis6, con->last.touch0[jX] / TOUCHX_SCALING, curr.touch0[jX] / TOUCHX_SCALING);
        state->AddChange(manID, gainput::PadButtonAxis6, con->last.touch0[jY] / TOUCHY_SCALING, curr.touch0[jY] / TOUCHY_SCALING);
    }
    // Touch 1
    if (touchDiff & PS4_TCH_1)
        state->AddChange(manID, gainput::PadButton20, (bool)(con->last.touch & PS4_TCH_1), (bool)(curr.touch & PS4_TCH_1));
    // Touch 1 Movement
    else if (curr.touch & PS4_TCH_1)
    {
        state->AddChange(manID, gainput::PadButtonAxis7, con->last.touch1[jX] / TOUCHX_SCALING, curr.touch1[jX] / TOUCHX_SCALING);
        state->AddChange(manID, gainput::PadButtonAxis7, con->last.touch1[jY] / TOUCHY_SCALING, curr.touch1[jY] / TOUCHY_SCALING);
    }


    // Sensors

    state->AddChange(manID, gainput::PadButtonGyroscopeX, con->last.gyro[sX] / SENSOR_SCALING, curr.gyro[sX] / SENSOR_SCALING);
    state->AddChange(manID, gainput::PadButtonGyroscopeY, con->last.gyro[sY] / SENSOR_SCALING, curr.gyro[sY] / SENSOR_SCALING);
    state->AddChange(manID, gainput::PadButtonGyroscopeZ, con->last.gyro[sZ] / SENSOR_SCALING, curr.gyro[sZ] / SENSOR_SCALING);
    state->AddChange(manID, gainput::PadButtonAccelerationX, con->last.accel[sX] / SENSOR_SCALING, curr.accel[sX] / SENSOR_SCALING);
    state->AddChange(manID, gainput::PadButtonAccelerationY, con->last.accel[sY] / SENSOR_SCALING, curr.accel[sY] / SENSOR_SCALING);
    state->AddChange(manID, gainput::PadButtonAccelerationZ, con->last.accel[sZ] / SENSOR_SCALING, curr.accel[sZ] / SENSOR_SCALING);


    // Store Data
    con->last = curr;
}


// General

static int HIDUpdatePS4Controller(HIDController* controller, gainput::InputDeltaState * state)
{
    PS4Controller* con = (PS4Controller*)controller->data;
    hid_device* dev = (hid_device*)controller->hidDev;

    uint8_t data[PS4_BT_INPUT_REPORT_SIZE];
    int32_t size;

    for (; (size = hid_read_timeout(dev, data, PS4_BT_INPUT_REPORT_SIZE, 0)) > 0;)
    {
        if (*data == PS4_BASIC_INPUT_REPORT_ID)
        {
            ASSERT(size <= PS4_USB_INPUT_REPORT_SIZE);
            HIDParsePS4ControllerInputReport(con, (PS4CInputReport*)(data + 1), controller->manID, state);
        }
        else
        {
            ASSERT(*data >= PS4_EXT_INPUT_REPORT_ID_FIRST && *data <= PS4_EXT_INPUT_REPORT_ID_LAST);
            ASSERT(size <= PS4_BT_INPUT_REPORT_SIZE);
            HIDParsePS4ControllerInputReport(con, (PS4CInputReport*)(data + 3), controller->manID, state);
        }
    }

    if (size < 0)
        // Controller should be disconnected if this is true
        return -1;
    return 0;
}

static void HIDClosePS4Controller(HIDController* controller)
{
    hid_close((hid_device *)controller->hidDev);
    controller->hidDev = NULL;
}

int HIDOpenPS4Controller(HIDDeviceInfo* devInfo, HIDController* controller, uint8_t playerNum)
{
    UNREF_PARAM(playerNum); 
    hid_device * hidDev = hid_open_path(devInfo->logicalSystemPath);
    if (!hidDev)
    {
        LOGF(LogLevel::eWARNING, "HID Open failed on device %s", devInfo->logicalSystemPath);
        LOGF(LogLevel::eWARNING, "   hid err str: %ls", hid_error(hidDev));
        return -1;
    }

    // store the device
    controller->hidDev = hidDev;
    controller->Update = HIDUpdatePS4Controller;
    controller->Close = HIDClosePS4Controller;

    controller->SetPlayer = HIDSetPlayerPS4Controller;
    controller->SetLights = HIDSetLightsPS4Controller;
    controller->DoRumble = HIDDoRumblePS4Controller;

    // initialized the opaque buffer
    PS4Controller* con = (PS4Controller *)controller->data;
    memset(con, 0, sizeof *con);

    con->official = devInfo->vendorID == vSony;
    con->dongle = con->official && devInfo->productID == pidSonyDS4Dongle;
    con->effectsEnabled = true;

    // test if usb connection
    uint8_t data[PS4_BT_INPUT_REPORT_SIZE];
    memset(data, 0, sizeof data);
    *data = PS4_FEATURE_REPORT_DEVICE_ID;
    hid_get_feature_report(hidDev, data, PS4_USB_INPUT_REPORT_SIZE);

    if (con->official && !con->dongle)
    {
        // test if usb connection
        memset(data, 0, sizeof data);
        *data = PS4_FEATURE_REPORT_DEVICE_ID;
        int32_t size = hid_get_feature_report(hidDev, data, PS4_USB_INPUT_REPORT_SIZE);

        // is usb
        if (size >= 7)
        {
            uint8_t devID[6];

            devID[0] = data[6];
            devID[1] = data[5];
            devID[2] = data[4];
            devID[3] = data[3];
            devID[4] = data[2];
            devID[5] = data[1];

            LOGF(LogLevel::eINFO, "Device ID is: %.2X-%.2X-%.2X-%.2X-%.2X-%.2X",
                devID[0], devID[1], devID[2], devID[3], devID[4], devID[5]);
        }
        // is bluetooth
        else
        {
            con->bluetooth = 1;
        }
    }
    
    // default blue ps color
    con->ledColor[cR] = 0x00u;
    con->ledColor[cG] = 0x00u;
    con->ledColor[cB] = 0x40u;
    HIDSendEffectsPS4Controller(controller);

    return 0;
}
