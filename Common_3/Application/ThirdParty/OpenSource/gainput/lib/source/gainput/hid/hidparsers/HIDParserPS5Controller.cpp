
#include "../GainputHIDTypes.h"
#include "../GainputHIDWhitelist.h"
#include "../../../hidapi/hidapi.h"
#include "../../../../../../Utilities/Interfaces/ILog.h"

#include "../../../../include/gainput/gainput.h"
#include "../../../../include/gainput/GainputInputDeltaState.h"

#define PS5_REP_USB_IN        0x01
#define PS5_REP_USB_OUT       0x02
#define PS5_REP_BLUETOOTH_IN  0x31
#define PS5_REP_BLUETOOTH_OUT 0x31

#define PS5_PACKET_SIZE_USB_IN        64
#define PS5_PACKET_SIZE_USB_OUT       63
#define PS5_PACKET_SIZE_BLUETOOTH_IN  78
#define PS5_PACKET_SIZE_BLUETOOTH_OUT 78

#define PS5_FEATURE_REPORT_CALIBRATION   0x05
#define PS5_FEATURE_REPORT_PAIRING_INFO  0x09
#define PS5_FEATURE_REPORT_FIRMWARE_INFO 0x20

#define PS5_FEATURE_REPORT_SIZE_CALIBRATION   71
#define PS5_FEATURE_REPORT_SIZE_PAIRING_INFO  20
#define PS5_FEATURE_REPORT_SIZE_FIRMWARE_INFO 64

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

#define EFFECTS_PRE_RUMBLE (1 << 0)
#define EFFECTS_LED_RESET  (1 << 1)
#define EFFECTS_LED        (1 << 2)
#define EFFECTS_LIGHTS_PAD (1 << 3)
#define EFFECTS_LIGHTS_MIC (1 << 4)

#define MIC_OFF   0
#define MIC_SOLID 1
#define MIC_PULSE 2
#define MIC_LAST  MIC_PULSE

struct PS5ParsedInput
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
    // 15 : Mic Button
    uint16_t buttons;
    //  0 : TouchPad Button
    //  1 : Touch 0
    //  2 : Touch 1
    uint8_t touch;
    // 0-3 : Battery level
    //   4 : Is Wired Bool
    //   5 : Battery Fully Charged
    //   6 : Headphones
    //   7 : USB
    uint8_t misc;
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
    uint8_t playerNum : 4; // 4b
    uint8_t bluetooth : 1; // 1b
    uint8_t btLightPending : 1; // 1b
    uint8_t useStandardColors : 1; // 1b
    uint8_t useStandardTrackpadLights : 1; // 1b
    uint8_t ledColor[3]; // rgb
    uint8_t ledTouchpad;
    uint8_t ledMicrophone;
    uint8_t rumble[2]; // lr

    PS5ParsedInput last;
};
// if exceeded, increase HID_CONTROLLER_BUFFER_SIZE
COMPILE_ASSERT(sizeof(PS5Controller) <= HID_CONTROLLER_BUFFER_SIZE);


struct PS5CInputReport
{
    // ensures sensors sit on word boundaries
    // if the packet is over usb this would be the id, but over bluetooth has additional data
    uint8_t  __pad0;

    uint8_t  leftJoystick[2];
    uint8_t  rightJoystick[2];
    uint8_t  trigger[2];
    uint8_t  reportNum;
    uint8_t  buttons[4];

    uint8_t  __pad1[4]; // some form of timestamp or sequence data?

    int16_t  gyro[3]; // is signed
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

    uint8_t  __pad2[8];
    uint8_t  __pad3[4]; // some form of timestamp or sequence data?

    uint8_t  batteryLevel;
    uint8_t  wiresConnected; // 0x1 is headphones, 0x8 is usb

    uint8_t  __pad4[9];

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
    uint8_t  __pad0;

    uint8_t  leftJoystick[2];
    uint8_t  rightJoystick[2];
    uint8_t  buttons[3];
    uint8_t  trigger[2];
};

double DEADZONE = 0.05;
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
#define TOUCHY_SCALING (float)(1079.0f)
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
    return devInfo->type == ctPS5;
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

static void HIDSendEffectsPS5Controller(HIDController * controller, int effectsToSend)
{
    PS5Controller* con = (PS5Controller *)controller->data;

    uint8_t data[PS5_PACKET_SIZE_BLUETOOTH_OUT];
    memset(data, 0, sizeof data);
    int size = 0;
    int offset = 0;

    PS5CEffectsState* effects = PrepEffectsPacket(con, data, &size, &offset);



    // wait on light changes until after bt has finished its sequence
    if (con->btLightPending &&
        (effectsToSend & (EFFECTS_LED | EFFECTS_LIGHTS_PAD)))
        return;

    // Present rumble state must always be part of the packet
    uint8_t doRumble = con->rumble[tL] || con->rumble[tR];
    // if 1
    //   enable rumble, disable rumble audio
    // if 0
    //   disable rumble, enable rumble audio
    effects->flagsHapticsEnable |= doRumble << 0;
    effects->flagsHapticsEnable |= doRumble << 1;

    effects->rumbleLeft = con->rumble[tL];
    effects->rumbleRight = con->rumble[tR];

    if (effectsToSend & EFFECTS_PRE_RUMBLE)
    {
        // disable rumble audio
        effects->flagsHapticsEnable |= 1 << 1;
    }
    if (effectsToSend & EFFECTS_LED_RESET)
    {
        // flag for reset
        effects->flagsLedsEnable |= 1 << 3;
    }
    if (effectsToSend & EFFECTS_LED)
    {
        // enable setting color
        effects->flagsLedsEnable |= 1 << 2;

        effects->ledR = con->ledColor[cR];
        effects->ledG = con->ledColor[cG];
        effects->ledB = con->ledColor[cB];
    }
    if (effectsToSend & EFFECTS_LIGHTS_PAD)
    {
        // enable setting touchpad light
        effects->flagsLedsEnable |= 1 << 4;

        effects->lightsTrackpad = con->ledTouchpad;
    }
    if (effectsToSend & EFFECTS_LIGHTS_MIC)
    {
        // enable setting mic light
        effects->flagsLedsEnable |= 1 << 0;

        effects->ledMicrophone = con->ledMicrophone;
    }



    if (con->bluetooth)
    {
        uint8_t hdr = 0xA2;
        uint32_t crc = crc32(0, &hdr, 1);
        crc = crc32(crc, data, (uint32_t)(size - sizeof crc));
        memcpy(data + (size - sizeof crc), &crc, sizeof crc);
    }

    hid_write(controller->hidDev, data, size);
}

// Lights

static void HIDUpdateBTLightStatusPS5Controller(HIDController * controller, uint32_t hidDevTime)
{
    const uint32_t btLightMaxBlinkPeriod = 10200000;
    if (hidDevTime >= btLightMaxBlinkPeriod)
    {
        PS5Controller* con = (PS5Controller *)controller->data;
        con->btLightPending = 0;

        HIDSendEffectsPS5Controller(controller, EFFECTS_LED_RESET);
        HIDSendEffectsPS5Controller(controller, EFFECTS_LED | EFFECTS_LIGHTS_PAD);
    }
}

static void HIDSetLightsPS5Controller(HIDController * controller, uint8_t r, uint8_t g, uint8_t b)
{
    PS5Controller* con = (PS5Controller *)controller->data;

    con->ledColor[cR] = r;
    con->ledColor[cG] = g;
    con->ledColor[cB] = b;

    con->useStandardColors = 0;

    HIDSendEffectsPS5Controller(controller, EFFECTS_LED);
}

// Not used in Gainput
static void HIDSetStandardPlayerLightsPS5Controller(HIDController * controller)
{
    PS5Controller* con = (PS5Controller *)controller->data;

    static const uint8_t standardColors[] =
    {
        0x00, 0x00, 0x40,
        0x40, 0x00, 0x00,
        0x00, 0x40, 0x00,
        0x20, 0x00, 0x20,
        0x20, 0x10, 0x00,
        0x00, 0x10, 0x10,
        0x10, 0x10, 0x10
    };
    static const uint8_t maxColors = (sizeof standardColors) / 3;

    int offset = (con->playerNum % maxColors) * 3;

    con->ledColor[cR] = standardColors[offset + 0];
    con->ledColor[cG] = standardColors[offset + 1];
    con->ledColor[cB] = standardColors[offset + 2];

    con->useStandardColors = 1;

    HIDSendEffectsPS5Controller(controller, EFFECTS_LED);
}
//
//// Not used in Gainput
#if 0
static void HIDSetTouchpadLightsPS5Controller(HIDController * controller, uint8_t lightMask, bool doFade)
{
    PS5Controller* con = (PS5Controller *)controller->data;

    con->ledTouchpad = lightMask & 0x1F;
    if (!doFade)
        con->ledTouchpad |= 0x20;

    con->useStandardTrackpadLights = 0;

    HIDSendEffectsPS5Controller(controller, EFFECTS_LIGHTS_PAD);
}
#endif

// Not used in Gainput
static void HIDSetStandardPlayerTouchpadLightsPS5Controller(HIDController * controller, bool doFade)
{
    PS5Controller* con = (PS5Controller *)controller->data;

    static const uint8_t lightMasks[] =
    {
        0x04,
        0x0A,
        0x15,
        0x1B
    };

    int offset = con->playerNum % sizeof lightMasks;

    con->ledTouchpad = lightMasks[offset];
    if (!doFade)
        con->ledTouchpad |= 0x20;

    con->useStandardTrackpadLights = 1;

    HIDSendEffectsPS5Controller(controller, EFFECTS_LIGHTS_PAD);
}
//
//// Not used in Gainput
#if 0
static void HIDSetMicrophoneLightPatternPS5Controller(HIDController * controller, int mode)
{
    PS5Controller* con = (PS5Controller *)controller->data;

    uint8_t clampedMode = mode;
    if (clampedMode > MIC_LAST)
        clampedMode = MIC_OFF;

    con->ledMicrophone = clampedMode;

    HIDSendEffectsPS5Controller(controller, EFFECTS_LIGHTS_MIC);
}
#endif
// Haptics

static void HIDDoRumblePS5Controller(HIDController * controller, uint16_t left, uint16_t right)
{
    PS5Controller* con = (PS5Controller *)controller->data;

    HIDSendEffectsPS5Controller(controller, EFFECTS_PRE_RUMBLE);

    con->rumble[tR] = right >> 8;
    con->rumble[tL] = left >> 8;

    HIDSendEffectsPS5Controller(controller, 0);
}

#if 0
// Not used in Gainput
static void HIDDoRumbleTriggersPS5Controller(HIDController * controller, uint16_t left, uint16_t right)
{
    // Not Set Up
}
#endif
// Misc

static void HIDSetPlayerPS5Controller(HIDController * controller, uint8_t num)
{
    PS5Controller* con = (PS5Controller *)controller->data;

    con->playerNum = num % 0xF;

    if (con->useStandardColors)
        HIDSetStandardPlayerLightsPS5Controller(controller);
    if (con->useStandardTrackpadLights)
        HIDSetStandardPlayerTouchpadLightsPS5Controller(controller, !!(con->ledTouchpad & 0x20));
}



// Input

static PS5ParsedInput ParseFullInputReport(PS5CInputReport* rep)
{
    PS5ParsedInput data;
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
    data.sensorTime = rep->sensorTime;

    return data;
}

static PS5ParsedInput ParseSimpleInputReport(PS5CInputReportSimple * rep)
{
    PS5ParsedInput data;
    memset(&data, 0, sizeof data);

    uint8_t dpadState = rep->buttons[0] & 0xF;

    // Store dpad values
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

    data.leftJoystick[jX] = JOY_TO_INT(rep->leftJoystick[jX]);
    data.leftJoystick[jY] = JOY_TO_INT(rep->leftJoystick[jY]);
    data.rightJoystick[jX] = JOY_TO_INT(rep->rightJoystick[jX]);
    data.rightJoystick[jY] = JOY_TO_INT(rep->rightJoystick[jY]);

    data.trigger[tL] = rep->trigger[tL];
    data.trigger[tR] = rep->trigger[tR];

    return data;
}


static void HIDParsePS5CInputReport(PS5Controller* con,
    PS5CInputReport* currRep, gainput::InputDeltaState * state)
{
    PS5ParsedInput curr = ParseFullInputReport(currRep);


    // Axes

    // Left Joystick
    bool leftActive = curr.leftJoystick[jX] || con->last.leftJoystick[jX] ||
        curr.leftJoystick[jY] || con->last.leftJoystick[jY];
    if (leftActive)
    {
        // Left Joystick X
        state->AddChange(CONTROLLER_ID, gainput::PadButtonLeftStickX,
            con->last.leftJoystick[jX] / SCALING, curr.leftJoystick[jX] / SCALING);
        // Left Joystick Y
        state->AddChange(CONTROLLER_ID, gainput::PadButtonLeftStickY,
            con->last.leftJoystick[jY] / -SCALING, curr.leftJoystick[jY] / -SCALING);
    }

    // Right Joystick
    bool rightActive = curr.rightJoystick[jX] || con->last.rightJoystick[jX] ||
        curr.rightJoystick[jY] || con->last.rightJoystick[jY];
    if (rightActive)
    {
        // Right Joystick X
        state->AddChange(CONTROLLER_ID, gainput::PadButtonRightStickX,
            con->last.rightJoystick[jX] / SCALING, curr.rightJoystick[jX] / SCALING);
        // Right Joystick Y
        state->AddChange(CONTROLLER_ID, gainput::PadButtonRightStickY,
            con->last.rightJoystick[jY] / -SCALING, curr.rightJoystick[jY] / -SCALING);
    }

    // Left Trigger
    if (curr.trigger[tL] || con->last.trigger[tL])
        state->AddChange(CONTROLLER_ID, gainput::PadButtonAxis4,
            con->last.trigger[tL] / TRIG_SCALING, curr.trigger[tL] / TRIG_SCALING);

    // Right Trigger
    if (curr.trigger[tR] || con->last.trigger[tR])
        state->AddChange(CONTROLLER_ID, gainput::PadButtonAxis5,
            con->last.trigger[tR] / TRIG_SCALING, curr.trigger[tR] / TRIG_SCALING);


    // Buttons

    uint16_t buttonsChanged = curr.buttons ^ con->last.buttons;
    if (buttonsChanged)
    {
        // dpad
        if (buttonsChanged & PS5_DP_UP)
            state->AddChange(CONTROLLER_ID, gainput::PadButtonUp, (bool)(con->last.buttons & PS5_DP_UP), (bool)(curr.buttons & PS5_DP_UP));
        if (buttonsChanged & PS5_DP_RT)
            state->AddChange(CONTROLLER_ID, gainput::PadButtonRight, (bool)(con->last.buttons & PS5_DP_RT), (bool)(curr.buttons & PS5_DP_RT));
        if (buttonsChanged & PS5_DP_DN)
            state->AddChange(CONTROLLER_ID, gainput::PadButtonDown, (bool)(con->last.buttons & PS5_DP_DN), (bool)(curr.buttons & PS5_DP_DN));
        if (buttonsChanged & PS5_DP_LT)
            state->AddChange(CONTROLLER_ID, gainput::PadButtonLeft, (bool)(con->last.buttons & PS5_DP_LT), (bool)(curr.buttons & PS5_DP_LT));
        // shape buttons
        if (buttonsChanged & PS5_SQUAR)
            state->AddChange(CONTROLLER_ID, gainput::PadButtonX, (bool)(con->last.buttons & PS5_SQUAR), (bool)(curr.buttons & PS5_SQUAR));
        if (buttonsChanged & PS5_CROSS)
            state->AddChange(CONTROLLER_ID, gainput::PadButtonA, (bool)(con->last.buttons & PS5_CROSS), (bool)(curr.buttons & PS5_CROSS));
        if (buttonsChanged & PS5_CIRCL)
            state->AddChange(CONTROLLER_ID, gainput::PadButtonB, (bool)(con->last.buttons & PS5_CIRCL), (bool)(curr.buttons & PS5_CIRCL));
        if (buttonsChanged & PS5_TRIAN)
            state->AddChange(CONTROLLER_ID, gainput::PadButtonY, (bool)(con->last.buttons & PS5_TRIAN), (bool)(curr.buttons & PS5_TRIAN));
        // Shoulders
        if (buttonsChanged & PS5_L_SHD)
            state->AddChange(CONTROLLER_ID, gainput::PadButtonL1, (bool)(con->last.buttons & PS5_L_SHD), (bool)(curr.buttons & PS5_L_SHD));
        if (buttonsChanged & PS5_R_SHD)
            state->AddChange(CONTROLLER_ID, gainput::PadButtonR1, (bool)(con->last.buttons & PS5_R_SHD), (bool)(curr.buttons & PS5_R_SHD));
        // Share
        if (buttonsChanged & PS5_SHARE)
            state->AddChange(CONTROLLER_ID, gainput::PadButtonSelect, (bool)(con->last.buttons & PS5_SHARE), (bool)(curr.buttons & PS5_SHARE));
        // Menu
        if (buttonsChanged & PS5_MENU_)
            state->AddChange(CONTROLLER_ID, gainput::PadButtonStart, (bool)(con->last.buttons & PS5_MENU_), (bool)(curr.buttons & PS5_MENU_));
        // Sticks
        if (buttonsChanged & PS5_L_STK)
            state->AddChange(CONTROLLER_ID, gainput::PadButtonL3, (bool)(con->last.buttons & PS5_L_STK), (bool)(curr.buttons & PS5_L_STK));
        if (buttonsChanged & PS5_R_STK)
            state->AddChange(CONTROLLER_ID, gainput::PadButtonR3, (bool)(con->last.buttons & PS5_R_STK), (bool)(curr.buttons & PS5_R_STK));
        // PS Button
        if (buttonsChanged & PS5_HOME_)
            state->AddChange(CONTROLLER_ID, gainput::PadButtonHome, (bool)(con->last.buttons & PS5_HOME_), (bool)(curr.buttons & PS5_HOME_));
        // Mic Button
        if (buttonsChanged & PS5_MICPH)
            state->AddChange(CONTROLLER_ID, gainput::PadButton18, (bool)(con->last.buttons & PS5_MICPH), (bool)(curr.buttons & PS5_MICPH));
    }

    uint8_t touchDiff = con->last.touch ^ curr.touch;
    // Touch Pad Button
    if (touchDiff & PS5_TCHBT)
        state->AddChange(CONTROLLER_ID, gainput::PadButton17, (bool)(con->last.touch & PS5_TCHBT), (bool)(curr.touch & PS5_TCHBT));


    // Touch

    // Touch 0
    if (touchDiff & PS5_TCH_0)
        state->AddChange(CONTROLLER_ID, gainput::PadButton19, (bool)(con->last.touch & PS5_TCH_0), (bool)(curr.touch & PS5_TCH_0));
    // Touch 0 Movement
    else if (curr.touch & PS5_TCH_0)
    {
        state->AddChange(CONTROLLER_ID, gainput::PadButtonAxis6, con->last.touch0[jX] / TOUCHX_SCALING, curr.touch0[jX] / TOUCHX_SCALING);
        state->AddChange(CONTROLLER_ID, gainput::PadButtonAxis6, con->last.touch0[jY] / TOUCHY_SCALING, curr.touch0[jY] / TOUCHY_SCALING);
    }
    // Touch 1
    if (touchDiff & PS5_TCH_1)
        state->AddChange(CONTROLLER_ID, gainput::PadButton20, (bool)(con->last.touch & PS5_TCH_1), (bool)(curr.touch & PS5_TCH_1));
    // Touch 1 Movement
    else if (curr.touch & PS5_TCH_1)
    {
        state->AddChange(CONTROLLER_ID, gainput::PadButtonAxis7, con->last.touch1[jX] / TOUCHX_SCALING, curr.touch1[jX] / TOUCHX_SCALING);
        state->AddChange(CONTROLLER_ID, gainput::PadButtonAxis7, con->last.touch1[jY] / TOUCHY_SCALING, curr.touch1[jY] / TOUCHY_SCALING);
    }


    // Sensors

    state->AddChange(CONTROLLER_ID, gainput::PadButtonGyroscopeX, con->last.gyro[sX] / SENSOR_SCALING, curr.gyro[sX] / SENSOR_SCALING);
    state->AddChange(CONTROLLER_ID, gainput::PadButtonGyroscopeY, con->last.gyro[sY] / SENSOR_SCALING, curr.gyro[sY] / SENSOR_SCALING);
    state->AddChange(CONTROLLER_ID, gainput::PadButtonGyroscopeZ, con->last.gyro[sZ] / SENSOR_SCALING, curr.gyro[sZ] / SENSOR_SCALING);
    state->AddChange(CONTROLLER_ID, gainput::PadButtonAccelerationX, con->last.accel[sX] / SENSOR_SCALING, curr.accel[sX] / SENSOR_SCALING);
    state->AddChange(CONTROLLER_ID, gainput::PadButtonAccelerationY, con->last.accel[sY] / SENSOR_SCALING, curr.accel[sY] / SENSOR_SCALING);
    state->AddChange(CONTROLLER_ID, gainput::PadButtonAccelerationZ, con->last.accel[sZ] / SENSOR_SCALING, curr.accel[sZ] / SENSOR_SCALING);


    // Store Data
    con->last = curr;
}

static void HIDParsePS5ControllerSimpleReport(PS5Controller* con,
    PS5CInputReportSimple* currRep, gainput::InputDeltaState * state)
{
    PS5ParsedInput curr = ParseSimpleInputReport(currRep);


    // Axes

    // Left Joystick
    bool leftActive = curr.leftJoystick[jX] || con->last.leftJoystick[jX] ||
        curr.leftJoystick[jY] || con->last.leftJoystick[jY];
    if (leftActive)
    {
        // Left Joystick X
        state->AddChange(CONTROLLER_ID, gainput::PadButtonLeftStickX,
            con->last.leftJoystick[jX] / SCALING, curr.leftJoystick[jX] / SCALING);
        // Left Joystick Y
        state->AddChange(CONTROLLER_ID, gainput::PadButtonLeftStickY,
            con->last.leftJoystick[jY] / -SCALING, curr.leftJoystick[jY] / -SCALING);
    }

    // Right Joystick
    bool rightActive = curr.rightJoystick[jX] || con->last.rightJoystick[jX] ||
        curr.rightJoystick[jY] || con->last.rightJoystick[jY];
    if (rightActive)
    {
        // Right Joystick X
        state->AddChange(CONTROLLER_ID, gainput::PadButtonRightStickX,
            con->last.rightJoystick[jX] / SCALING, curr.rightJoystick[jX] / SCALING);
        // Right Joystick Y
        state->AddChange(CONTROLLER_ID, gainput::PadButtonRightStickY,
            con->last.rightJoystick[jY] / -SCALING, curr.rightJoystick[jY] / -SCALING);
    }

    // Left Trigger
    if (curr.trigger[tL] || con->last.trigger[tL])
        state->AddChange(CONTROLLER_ID, gainput::PadButtonAxis4,
            con->last.trigger[tL] / TRIG_SCALING, curr.trigger[tL] / TRIG_SCALING);

    // Right Trigger
    if (curr.trigger[tR] || con->last.trigger[tR])
        state->AddChange(CONTROLLER_ID, gainput::PadButtonAxis5,
            con->last.trigger[tR] / TRIG_SCALING, curr.trigger[tR] / TRIG_SCALING);


    // Buttons

    uint16_t buttonsChanged = curr.buttons ^ con->last.buttons;
    if (buttonsChanged)
    {
        // dpad
        if (buttonsChanged & PS5_DP_UP)
            state->AddChange(CONTROLLER_ID, gainput::PadButtonUp, (bool)(con->last.buttons & PS5_DP_UP), (bool)(curr.buttons & PS5_DP_UP));
        if (buttonsChanged & PS5_DP_RT)
            state->AddChange(CONTROLLER_ID, gainput::PadButtonRight, (bool)(con->last.buttons & PS5_DP_RT), (bool)(curr.buttons & PS5_DP_RT));
        if (buttonsChanged & PS5_DP_DN)
            state->AddChange(CONTROLLER_ID, gainput::PadButtonDown, (bool)(con->last.buttons & PS5_DP_DN), (bool)(curr.buttons & PS5_DP_DN));
        if (buttonsChanged & PS5_DP_LT)
            state->AddChange(CONTROLLER_ID, gainput::PadButtonLeft, (bool)(con->last.buttons & PS5_DP_LT), (bool)(curr.buttons & PS5_DP_LT));
        // shape buttons
        if (buttonsChanged & PS5_SQUAR)
            state->AddChange(CONTROLLER_ID, gainput::PadButtonX, (bool)(con->last.buttons & PS5_SQUAR), (bool)(curr.buttons & PS5_SQUAR));
        if (buttonsChanged & PS5_CROSS)
            state->AddChange(CONTROLLER_ID, gainput::PadButtonA, (bool)(con->last.buttons & PS5_CROSS), (bool)(curr.buttons & PS5_CROSS));
        if (buttonsChanged & PS5_CIRCL)
            state->AddChange(CONTROLLER_ID, gainput::PadButtonB, (bool)(con->last.buttons & PS5_CIRCL), (bool)(curr.buttons & PS5_CIRCL));
        if (buttonsChanged & PS5_TRIAN)
            state->AddChange(CONTROLLER_ID, gainput::PadButtonY, (bool)(con->last.buttons & PS5_TRIAN), (bool)(curr.buttons & PS5_TRIAN));
        // Shoulders
        if (buttonsChanged & PS5_L_SHD)
            state->AddChange(CONTROLLER_ID, gainput::PadButtonL1, (bool)(con->last.buttons & PS5_L_SHD), (bool)(curr.buttons & PS5_L_SHD));
        if (buttonsChanged & PS5_R_SHD)
            state->AddChange(CONTROLLER_ID, gainput::PadButtonR1, (bool)(con->last.buttons & PS5_R_SHD), (bool)(curr.buttons & PS5_R_SHD));
        // Share
        if (buttonsChanged & PS5_SHARE)
            state->AddChange(CONTROLLER_ID, gainput::PadButtonSelect, (bool)(con->last.buttons & PS5_SHARE), (bool)(curr.buttons & PS5_SHARE));
        // Menu
        if (buttonsChanged & PS5_MENU_)
            state->AddChange(CONTROLLER_ID, gainput::PadButtonStart, (bool)(con->last.buttons & PS5_MENU_), (bool)(curr.buttons & PS5_MENU_));
        // Sticks
        if (buttonsChanged & PS5_L_STK)
            state->AddChange(CONTROLLER_ID, gainput::PadButtonL3, (bool)(con->last.buttons & PS5_L_STK), (bool)(curr.buttons & PS5_L_STK));
        if (buttonsChanged & PS5_R_STK)
            state->AddChange(CONTROLLER_ID, gainput::PadButtonR3, (bool)(con->last.buttons & PS5_R_STK), (bool)(curr.buttons & PS5_R_STK));
        // PS Button
        if (buttonsChanged & PS5_HOME_)
            state->AddChange(CONTROLLER_ID, gainput::PadButtonHome, (bool)(con->last.buttons & PS5_HOME_), (bool)(curr.buttons & PS5_HOME_));
        // Mic Button
        if (buttonsChanged & PS5_MICPH)
            state->AddChange(CONTROLLER_ID, gainput::PadButton18, (bool)(con->last.buttons & PS5_MICPH), (bool)(curr.buttons & PS5_MICPH));
    }

    uint8_t touchDiff = con->last.touch ^ curr.touch;
    // Touch Pad Button
    if (touchDiff & PS5_TCHBT)
        state->AddChange(CONTROLLER_ID, gainput::PadButton17, (bool)(con->last.touch & PS5_TCHBT), (bool)(curr.touch & PS5_TCHBT));


    // Store Data
    con->last.leftJoystick[jX] = curr.leftJoystick[jX];
    con->last.leftJoystick[jY] = curr.leftJoystick[jY];
    con->last.rightJoystick[jX] = curr.rightJoystick[jX];
    con->last.rightJoystick[jY] = curr.rightJoystick[jY];
    con->last.trigger[tL] = curr.trigger[tL];
    con->last.trigger[tR] = curr.trigger[tR];
    con->last.buttons = curr.buttons;
    con->last.touch = (con->last.touch & 0xFE) | (curr.touch & 0x01);
}



// General

static int HIDUpdatePS5Controller(HIDController* controller, gainput::InputDeltaState * state)
{
    PS5Controller* con = (PS5Controller*)controller->data;
    hid_device* dev = (hid_device*)controller->hidDev;

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
                // Handle the occasional simplified packet
                HIDParsePS5ControllerSimpleReport(con, (PS5CInputReportSimple*)(data), state);
            else
                // Normal USB input
                HIDParsePS5CInputReport(con, (PS5CInputReport*)(data), state);
        }
        else if (*data == PS5_REP_BLUETOOTH_IN)
        {
            PS5CInputReport* report = (PS5CInputReport*)(data + 1);
            if (con->btLightPending)
                HIDUpdateBTLightStatusPS5Controller(controller, report->sensorTime);

            HIDParsePS5CInputReport(con, report, state);
        }
    }

    if (size < 0)
        // Controller should be disconnected if this is true
        return -1;
    return 0;
}

static void HIDClosePS5Controller(HIDController* controller)
{
    hid_close((hid_device *)controller->hidDev);
    controller->hidDev = NULL;
}

int HIDOpenPS5Controller(HIDDeviceInfo* devInfo, HIDController* controller, uint8_t playerNum)
{
    hid_device * hidDev = hid_open_path(devInfo->logicalSystemPath);
    if (!hidDev)
    {
        LOGF(LogLevel::eWARNING, "HID Open failed on device %s", devInfo->logicalSystemPath);
        LOGF(LogLevel::eWARNING, "   hid err str: %ls", hid_error(hidDev));
        return -1;
    }

    // store the device
    controller->hidDev = hidDev;
    controller->Update = HIDUpdatePS5Controller;
    controller->Close = HIDClosePS5Controller;

    controller->SetPlayer = HIDSetPlayerPS5Controller;
    controller->SetLights = HIDSetLightsPS5Controller;
    controller->DoRumble = HIDDoRumblePS5Controller;

    // initialized the opaque buffer
    PS5Controller* con = (PS5Controller *)&controller->data;
    memset(con, 0, sizeof *con);

    // set a default state to prevent input spam at start
    con->last.leftJoystick[jX] = 0x7F;
    con->last.leftJoystick[jY] = 0x7F;
    con->last.rightJoystick[jX] = 0x7F;
    con->last.rightJoystick[jY] = 0x7F;

    uint8_t data[PS5_PACKET_SIZE_BLUETOOTH_IN];
    // Get the current state
    int32_t size = hid_read_timeout(hidDev, data, PS5_PACKET_SIZE_BLUETOOTH_IN, 16);

    con->playerNum = playerNum;
    con->bluetooth = size != PS5_PACKET_SIZE_USB_IN ? 1 : 0;
    con->btLightPending = con->bluetooth;
    con->useStandardColors = 0;
    con->useStandardTrackpadLights = 0;

    // Pairing info needs to be requested to get full reports over bluetooth
    uint8_t pairingInfo[PS5_FEATURE_REPORT_SIZE_PAIRING_INFO];
    *pairingInfo = PS5_FEATURE_REPORT_PAIRING_INFO;
    uint8_t devID[6];
    memset(devID, 0, sizeof(devID));
    if (hid_get_feature_report(hidDev, pairingInfo, PS5_FEATURE_REPORT_SIZE_PAIRING_INFO) == PS5_FEATURE_REPORT_SIZE_PAIRING_INFO)
    {
        devID[0] = pairingInfo[6];
        devID[1] = pairingInfo[5];
        devID[2] = pairingInfo[4];
        devID[3] = pairingInfo[3];
        devID[4] = pairingInfo[2];
        devID[5] = pairingInfo[1];

        LOGF(LogLevel::eINFO, "Device ID is: %.2X-%.2X-%.2X-%.2X-%.2X-%.2X",
            devID[0], devID[1], devID[2], devID[3], devID[4], devID[5]);
    }

    return 0;
}
