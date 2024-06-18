
#include "../GainputHID.h"
#include "../GainputHIDTypes.h"
#include "../GainputHIDWhitelist.h"
#include "../../../hidapi/hidapi.h"
#include "../../../../../../Utilities/Interfaces/ILog.h"

#include "../../../../include/gainput/gainput.h"
#include "../../../../include/gainput/GainputInputDeltaState.h"


//#define VERBOSE_SWITCH_PACKET_LOGGING

// switch controllers pack 3 packets of sensor data 5 ms apart
//   if this is uncommented all 3 packets will be pushed to the input results
#define SENSOR_FREQ_5MS

#define MAX_RETRIES 5
// timeout on waiting for command responses in ms
//   The controllers constantly send input events once set up; the command response
//   will generally be the 2nd to 5th package received after sending the command.
//   This just makes sure we don't wait forever
#define RESPONSE_TIMEOUT 100

#define SW_PACKET_SIZE_OUT       49

#define SW_PACKET_SIZE_USB_IN          64
#define SW_PACKET_SIZE_USB_OUT         64
#define SW_PACKET_SIZE_BLUETOOTH_IN    49
#define SW_PACKET_SIZE_BLUETOOTH_OUT   49
#define SW_PACKET_MAX_SIZE_IN  (SW_PACKET_SIZE_USB_IN > SW_PACKET_SIZE_BLUETOOTH_IN ? SW_PACKET_SIZE_USB_IN : SW_PACKET_SIZE_BLUETOOTH_IN)
#define SW_PACKET_MAX_SIZE_OUT  (SW_PACKET_SIZE_USB_OUT > SW_PACKET_SIZE_BLUETOOTH_OUT ? SW_PACKET_SIZE_USB_OUT : SW_PACKET_SIZE_BLUETOOTH_OUT)

#define SW_PACKET_TYPE_IN_GENERAL_REPLY  0x21
#define SW_PACKET_TYPE_IN_SETUP_ACK      0x81
#define SW_PACKET_TYPE_IN_GENERAL_ACK    0x80
#define SW_PACKET_TYPE_IN_FULL_STATE     0x30
#define SW_PACKET_TYPE_IN_SIMPLE_STATE   0x3F

#define SW_PACKET_TYPE_OUT_SETUP         0x80
#define SW_PACKET_TYPE_OUT_GENERAL       0x01
#define SW_PACKET_TYPE_OUT_RUMBLE_ONLY   0x10

#define SW_CON_TYPE_UNKNOWN              0x00
#define SW_CON_TYPE_JOY_L                0x01
#define SW_CON_TYPE_JOY_R                0x02
#define SW_CON_TYPE_PRO                  0x03

#define SW_SETUP_STATUS                  0x01
#define SW_SETUP_HANDSHAKE               0x02
#define SW_SETUP_HIGHSPEED               0x03
#define SW_SETUP_FORCE_USB               0x04
#define SW_SETUP_CLEAR_USB               0x05
#define SW_SETUP_RESET_MCU               0x06

#define SW_COMMAND_BT_MANUAL_PAIR        0x01
#define SW_COMMAND_REQ_DEV_INFO          0x02
#define SW_COMMAND_SPI_FLASH_READ        0x10
#define SW_COMMAND_SPI_FLASH_WRITE       0x11
#define SW_COMMAND_ENABLE_IMU            0x40
#define SW_COMMAND_ENABLE_RUMBLE         0x48
#define SW_COMMAND_SET_INPUT_MODE        0x03
#define SW_COMMAND_SET_HCI_STATE         0x06
#define SW_COMMAND_SET_IMU_SENSITIVTY    0x41
#define SW_COMMAND_SET_PLAYER_LIGHT      0x30
#define SW_COMMAND_SET_HOME_LIGHT        0x38

// Calibration constants

#define SW_ADDR_CALIB_TRG_USER_LEFT_STATUS   0x8010u
#define SW_ADDR_CALIB_TRG_USER_LEFT          0x8012u
#define SW_ADDR_CALIB_TRG_USER_LEFT_END      0x801Au
#define SW_ADDR_CALIB_TRG_USER_RIGHT_STATUS  0x801Bu
#define SW_ADDR_CALIB_TRG_USER_RIGHT         0x801Du
#define SW_ADDR_CALIB_TRG_FAC_LEFT           0x603Du
#define SW_ADDR_CALIB_TRG_FAC_RIGHT          0x6046u

#define SW_CONST_USER_CALIB_0            0xB2u
#define SW_CONST_USER_CALIB_1            0xA1u
#define SW_CONST_LEN_USER_CALIB_STATUS   (SW_ADDR_CALIB_TRG_USER_LEFT - SW_ADDR_CALIB_TRG_USER_LEFT_STATUS)
#define SW_CONST_LEN_TRG_CALIB           (SW_ADDR_CALIB_TRG_USER_LEFT_END - SW_ADDR_CALIB_TRG_USER_LEFT + 1)


// Calibration
#define CALIB_TYPE_USER    0
#define CALIB_TYPE_FACTORY 1

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

struct SCParsedInput
{
    int16_t  leftJoystick[2];
    int16_t  rightJoystick[2];
    //  0 : Y
    //  1 : X
    //  2 : B
    //  3 : A
    //  4 : R SR
    //  5 : R SL
    //  6 : R Shoulder
    //  7 : R Trigger
    //  8 : dpad down
    //  9 : dpad up
    // 10 : dpad right
    // 11 : dpad left
    // 12 : L SR
    // 13 : L SL
    // 14 : L Shoulder
    // 15 : L Trigger
    uint16_t buttons;
    //  0 : Minus
    //  1 : Plus
    //  2 : R Stick
    //  3 : L Stick
    //  4 : Home
    //  5 : Capture
    uint8_t lowFreqButtons;
    //   0 : Is Wired Bool
    //   4 : Fully Charged Bool
    // 5-7 : Battery level
    uint8_t misc;

    int16_t  accel[3];
    int16_t  gyro[3];
};

#define SW_Y_____ 0x0001
#define SW_X_____ 0x0002
#define SW_B_____ 0x0004
#define SW_A_____ 0x0008
#define SW_R_SR__ 0x0010
#define SW_R_SL__ 0x0020
#define SW_R_SHLD 0x0040
#define SW_R_TRIG 0x0080
#define SW_DP_DWN 0x0100
#define SW_DP_UP_ 0x0200
#define SW_DP_RGT 0x0400
#define SW_DP_LFT 0x0800
#define SW_L_SR__ 0x1000
#define SW_L_SL__ 0x2000
#define SW_L_SHLD 0x4000
#define SW_L_TRIG 0x8000

#define SW_MINUS_ 0x01
#define SW_PLUS__ 0x02
#define SW_R_STCK 0x04
#define SW_L_STCK 0x08
#define SW_HOME__ 0x10
#define SW_CAPTUR 0x20

struct Rumble
{
    uint8_t data[4];
};
COMPILE_ASSERT(sizeof(Rumble) == 4);

struct Calibration
{
    int16_t mid;

    int16_t bLo;
    int16_t bHi;
};
COMPILE_ASSERT(sizeof(Calibration) == 6);

struct AxisCalibration
{
    Calibration x;
    Calibration y;
};
COMPILE_ASSERT(sizeof(AxisCalibration) == 12);

struct SwitchController
{
    uint8_t playerNum : 4; // 4b
    uint8_t bluetooth : 1; // 1b
    uint8_t isGrip : 1; // 1b
    uint8_t isRight : 1; // 1b
    uint8_t isLeft : 1; // 1b
    uint8_t cmdNum; // 4b
    uint8_t type;

    Rumble rumble[2];
    AxisCalibration calib[2];

    SCParsedInput last;
};
// if exceeded, increase HID_CONTROLLER_BUFFER_SIZE
COMPILE_ASSERT(sizeof(SwitchController) <= HID_CONTROLLER_BUFFER_SIZE);


// Packet formats for sending to the switch controllers

struct SCPacketOut_Setup
{
    uint8_t packetType;
    uint8_t command;

    uint8_t __pad0[HID_DEFAULT_PACKET_SIZE - 2];
};
COMPILE_ASSERT(sizeof(SCPacketOut_Setup) == HID_DEFAULT_PACKET_SIZE);

struct SCPacketOut_Rumble
{
    uint8_t packetType;
    uint8_t sequenceNum;
    Rumble rumble[2];
};
COMPILE_ASSERT(sizeof(SCPacketOut_Rumble) == 10);

struct SCPacketOut_General
{
    uint8_t packetType;
    uint8_t sequenceNum;
    Rumble rumble[2];
    uint8_t command;
    uint8_t data[SW_PACKET_SIZE_OUT - 11];

    uint8_t __pad0[HID_DEFAULT_PACKET_SIZE - SW_PACKET_SIZE_OUT];
};
COMPILE_ASSERT(sizeof(SCPacketOut_General) == HID_DEFAULT_PACKET_SIZE);


// Packet formats for listening to the switch controllers

#define SW_SENS__0MS 0
#define SW_SENS__5MS 1
#define SW_SENS_10MS 2

struct SCPacketIn_Input
{
    uint8_t  sequenceNum;
    uint8_t  battery;
    uint8_t  buttons[3];
    uint8_t  leftJoystick[3];
    uint8_t  rightJoystick[3];
    uint8_t  rumbleState;
};
COMPILE_ASSERT(sizeof(SCPacketIn_Input) == 12);

struct SCPacketIn_InputFull
{
    SCPacketIn_Input state;

    // three packets of sensor data in a row 5ms appart
    struct
    {
        int16_t accel[3];
        int16_t gyro[3];
    } sens[3];
};
COMPILE_ASSERT(sizeof(SCPacketIn_InputFull) == 48);

struct SCPacketIn_InputSimple
{
    uint8_t  packetType;

    uint8_t  buttons[2];
    uint8_t  dpad;
    uint16_t  leftJoystick[2];
    uint16_t  rightJoystick[2];
};
COMPILE_ASSERT(sizeof(SCPacketIn_InputSimple) == 12);

struct SCPacketIn_SetupStatus
{
    uint8_t setupAck;
    uint8_t setupFlag;
    uint8_t status;

    uint8_t devType;
    uint8_t devID[6];
};
COMPILE_ASSERT(sizeof(SCPacketIn_SetupStatus) == 10);

struct SCPacketIn_CommandResults
{
    uint8_t packetType;
    SCPacketIn_Input state;

    uint8_t ack;
    uint8_t command;

    // can be either SPIData or DevInfo
    union
    {
        uint8_t __pad1[35];
        struct
        {
            // should access the value of addr via a pointer to uint32
            uint8_t addr[4];
            uint8_t len;
            uint8_t data[30];
        } spi;

        struct
        {
            uint8_t firmwareVer[2];
            uint8_t devType;
            uint8_t __pad2;
            uint8_t devID[6];
            uint8_t __pad3;
            uint8_t colorLoc;
        } devInfo;
    };
};
COMPILE_ASSERT(sizeof(SCPacketIn_CommandResults) == 50);
COMPILE_ASSERT(offsetof(SCPacketIn_CommandResults, spi.data) == 20);
COMPILE_ASSERT(offsetof(SCPacketIn_CommandResults, devInfo.colorLoc) == 26);


// Util

void WriteLE(uint8_t* dest, uint32_t src)
{
    dest[0] = (uint8_t)(src & 0xFF);
    dest[1] = (uint8_t)((src & 0xFF00) >> 8);
    dest[2] = (uint8_t)((src & 0xFF0000) >> 16);
    dest[3] = (uint8_t)((src & 0xFF000000) >> 24);
}

int16_t Get12Bits(uint8_t* src, uint32_t start)
{
    ASSERT(start == 0 || start == 4);
    uint16_t res = 0;

    if (start == 0)
    {
        res = (uint16_t)*src;
        res |= (((uint16_t) * (src + 1)) & 0xF) << 8;
    }
    else if (start == 4)
    {
        res = (((uint16_t)*src) & 0xF0) >> 4;
        res |= ((uint16_t) * (src + 1)) << 4;
    }

    return (int16_t)res;
}


// Info

bool HIDIsSupportedSwitchController(HIDDeviceInfo* devInfo)
{
    // NOTE: Apparently there are third party controllers that only support
    //   passing input state, and can't receive packets. May be an issue
    //   in the future if we allow them to be used.
    switch (devInfo->type)
    {
    case ctSwitchPro:
    case ctSwitchJoyConL:
    case ctSwitchJoyConR:
    case ctSwitchJoyConPair:
        return true;
    default:
        return false;
    }
}



// Input

#define CENTERING 0x7FF
#define DEADZONE 0.15
#define NEG_BOUND (int16_t)(-CENTERING * DEADZONE)
#define POS_BOUND (uint16_t)(CENTERING * DEADZONE)
#define SCALING (float)(CENTERING)
// gravity pulls on whatever acceleration axis faces down
// TODO: test for proper calibration
#define GRAVITY (float)(0x1000)
#define SENSOR_SCALING (float)(0x7FFF)

static int16_t Calibrate(Calibration* calib, int16_t val)
{
    int16_t newVal = val - calib->mid;

    if (newVal > calib->bHi)
        calib->bHi = newVal;
    if (newVal < calib->bLo)
        calib->bLo = newVal;

    if (newVal > calib->bHi * DEADZONE)
        newVal = (int16_t)((newVal / (float)calib->bHi) * CENTERING);
    else if (newVal < calib->bLo * DEADZONE)
        newVal = (int16_t)((newVal / (float)calib->bLo) * -CENTERING);
    else
        newVal = 0;

    return newVal;
}

static SCParsedInput ParseInput(SCPacketIn_Input* rep, AxisCalibration * calibL, AxisCalibration * calibR, uint8_t conType)
{
    SCParsedInput data;
    memset(&data, 0, sizeof data);

    if (conType != SW_CON_TYPE_JOY_R)
    {
        data.leftJoystick[jX] = Get12Bits(rep->leftJoystick, 0);
        data.leftJoystick[jY] = Get12Bits(rep->leftJoystick + 1, 4);

        data.leftJoystick[jX] = Calibrate(&calibL->x, data.leftJoystick[jX]);
        data.leftJoystick[jY] = Calibrate(&calibL->y, data.leftJoystick[jY]);
    }

    if (conType != SW_CON_TYPE_JOY_L)
    {
        data.rightJoystick[jX] = Get12Bits(rep->rightJoystick, 0);
        data.rightJoystick[jY] = Get12Bits(rep->rightJoystick + 1, 4);

        data.rightJoystick[jX] = Calibrate(&calibR->x, data.rightJoystick[jX]);
        data.rightJoystick[jY] = Calibrate(&calibR->y, data.rightJoystick[jY]);
    }


    // Y X B A buttons, R Shoulder & Trigger
    data.buttons |= rep->buttons[0] & 0xCF;

    // Right Joycon internal paddles
    if (conType == SW_CON_TYPE_JOY_R)
        data.buttons |= rep->buttons[0] & 0x30;

    // dpad buttons, L Shoulder & Trigger
    data.buttons |= (rep->buttons[2] & 0xCF) << 8;

    // Left Joycon internal paddles
    if (conType == SW_CON_TYPE_JOY_L)
        data.buttons |= (rep->buttons[2] & 0x30) << 8;

    data.lowFreqButtons |= rep->buttons[1] & 0x3F;

    // battery level
    data.misc = rep->battery;


    return data;
}

static SCParsedInput ParseInputSimple(SCPacketIn_InputSimple* rep, uint8_t conType)
{
    SCParsedInput data;
    memset(&data, 0, sizeof data);

    if (conType == SW_CON_TYPE_PRO)
    {
        // when in simple mode all stick deltas are multiples of 16
        // Y is inverted
        if (conType != SW_CON_TYPE_JOY_R)
        {
            data.leftJoystick[jX] = (int16_t)(rep->leftJoystick[0] / 16);
            data.leftJoystick[jY] = (int16_t)((0xFFFF - rep->leftJoystick[1]) / 16);
        }

        if (conType != SW_CON_TYPE_JOY_L)
        {
            data.rightJoystick[jX] = (int16_t)(rep->rightJoystick[0] / 16);
            data.rightJoystick[jY] = (int16_t)((0xFFFF - rep->rightJoystick[1]) / 16);
        }
    }
    else if (conType == SW_CON_TYPE_JOY_L)
    {
        // map the joycon l & zl buttons to the triggers when in simple mode
        data.buttons |= (uint16_t)(rep->buttons[1] & 0x40) << 1;
        data.buttons |= (uint16_t)(rep->buttons[1] & 0x80) << 8;
    }
    else if (conType == SW_CON_TYPE_JOY_R)
    {
        // map the joycon zr & r buttons to the triggers when in simple mode
        data.buttons |= (uint16_t)(rep->buttons[1] & 0x40) << 9;
        data.buttons |= (uint16_t)(rep->buttons[1] & 0x80);
    }

    data.buttons |= (uint16_t)(rep->buttons[0] & 0b0011) << 2;
    data.buttons |= (uint16_t)(rep->buttons[0] & 0b1100) >> 2;
    data.buttons |= (uint16_t)(rep->buttons[0] & 0x10) << 10;
    data.buttons |= (uint16_t)(rep->buttons[0] & 0x20) << 1;
    data.buttons |= (uint16_t)(rep->buttons[0] & 0x40) << 9;
    data.buttons |= (uint16_t)(rep->buttons[0] & 0x80);

    data.lowFreqButtons |= (rep->buttons[1] & 0x33);
    data.lowFreqButtons |= (rep->buttons[1] & 0x4) << 1;
    data.lowFreqButtons |= (rep->buttons[1] & 0x8) >> 1;

    switch (rep->dpad & 0xF)
    {
    case 0: data.buttons |= 0x200; break;
    case 1: data.buttons |= 0x600; break;
    case 2: data.buttons |= 0x400; break;
    case 3: data.buttons |= 0x500; break;
    case 4: data.buttons |= 0x100; break;
    case 5: data.buttons |= 0x900; break;
    case 6: data.buttons |= 0x800; break;
    case 7: data.buttons |= 0xA00; break;
    case 8: break;
    default: break;
    }


    return data;
}

static void ParseAndHandleSensors(SwitchController* con, uint32_t manID,
	gainput::InputDeltaState * state, SCParsedInput* curr, SCPacketIn_InputFull* rep)
{
#if defined(SENSOR_FREQ_5MS)
    state->AddChange(manID, gainput::PadButtonGyroscopeX,    con->last.gyro[sX]  / SENSOR_SCALING, rep->sens[0].gyro[sX]  / SENSOR_SCALING);
    state->AddChange(manID, gainput::PadButtonGyroscopeY,    con->last.gyro[sY]  / SENSOR_SCALING, rep->sens[0].gyro[sY]  / SENSOR_SCALING);
    state->AddChange(manID, gainput::PadButtonGyroscopeZ,    con->last.gyro[sZ]  / SENSOR_SCALING, rep->sens[0].gyro[sZ]  / SENSOR_SCALING);
    state->AddChange(manID, gainput::PadButtonAccelerationX, con->last.accel[sX] / SENSOR_SCALING, rep->sens[0].accel[sX] / SENSOR_SCALING);
    state->AddChange(manID, gainput::PadButtonAccelerationY, con->last.accel[sY] / SENSOR_SCALING, rep->sens[0].accel[sY] / SENSOR_SCALING);
    state->AddChange(manID, gainput::PadButtonAccelerationZ, con->last.accel[sZ] / SENSOR_SCALING, rep->sens[0].accel[sZ] / SENSOR_SCALING);

    state->AddChange(manID, gainput::PadButtonGyroscopeX,    rep->sens[0].gyro[sX]  / SENSOR_SCALING, rep->sens[1].gyro[sX]  / SENSOR_SCALING);
    state->AddChange(manID, gainput::PadButtonGyroscopeY,    rep->sens[0].gyro[sY]  / SENSOR_SCALING, rep->sens[1].gyro[sY]  / SENSOR_SCALING);
    state->AddChange(manID, gainput::PadButtonGyroscopeZ,    rep->sens[0].gyro[sZ]  / SENSOR_SCALING, rep->sens[1].gyro[sZ]  / SENSOR_SCALING);
    state->AddChange(manID, gainput::PadButtonAccelerationX, rep->sens[0].accel[sX] / SENSOR_SCALING, rep->sens[1].accel[sX] / SENSOR_SCALING);
    state->AddChange(manID, gainput::PadButtonAccelerationY, rep->sens[0].accel[sY] / SENSOR_SCALING, rep->sens[1].accel[sY] / SENSOR_SCALING);
    state->AddChange(manID, gainput::PadButtonAccelerationZ, rep->sens[0].accel[sZ] / SENSOR_SCALING, rep->sens[1].accel[sZ] / SENSOR_SCALING);

    state->AddChange(manID, gainput::PadButtonGyroscopeX,    rep->sens[0].gyro[sX]  / SENSOR_SCALING, rep->sens[2].gyro[sX]  / SENSOR_SCALING);
    state->AddChange(manID, gainput::PadButtonGyroscopeY,    rep->sens[0].gyro[sY]  / SENSOR_SCALING, rep->sens[2].gyro[sY]  / SENSOR_SCALING);
    state->AddChange(manID, gainput::PadButtonGyroscopeZ,    rep->sens[0].gyro[sZ]  / SENSOR_SCALING, rep->sens[2].gyro[sZ]  / SENSOR_SCALING);
    state->AddChange(manID, gainput::PadButtonAccelerationX, rep->sens[0].accel[sX] / SENSOR_SCALING, rep->sens[2].accel[sX] / SENSOR_SCALING);
    state->AddChange(manID, gainput::PadButtonAccelerationY, rep->sens[0].accel[sY] / SENSOR_SCALING, rep->sens[2].accel[sY] / SENSOR_SCALING);
    state->AddChange(manID, gainput::PadButtonAccelerationZ, rep->sens[0].accel[sZ] / SENSOR_SCALING, rep->sens[2].accel[sZ] / SENSOR_SCALING);
#else
    state->AddChange(manID, gainput::PadButtonGyroscopeX,    con->last.gyro[sX]  / SENSOR_SCALING, rep->sens[2].gyro[sX]  / SENSOR_SCALING);
    state->AddChange(manID, gainput::PadButtonGyroscopeY,    con->last.gyro[sY]  / SENSOR_SCALING, rep->sens[2].gyro[sY]  / SENSOR_SCALING);
    state->AddChange(manID, gainput::PadButtonGyroscopeZ,    con->last.gyro[sZ]  / SENSOR_SCALING, rep->sens[2].gyro[sZ]  / SENSOR_SCALING);
    state->AddChange(manID, gainput::PadButtonAccelerationX, con->last.accel[sX] / SENSOR_SCALING, rep->sens[2].accel[sX] / SENSOR_SCALING);
    state->AddChange(manID, gainput::PadButtonAccelerationY, con->last.accel[sY] / SENSOR_SCALING, rep->sens[2].accel[sY] / SENSOR_SCALING);
    state->AddChange(manID, gainput::PadButtonAccelerationZ, con->last.accel[sZ] / SENSOR_SCALING, rep->sens[2].accel[sZ] / SENSOR_SCALING);
#endif

    curr->accel[sX] = rep->sens[2].accel[sX];
    curr->accel[sY] = rep->sens[2].accel[sY];
    curr->accel[sZ] = rep->sens[2].accel[sZ];
    curr->gyro[sX] = rep->sens[2].gyro[sX];
    curr->gyro[sY] = rep->sens[2].gyro[sY];
    curr->gyro[sZ] = rep->sens[2].gyro[sZ];
}

static void HandleParsedInput(SwitchController* con, uint32_t manID,
	gainput::InputDeltaState * state, SCParsedInput* curr)
{
    // Axes

    // Left Joystick
    bool leftActive = curr->leftJoystick[jX] || con->last.leftJoystick[jX] ||
        curr->leftJoystick[jY] || con->last.leftJoystick[jY];
    if (leftActive)
    {
        // Left Joystick X
        state->AddChange(manID, gainput::PadButtonLeftStickX,
            con->last.leftJoystick[jX] / SCALING, curr->leftJoystick[jX] / SCALING);
        // Left Joystick Y
        state->AddChange(manID, gainput::PadButtonLeftStickY,
            con->last.leftJoystick[jY] / SCALING, curr->leftJoystick[jY] / SCALING);
    }

    // Right Joystick
    bool rightActive = curr->rightJoystick[jX] || con->last.rightJoystick[jX] ||
        curr->rightJoystick[jY] || con->last.rightJoystick[jY];
    if (rightActive)
    {
        // Right Joystick X
        state->AddChange(manID, gainput::PadButtonRightStickX,
            con->last.rightJoystick[jX] / SCALING, curr->rightJoystick[jX] / SCALING);
        // Right Joystick Y
        state->AddChange(manID, gainput::PadButtonRightStickY,
            con->last.rightJoystick[jY] / SCALING, curr->rightJoystick[jY] / SCALING);
    }


    // On the switch the triggers are buttons, but we'll treat them as axises
    bool lWasTriggered = !!(con->last.buttons & SW_L_TRIG);
    bool lTriggered = !!(curr->buttons & SW_L_TRIG);
    if (lWasTriggered || lTriggered)
        state->AddChange(manID, gainput::PadButtonAxis4, (float)lWasTriggered, (float)lTriggered);
    bool rWasTriggered = !!(con->last.buttons & SW_R_TRIG);
    bool rTriggered = !!(curr->buttons & SW_R_TRIG);
    if (rWasTriggered || rTriggered)
        state->AddChange(manID, gainput::PadButtonAxis5, (float)rWasTriggered, (float)rTriggered);


    // Buttons

    uint16_t buttonsChanged = (curr->buttons ^ con->last.buttons) & ~SW_R_TRIG & ~SW_L_TRIG;

    if (buttonsChanged)
    {
        if (buttonsChanged & SW_Y_____)
            state->AddChange(manID, gainput::PadButtonX, (bool)(con->last.buttons & SW_Y_____), (bool)(curr->buttons & SW_Y_____));
        if (buttonsChanged & SW_X_____)
            state->AddChange(manID, gainput::PadButtonY, (bool)(con->last.buttons & SW_X_____), (bool)(curr->buttons & SW_X_____));
        if (buttonsChanged & SW_B_____)
            state->AddChange(manID, gainput::PadButtonA, (bool)(con->last.buttons & SW_B_____), (bool)(curr->buttons & SW_B_____));
        if (buttonsChanged & SW_A_____)
            state->AddChange(manID, gainput::PadButtonB, (bool)(con->last.buttons & SW_A_____), (bool)(curr->buttons & SW_A_____));
        // For now we'll send them as L2 and R2, in the future we'll diffrentiate them and allow for remapping
        if (buttonsChanged & SW_R_SR__)
            state->AddChange(manID, gainput::PadButtonR2, (bool)(con->last.buttons & SW_R_SR__), (bool)(curr->buttons & SW_R_SR__));
        if (buttonsChanged & SW_R_SL__)
            state->AddChange(manID, gainput::PadButtonL2, (bool)(con->last.buttons & SW_R_SL__), (bool)(curr->buttons & SW_R_SL__));
        if (buttonsChanged & SW_R_SHLD)
            state->AddChange(manID, gainput::PadButtonR1, (bool)(con->last.buttons & SW_R_SHLD), (bool)(curr->buttons & SW_R_SHLD));
        if (buttonsChanged & SW_DP_DWN)
            state->AddChange(manID, gainput::PadButtonDown, (bool)(con->last.buttons & SW_DP_DWN), (bool)(curr->buttons & SW_DP_DWN));
        if (buttonsChanged & SW_DP_UP_)
            state->AddChange(manID, gainput::PadButtonUp, (bool)(con->last.buttons & SW_DP_UP_), (bool)(curr->buttons & SW_DP_UP_));
        if (buttonsChanged & SW_DP_RGT)
            state->AddChange(manID, gainput::PadButtonRight, (bool)(con->last.buttons & SW_DP_RGT), (bool)(curr->buttons & SW_DP_RGT));
        if (buttonsChanged & SW_DP_LFT)
            state->AddChange(manID, gainput::PadButtonLeft, (bool)(con->last.buttons & SW_DP_LFT), (bool)(curr->buttons & SW_DP_LFT));
        // For now we'll send them as L2 and R2, in the future we'll diffrentiate them and allow for remapping
        if (buttonsChanged & SW_L_SR__)
            state->AddChange(manID, gainput::PadButtonR2, (bool)(con->last.buttons & SW_L_SR__), (bool)(curr->buttons & SW_L_SR__));
        if (buttonsChanged & SW_L_SL__)
            state->AddChange(manID, gainput::PadButtonL2, (bool)(con->last.buttons & SW_L_SL__), (bool)(curr->buttons & SW_L_SL__));
        if (buttonsChanged & SW_L_SHLD)
            state->AddChange(manID, gainput::PadButtonL1, (bool)(con->last.buttons & SW_L_SHLD), (bool)(curr->buttons & SW_L_SHLD));
    }


    // Low Frequency Buttons

    uint8_t lowFreqDiff = con->last.lowFreqButtons ^ curr->lowFreqButtons;

    if (lowFreqDiff)
    {
        // dpad
        if (lowFreqDiff & SW_MINUS_)
            state->AddChange(manID, gainput::PadButtonSelect, (bool)(con->last.lowFreqButtons & SW_MINUS_), (bool)(curr->lowFreqButtons & SW_MINUS_));
        if (lowFreqDiff & SW_PLUS__)
            state->AddChange(manID, gainput::PadButtonStart, (bool)(con->last.lowFreqButtons & SW_PLUS__), (bool)(curr->lowFreqButtons & SW_PLUS__));
        if (lowFreqDiff & SW_R_STCK)
            state->AddChange(manID, gainput::PadButtonR3, (bool)(con->last.lowFreqButtons & SW_R_STCK), (bool)(curr->lowFreqButtons & SW_R_STCK));
        if (lowFreqDiff & SW_L_STCK)
            state->AddChange(manID, gainput::PadButtonL3, (bool)(con->last.lowFreqButtons & SW_L_STCK), (bool)(curr->lowFreqButtons & SW_L_STCK));
        // shape buttons
        if (lowFreqDiff & SW_HOME__)
            state->AddChange(manID, gainput::PadButtonHome, (bool)(con->last.lowFreqButtons & SW_HOME__), (bool)(curr->lowFreqButtons & SW_HOME__));
        if (lowFreqDiff & SW_CAPTUR)
            state->AddChange(manID, gainput::PadButton17, (bool)(con->last.lowFreqButtons & SW_CAPTUR), (bool)(curr->lowFreqButtons & SW_CAPTUR));
    }
}


static void HIDParseSwitchControllerInputFull(SwitchController* con,
    SCPacketIn_InputFull* currRep, uint32_t manID, gainput::InputDeltaState * state)
{
    SCParsedInput parsedRep = ParseInput(&currRep->state, &con->calib[tL], &con->calib[tR], con->type);
    ParseAndHandleSensors(con, manID, state, &parsedRep, currRep);
    HandleParsedInput(con, manID, state, &parsedRep);

    con->last = parsedRep;
}

static void HIDParseSwitchControllerInputSimple(SwitchController* con,
    SCPacketIn_InputSimple* currRep, uint32_t manID, gainput::InputDeltaState * state)
{
    SCParsedInput parsedRep = ParseInputSimple(currRep, con->type);
    HandleParsedInput(con, manID, state, &parsedRep);

    con->last = parsedRep;
}


// Communication

// Package communication for setting up feature access
static int SendSetup(HIDController* con, uint8_t command, bool waitForReply, uint8_t* out, uint32_t outSize)
{
    SCPacketOut_Setup packet;
    memset(&packet, 0, sizeof packet);
    packet.packetType = SW_PACKET_TYPE_OUT_SETUP;
    packet.command = command;

    for (int32_t retry = MAX_RETRIES; retry; --retry)
    {
        if (!hid_write(con->hidDev, (uint8_t*)&packet, sizeof packet))
            continue;

        if (!waitForReply)
            return 0;


        // now wait for a reply

        uint8_t recBuf[SW_PACKET_MAX_SIZE_IN];
        int32_t size;

        uint64_t now = HIDGetTime();;
        uint64_t stop = now + RESPONSE_TIMEOUT;

        while ((size = hid_read_timeout(con->hidDev, recBuf, SW_PACKET_MAX_SIZE_IN, 0)) != -1)
        {
#if defined(VERBOSE_SWITCH_PACKET_LOGGING)
            if (size &&
                // One or the other of these two get sent constantly so long as the device is properly connected
                recBuf[0] != SW_PACKET_TYPE_IN_FULL_STATE &&
                recBuf[0] != SW_PACKET_TYPE_IN_SIMPLE_STATE)
            {
                RAW_LOGF(LogLevel::eINFO, "Setup:");
                for (int i = 0; i < 64; ++i)
                    RAW_LOGF(LogLevel::eINFO, " %2x", recBuf[i]);
                RAW_LOGF(LogLevel::eINFO, "\n");
            }
#endif

            if (size &&
                recBuf[0] == SW_PACKET_TYPE_IN_SETUP_ACK &&
                recBuf[1] == command)
            {
                if (out && outSize > 0)
                {
                    memcpy(out, recBuf, outSize < SW_PACKET_MAX_SIZE_IN ? outSize : SW_PACKET_MAX_SIZE_IN);
                }

                return size;
            }

            // busy wait
            now = HIDGetTime();
            if (now >= stop)
                break;
        }
    }

    return -1;
}

// Package communication for querying and/or writing state
static int SendCommand(HIDController* con, uint8_t command,
    uint8_t* data, uint32_t dataSize, uint8_t* out, uint32_t outSize)
{
    SwitchController* swCon = (SwitchController*)con->data;

    // Init packet
    SCPacketOut_General packet;
    memset(&packet, 0, sizeof packet);
    packet.packetType = SW_PACKET_TYPE_OUT_GENERAL;
    packet.rumble[tL] = swCon->rumble[tL];
    packet.rumble[tR] = swCon->rumble[tR];
    packet.command = command;
    memcpy(packet.data, data, dataSize < sizeof packet.data ? dataSize : sizeof packet.data);


    for (int32_t retry = MAX_RETRIES; retry; --retry)
    {
        // packets require sequential 4 bit ids
        packet.sequenceNum = swCon->cmdNum;
        swCon->cmdNum = (swCon->cmdNum + 1) % 0xF;

        if (!hid_write(con->hidDev, (uint8_t*)&packet, sizeof packet))
            continue;


        // now wait for a reply

        uint8_t recBuf[SW_PACKET_MAX_SIZE_IN];
        int32_t size;

        uint64_t now = HIDGetTime();
        uint64_t stop = now + RESPONSE_TIMEOUT;

        while ((size = hid_read_timeout(con->hidDev, recBuf, SW_PACKET_MAX_SIZE_IN, 0)) != -1)
        {
            SCPacketIn_CommandResults* res = (SCPacketIn_CommandResults*)recBuf;

#if defined(VERBOSE_SWITCH_PACKET_LOGGING)
            if (size &&
                // One or the other of these two get sent constantly so long as the device is properly connected
                res->packetType != SW_PACKET_TYPE_IN_FULL_STATE &&
                res->packetType != SW_PACKET_TYPE_IN_SIMPLE_STATE)
            {
                RAW_LOGF(LogLevel::eINFO, "Command:");
                for (int i = 0; i < 64; ++i)
                    RAW_LOGF(LogLevel::eINFO, " %2x", recBuf[i]);
                RAW_LOGF(LogLevel::eINFO, "\n");
            }
#endif

            if (size &&
                res->packetType == SW_PACKET_TYPE_IN_GENERAL_REPLY &&
                res->ack        &  SW_PACKET_TYPE_IN_GENERAL_ACK &&
                res->command    == command)
            {
                if (out && outSize > 0)
                {
                    memcpy(out, recBuf, outSize < SW_PACKET_MAX_SIZE_IN ? outSize : SW_PACKET_MAX_SIZE_IN);
                }

                return size;
            }

            // busy wait
            now = HIDGetTime();
            if (now >= stop)
                break;
        }
    }

    return -1;
}

static int SendRumble(HIDController* con, Rumble left, Rumble right)
{
    SwitchController* swCon = (SwitchController*)con->data;

    // Init packet
    SCPacketOut_General packet;
    memset(&packet, 0, sizeof packet);
    packet.packetType = SW_PACKET_TYPE_OUT_RUMBLE_ONLY;
    packet.rumble[tL] = left;
    packet.rumble[tR] = right;
    packet.sequenceNum = swCon->cmdNum;
    swCon->cmdNum = (swCon->cmdNum + 1) % 0xF;

    return hid_write(con->hidDev, (uint8_t*)&packet, sizeof packet);
}

static int SendReadRequest(HIDController* con, uint32_t memAddr, uint8_t numBytesToRead, uint8_t* out, uint32_t outSize)
{
    uint8_t sendBuf[5];
    memset(sendBuf, 0, sizeof sendBuf);
    WriteLE(sendBuf, memAddr);
    sendBuf[4] = numBytesToRead;

    int size = SendCommand(con, SW_COMMAND_SPI_FLASH_READ, sendBuf, sizeof sendBuf, out, outSize);
    if (size < 0)
        LOGF(eERROR, "Failed to read memory from Nintendo controller!");

    return size;
}


// Effects

// source: https://github.com/dekuNukem/Nintendo_Switch_Reverse_Engineering/blob/master/rumble_data_table.md
// 78: This and below will give a strobing effect
// 95: Starts becoming audible
// 174/178: The most power, likely near the resonate frequency of the LRA. At the theoretical safe max amplitude this is startlingly strong
// 334: (And to a lesser degree 327) Another powerful frequency, likely around 2x the resonate frequency
// 600: Almost physically imperceptible by now other than the motors kicking into motion
// 1253: Audible all the way up this last viable frequency
static const uint16_t freq_ref[] =
{// freq   low    high  freq   low    high  freq   low    high  freq   low    high  freq   low    high  freq   low    high
      41, 0x01, 0x0000,   42, 0x02, 0x0000,   43, 0x03, 0x0000,   44, 0x04, 0x0000,   45, 0x05, 0x0000,   46, 0x06, 0x0000,
      47, 0x07, 0x0000,   48, 0x08, 0x0000,   49, 0x09, 0x0000,   50, 0x0A, 0x0000,   51, 0x0B, 0x0000,   52, 0x0C, 0x0000,
      53, 0x0D, 0x0000,   54, 0x0E, 0x0000,   55, 0x0F, 0x0000,   57, 0x10, 0x0000,   58, 0x11, 0x0000,   59, 0x12, 0x0000,
      60, 0x13, 0x0000,   62, 0x14, 0x0000,   63, 0x15, 0x0000,   64, 0x16, 0x0000,   66, 0x17, 0x0000,   67, 0x18, 0x0000,
      69, 0x19, 0x0000,   70, 0x1A, 0x0000,   72, 0x1B, 0x0000,   73, 0x1C, 0x0000,   75, 0x1D, 0x0000,   77, 0x1e, 0x0000,
      78, 0x1f, 0x0000,   80, 0x20, 0x0000,   82, 0x21, 0x0400,   84, 0x22, 0x0800,   85, 0x23, 0x0c00,   87, 0x24, 0x1000,
      89, 0x25, 0x1400,   91, 0x26, 0x1800,   93, 0x27, 0x1c00,   95, 0x28, 0x2000,   97, 0x29, 0x2400,   99, 0x2a, 0x2800,
     102, 0x2b, 0x2c00,  104, 0x2c, 0x3000,  106, 0x2d, 0x3400,  108, 0x2e, 0x3800,  111, 0x2f, 0x3c00,  113, 0x30, 0x4000,
     116, 0x31, 0x4400,  118, 0x32, 0x4800,  121, 0x33, 0x4c00,  123, 0x34, 0x5000,  126, 0x35, 0x5400,  129, 0x36, 0x5800,
     132, 0x37, 0x5c00,  135, 0x38, 0x6000,  137, 0x39, 0x6400,  141, 0x3a, 0x6800,  144, 0x3b, 0x6c00,  147, 0x3c, 0x7000,
     150, 0x3d, 0x7400,  153, 0x3e, 0x7800,  157, 0x3f, 0x7c00,  160, 0x40, 0x8000,  164, 0x41, 0x8400,  167, 0x42, 0x8800,
     171, 0x43, 0x8c00,  174, 0x44, 0x9000,  178, 0x45, 0x9400,  182, 0x46, 0x9800,  186, 0x47, 0x9c00,  190, 0x48, 0xa000,
     194, 0x49, 0xa400,  199, 0x4a, 0xa800,  203, 0x4b, 0xac00,  207, 0x4c, 0xb000,  212, 0x4d, 0xb400,  217, 0x4e, 0xb800,
     221, 0x4f, 0xbc00,  226, 0x50, 0xc000,  231, 0x51, 0xc400,  236, 0x52, 0xc800,  241, 0x53, 0xcc00,  247, 0x54, 0xd000,
     252, 0x55, 0xd400,  258, 0x56, 0xd800,  263, 0x57, 0xdc00,  269, 0x58, 0xe000,  275, 0x59, 0xe400,  281, 0x5a, 0xe800,
     287, 0x5b, 0xec00,  293, 0x5c, 0xf000,  300, 0x5d, 0xf400,  306, 0x5e, 0xf800,  313, 0x5f, 0xfc00,  320, 0x60, 0x0001,
     327, 0x61, 0x0401,  334, 0x62, 0x0801,  341, 0x63, 0x0c01,  349, 0x64, 0x1001,  357, 0x65, 0x1401,  364, 0x66, 0x1801,
     372, 0x67, 0x1c01,  381, 0x68, 0x2001,  389, 0x69, 0x2401,  397, 0x6a, 0x2801,  406, 0x6b, 0x2c01,  415, 0x6c, 0x3001,
     424, 0x6d, 0x3401,  433, 0x6e, 0x3801,  443, 0x6f, 0x3c01,  453, 0x70, 0x4001,  462, 0x71, 0x4401,  473, 0x72, 0x4801,
     483, 0x73, 0x4c01,  494, 0x74, 0x5001,  504, 0x75, 0x5401,  515, 0x76, 0x5801,  527, 0x77, 0x5c01,  538, 0x78, 0x6001,
     550, 0x79, 0x6401,  562, 0x7a, 0x6801,  574, 0x7b, 0x6c01,  587, 0x7c, 0x7001,  600, 0x7d, 0x7401,  613, 0x7e, 0x7801,
     626, 0x7f, 0x7c01,  640, 0x00, 0x8001,  654, 0x00, 0x8401,  668, 0x00, 0x8801,  683, 0x00, 0x8c01,  698, 0x00, 0x9001,
     713, 0x00, 0x9401,  729, 0x00, 0x9801,  745, 0x00, 0x9c01,  761, 0x00, 0xa001,  778, 0x00, 0xa401,  795, 0x00, 0xa801,
     812, 0x00, 0xac01,  830, 0x00, 0xb001,  848, 0x00, 0xb401,  867, 0x00, 0xb801,  886, 0x00, 0xbc01,  905, 0x00, 0xc001,
     925, 0x00, 0xc401,  945, 0x00, 0xc801,  966, 0x00, 0xcc01,  987, 0x00, 0xd001, 1009, 0x00, 0xd401, 1031, 0x00, 0xd801,
    1053, 0x00, 0xdc01, 1076, 0x00, 0xe001, 1100, 0x00, 0xe401, 1124, 0x00, 0xe801, 1149, 0x00, 0xec01, 1174, 0x00, 0xf001,
    1199, 0x00, 0xf401, 1226, 0x00, 0xf801, 1253, 0x00, 0xfc01
};

static int GetNearestFreq(uint16_t freq)
{
    const uint32_t len = sizeof(freq_ref) / sizeof(uint16_t);

    for (uint32_t i = 0; i < len; i += 3)
    {
        if (freq <= freq_ref[i])
        {
            return i;
        }
    }

    // return the last entry if none are a better fit
    return len - 3;
}

static const uint16_t amp_safe_max = 1003;
// source: https://github.com/dekuNukem/Nintendo_Switch_Reverse_Engineering/blob/master/rumble_data_table.md
static const uint16_t amp_ref[] =
{//  amp   low    high   amp   low    high   amp   low    high   amp   low    high   amp   low    high   amp   low    high
       0, 0x0040, 0x00,   10, 0x8040, 0x02,   12, 0x0041, 0x04,   14, 0x8041, 0x06,   17, 0x0042, 0x08,   20, 0x8042, 0x0a,
      24, 0x0043, 0x0c,   28, 0x8043, 0x0e,   33, 0x0044, 0x10,   40, 0x8044, 0x12,   47, 0x0045, 0x14,   56, 0x8045, 0x16,
      67, 0x0046, 0x18,   80, 0x8046, 0x1a,   95, 0x0047, 0x1c,  112, 0x8047, 0x1e,  117, 0x0048, 0x20,  123, 0x8048, 0x22,
     128, 0x0049, 0x24,  134, 0x8049, 0x26,  140, 0x004a, 0x28,  146, 0x804a, 0x2a,  152, 0x004b, 0x2c,  159, 0x804b, 0x2e,
     166, 0x004c, 0x30,  173, 0x804c, 0x32,  181, 0x004d, 0x34,  189, 0x804d, 0x36,  198, 0x004e, 0x38,  206, 0x804e, 0x3a,
     215, 0x004f, 0x3c,  225, 0x804f, 0x3e,  230, 0x0050, 0x40,  235, 0x8050, 0x42,  240, 0x0051, 0x44,  245, 0x8051, 0x46,
     251, 0x0052, 0x48,  256, 0x8052, 0x4a,  262, 0x0053, 0x4c,  268, 0x8053, 0x4e,  273, 0x0054, 0x50,  279, 0x8054, 0x52,
     286, 0x0055, 0x54,  292, 0x8055, 0x56,  298, 0x0056, 0x58,  305, 0x8056, 0x5a,  311, 0x0057, 0x5c,  318, 0x8057, 0x5e,
     325, 0x0058, 0x60,  332, 0x8058, 0x62,  340, 0x0059, 0x64,  347, 0x8059, 0x66,  355, 0x005a, 0x68,  362, 0x805a, 0x6a,
     370, 0x005b, 0x6c,  378, 0x805b, 0x6e,  387, 0x005c, 0x70,  395, 0x805c, 0x72,  404, 0x005d, 0x74,  413, 0x805d, 0x76,
     422, 0x005e, 0x78,  431, 0x805e, 0x7a,  440, 0x005f, 0x7c,  450, 0x805f, 0x7e,  460, 0x0060, 0x80,  470, 0x8060, 0x82,
     480, 0x0061, 0x84,  491, 0x8061, 0x86,  501, 0x0062, 0x88,  512, 0x8062, 0x8a,  524, 0x0063, 0x8c,  535, 0x8063, 0x8e,
     547, 0x0064, 0x90,  559, 0x8064, 0x92,  571, 0x0065, 0x94,  584, 0x8065, 0x96,  596, 0x0066, 0x98,  609, 0x8066, 0x9a,
     623, 0x0067, 0x9c,  636, 0x8067, 0x9e,  650, 0x0068, 0xa0,  665, 0x8068, 0xa2,  679, 0x0069, 0xa4,  694, 0x8069, 0xa6,
     709, 0x006a, 0xa8,  725, 0x806a, 0xaa,  741, 0x006b, 0xac,  757, 0x806b, 0xae,  773, 0x006c, 0xb0,  790, 0x806c, 0xb2,
     808, 0x006d, 0xb4,  825, 0x806d, 0xb6,  843, 0x006e, 0xb8,  862, 0x806e, 0xba,  881, 0x006f, 0xbc,  900, 0x806f, 0xbe,
     920, 0x0070, 0xc0,  940, 0x8070, 0xc2,  960, 0x0071, 0xc4,  981, 0x8071, 0xc6, amp_safe_max, 0x0072, 0xc8,
     // Everything below here risks damaging the actuators
    //1025, 0x8072, 0xca, 1047, 0x0073, 0xcc, 1070, 0x8073, 0xce, 1094, 0x0074, 0xd0, 1118, 0x8074, 0xd2, 1142, 0x0075, 0xd4,
    //1167, 0x8075, 0xd6, 1193, 0x0076, 0xd8, 1219, 0x8076, 0xda, 1245, 0x0077, 0xdc, 1273, 0x8077, 0xde, 1301, 0x0078, 0xe0,
    //1329, 0x8078, 0xe2, 1358, 0x0079, 0xe4, 1388, 0x8079, 0xe6, 1418, 0x007a, 0xe8, 1449, 0x807a, 0xea, 1481, 0x007b, 0xec,
    //1513, 0x807b, 0xee, 1547, 0x007c, 0xf0, 1580, 0x807c, 0xf2, 1615, 0x007d, 0xf4, 1650, 0x807d, 0xf6, 1686, 0x007e, 0xf8,
    //1723, 0x807e, 0xfa, 1761, 0x007f, 0xfc, 1800, 0x807f, 0xfe
};

static uint16_t GetNearestAmp(uint16_t amp)
{
    const uint32_t len = sizeof(amp_ref) / sizeof(uint16_t);

    for (uint16_t i = 0; i < len; i += 3)
    {
        if (amp <= amp_ref[i])
        {
            return amp <= amp_safe_max ? i : 300u;
        }
    }

    // return the last safe entry if none are a better fit
    return 300u;
}


static void SetNoRumble(Rumble* rumble)
{
    rumble->data[0] = 0x00;
    rumble->data[1] = 0x01;
    rumble->data[2] = 0x40;
    rumble->data[3] = 0x40;
}

static void SetRumble(Rumble* rumble, uint8_t lowFreq, uint16_t highFreq, uint16_t lowAmp, uint8_t highAmp)
{
    rumble->data[0] = (highFreq >> 8) & 0xFF;
    rumble->data[1] = highAmp | (highFreq & 0xFF);
    rumble->data[2] = ((lowAmp >> 8) & 0xFF) | lowFreq;
    rumble->data[3] = lowAmp & 0xFF;
}

static void HIDDoRumbleSwitchCon(HIDController* controller, uint16_t left, uint16_t right)
{
    SwitchController* con = (SwitchController*)&controller->data;

    if (left == 0 && right == 0)
    {
        SetNoRumble(&con->rumble[tL]);
        SetNoRumble(&con->rumble[tR]);
    }
    else
    {
        uint16_t iLeft = GetNearestAmp((uint16_t)((left / (float)0xFFFF) * amp_safe_max));
        uint16_t iRight = GetNearestAmp((uint16_t)((right / (float)0xFFFF) * amp_safe_max));
        uint16_t iFreq = (uint16_t)GetNearestFreq(160);

        // frequency and amplitude stored as {..., val, low, high, ...} so just index to get the relative low and high vals
        //   passed index always points to 'val'
        uint8_t lowFreq = (uint8_t)freq_ref[iFreq + 1];
        uint16_t highFreq =        freq_ref[iFreq + 2];

        uint16_t lowAmp =          amp_ref[iLeft + 1];
        uint8_t highAmp = (uint8_t)amp_ref[iLeft + 2];
        SetRumble(&con->rumble[tL], lowFreq, highFreq, lowAmp, highAmp);

        lowAmp =           amp_ref[iRight + 1];
        highAmp = (uint8_t)amp_ref[iRight + 2];
        SetRumble(&con->rumble[tR], lowFreq, highFreq, lowAmp, highAmp);
    }

    SendRumble(controller, con->rumble[tL], con->rumble[tR]);
}

static void SetHomeButtonLightPattern(HIDController* controller, uint8_t intensity)
{
    uint8_t mappedIntensity = (uint8_t)((intensity / 100.0f) * 0xF);

    // TODO: needs lots of testing
    uint8_t ledData[4];
    ledData[0] = 0x1; // enable
    ledData[0] |= 0x0 << 4; // cycles per 8 ms ?
    ledData[1] = mappedIntensity << 4; // start intensity
    ledData[1] |= 0x0; // start cycles
    ledData[2] = mappedIntensity << 4; // transition intensity
    ledData[2] |= 0x0; // transition cycles
    ledData[3] = 0x0; // end
    ledData[3] |= 0x0 << 4; // non cycles?

    SendCommand(controller, SW_COMMAND_SET_HOME_LIGHT, ledData, sizeof ledData, NULL, 0);
}

// 0 based, stores 4 lowest bits, applies value mod 4
static void HIDSetPlayerSwitchCon(HIDController* controller, uint8_t playerNum)
{
    SwitchController* con = (SwitchController *)&controller->data;
    con->playerNum = playerNum & 0xF;

    uint8_t ledData[] = { (uint8_t)(1 << (con->playerNum % 4)) };
    SendCommand(controller, SW_COMMAND_SET_PLAYER_LIGHT, ledData, sizeof ledData, NULL, 0);
}

static void HIDSetLightsSwitchCon(HIDController* controller, uint8_t r, uint8_t g, uint8_t b)
{
    UNREF_PARAM(r); 
    UNREF_PARAM(g); 
    SetHomeButtonLightPattern(controller, b);
}


// General

static int HIDUpdateSwitchController(HIDController* controller, gainput::InputDeltaState * state)
{
    SwitchController* con = (SwitchController*)controller->data;
    hid_device* dev = (hid_device*)controller->hidDev;

    uint8_t data[SW_PACKET_SIZE_USB_IN];
    int32_t size;

    for (; (size = hid_read_timeout(dev, data, SW_PACKET_SIZE_USB_IN, 0)) > 0;)
    {
        ASSERT(size <= SW_PACKET_SIZE_USB_IN);

        switch (data[0])
        {
        case SW_PACKET_TYPE_IN_FULL_STATE:
            HIDParseSwitchControllerInputFull(con, (SCPacketIn_InputFull*)(data + 1), controller->manID, state);
            break;
        case SW_PACKET_TYPE_IN_SIMPLE_STATE:
            HIDParseSwitchControllerInputSimple(con, (SCPacketIn_InputSimple*)(data), controller->manID, state);
            break;
        default:
            break;
        }
    }

    if (size < 0)
        // Controller should be disconnected if this is true
        return -1;
    return 0;
}

static void HIDCloseSwitchController(HIDController* controller)
{
    hid_close((hid_device *)controller->hidDev);
    controller->hidDev = NULL;
}


// Setup

static int DetermineConnectionType(HIDDeviceInfo* dev, HIDController* controller)
{
    uint8_t recBuf[SW_PACKET_MAX_SIZE_IN];

    SwitchController* con = (SwitchController *)&controller->data;

    // query for communication availability over usb
    if (SendSetup(controller, SW_SETUP_STATUS, true, recBuf, SW_PACKET_MAX_SIZE_IN) >= 0)
    {
        SCPacketIn_SetupStatus* rep = (SCPacketIn_SetupStatus*)recBuf;
        // set when using the grip but the given joycon is disconnected
        if (rep->status == 0x3)
            return -1;

        // pull info
        con->type = rep->devType;
        dev->mac =
            ((uint64_t)rep->devID[0]) <<  0 |
            ((uint64_t)rep->devID[1]) <<  8 |
            ((uint64_t)rep->devID[2]) << 16 |
            ((uint64_t)rep->devID[3]) << 24 |
            ((uint64_t)rep->devID[4]) << 32 |
            ((uint64_t)rep->devID[5]) << 40;

        uint8_t temp;
        if (GetIDByMac(dev->mac, &temp, dev->id))
            return -2;

        return 0;
    }

    uint8_t sendBuf[SW_PACKET_MAX_SIZE_IN];
    memset(sendBuf, 0, sizeof sendBuf);

    // test if bluetooth
    if (SendCommand(controller, SW_COMMAND_REQ_DEV_INFO, sendBuf, 2, recBuf, SW_PACKET_MAX_SIZE_IN) >= 0)
    {
        SCPacketIn_CommandResults* rep = (SCPacketIn_CommandResults*)recBuf;

        // pull info
        con->type = rep->devInfo.devType;
        con->bluetooth = 1;
        dev->mac =
            ((uint64_t)rep->devInfo.devID[0]) << 40 |
            ((uint64_t)rep->devInfo.devID[1]) << 32 |
            ((uint64_t)rep->devInfo.devID[2]) << 24 |
            ((uint64_t)rep->devInfo.devID[3]) << 16 |
            ((uint64_t)rep->devInfo.devID[4]) <<  8 |
            ((uint64_t)rep->devInfo.devID[5]) <<  0;

        uint8_t temp;
        if (GetIDByMac(dev->mac, &temp, dev->id))
            return -2;

        return 0;
    }

    return -1;
}

static int InitializeConnection(HIDController* controller)
{
    SwitchController* con = (SwitchController *)&controller->data;

    if (!con->bluetooth)
    {
        // Setup USB

        // The right hand controller does in fact get initialized, it just doesn't respond like
        //   anything else when in the grip, so we shouldn't wait. If set up though we should
        //   receive packets as expected in the update step.
        if (con->isRight && con->isGrip)
        {
            SendSetup(controller, SW_SETUP_HANDSHAKE, false, NULL, 0);
            SendSetup(controller, SW_SETUP_HIGHSPEED, false, NULL, 0);
            SendSetup(controller, SW_SETUP_HANDSHAKE, false, NULL, 0);
            SendSetup(controller, SW_SETUP_FORCE_USB, false, NULL, 0);
        }
        else
        {
            SendSetup(controller, SW_SETUP_HANDSHAKE, true, NULL, 0);
            SendSetup(controller, SW_SETUP_HIGHSPEED, true, NULL, 0);
            SendSetup(controller, SW_SETUP_HANDSHAKE, true, NULL, 0);
            SendSetup(controller, SW_SETUP_FORCE_USB, false, NULL, 0);
        }
    }

    return 0;
}

static int GetCalibrationType(HIDController* controller, uint32_t addr)
{
    uint8_t recBuf[SW_PACKET_MAX_SIZE_IN];

    if (SendReadRequest(controller, addr, SW_CONST_LEN_USER_CALIB_STATUS, recBuf, sizeof recBuf) > 0)
    {
        SCPacketIn_CommandResults* res = (SCPacketIn_CommandResults*)recBuf;
        return res->spi.data[0] != SW_CONST_USER_CALIB_0 || res->spi.data[1] != SW_CONST_USER_CALIB_1 ? CALIB_TYPE_FACTORY : CALIB_TYPE_USER;
    }

    return -1;
}

static void GetCalibration(HIDController* controller, uint32_t addr, bool isLeft, AxisCalibration* outCalib)
{
    uint8_t recBuf[SW_PACKET_MAX_SIZE_IN];

    if (SendReadRequest(controller, addr, SW_CONST_LEN_TRG_CALIB, recBuf, sizeof recBuf) > 0)
    {
        SCPacketIn_CommandResults* res = (SCPacketIn_CommandResults*)recBuf;

        int16_t xHighD;
        int16_t yHighD;
        int16_t xMid;
        int16_t yMid;
        int16_t xLowD;
        int16_t yLowD;

        if (isLeft)
        {
            xHighD = Get12Bits(res->spi.data, 0);
            yHighD = Get12Bits(res->spi.data + 1, 4);
            xMid =   Get12Bits(res->spi.data + 3, 0);
            yMid =   Get12Bits(res->spi.data + 4, 4);
            xLowD =  Get12Bits(res->spi.data + 6, 0);
            yLowD =  Get12Bits(res->spi.data + 7, 4);
        }
        else
        {
            xMid =   Get12Bits(res->spi.data, 0);
            yMid =   Get12Bits(res->spi.data + 1, 4);
            xLowD =  Get12Bits(res->spi.data + 3, 0);
            yLowD =  Get12Bits(res->spi.data + 4, 4);
            xHighD = Get12Bits(res->spi.data + 6, 0);
            yHighD = Get12Bits(res->spi.data + 7, 4);
        }

        outCalib->x.bLo = -xLowD;
        outCalib->x.mid = xMid;
        outCalib->x.bHi = xHighD;

        outCalib->y.bLo = -yLowD;
        outCalib->y.mid = yMid;
        outCalib->y.bHi = yHighD;
    }
}

static void GetTriggerCalibration(HIDController* controller)
{
    SwitchController* con = (SwitchController*)&controller->data;

    // get left
    int leftTrgCalibType = GetCalibrationType(controller, SW_ADDR_CALIB_TRG_USER_LEFT_STATUS);

    if (leftTrgCalibType == CALIB_TYPE_FACTORY)
    {
        GetCalibration(controller, SW_ADDR_CALIB_TRG_FAC_LEFT, true, &con->calib[tL]);
    }
    else if (leftTrgCalibType == CALIB_TYPE_USER)
    {
        GetCalibration(controller, SW_ADDR_CALIB_TRG_USER_LEFT, true, &con->calib[tL]);
    }

    // get right
    int rightTrgCalibType = GetCalibrationType(controller, SW_ADDR_CALIB_TRG_USER_RIGHT_STATUS);

    if (rightTrgCalibType == CALIB_TYPE_FACTORY)
    {
        GetCalibration(controller, SW_ADDR_CALIB_TRG_FAC_RIGHT, false, &con->calib[tR]);
    }
    else if (rightTrgCalibType == CALIB_TYPE_USER)
    {
        GetCalibration(controller, SW_ADDR_CALIB_TRG_USER_RIGHT, false, &con->calib[tR]);
    }

    LOGF(eINFO, "Controller calibration retrieved:\n"
        "  Left Config: %s\n"
        "    X: %i <--- %i ---> %i\n"
        "    Y: %i <--- %i ---> %i\n"
        "  Right Config: %s\n"
        "    X: %i <--- %i ---> %i\n"
        "    Y: %i <--- %i ---> %i\n",
        leftTrgCalibType == CALIB_TYPE_FACTORY ? "Factory" : leftTrgCalibType == CALIB_TYPE_USER ? "User" : "Unknown",
        con->calib[tL].x.mid + con->calib[tL].x.bLo, con->calib[tL].x.mid, con->calib[tL].x.mid + con->calib[tL].x.bHi,
        con->calib[tL].y.mid + con->calib[tL].y.bLo, con->calib[tL].y.mid, con->calib[tL].y.mid + con->calib[tL].y.bHi,
        rightTrgCalibType == CALIB_TYPE_FACTORY ? "Factory" : rightTrgCalibType == CALIB_TYPE_USER ? "User" : "Unknown",
        con->calib[tR].x.mid + con->calib[tR].x.bLo, con->calib[tR].x.mid, con->calib[tR].x.mid + con->calib[tR].x.bHi,
        con->calib[tR].y.mid + con->calib[tR].y.bLo, con->calib[tR].y.mid, con->calib[tR].y.mid + con->calib[tR].y.bHi);
}

int HIDOpenSwitchController(HIDDeviceInfo* devInfo, HIDController* controller, uint8_t playerNum)
{
    hid_device * hidDev = hid_open_path(devInfo->logicalSystemPath);
    if (!hidDev)
    {
        LOGF(LogLevel::eWARNING, "HID Open failed on device %s", devInfo->logicalSystemPath);
        LOGF(LogLevel::eWARNING, "   hid err str: %ls", hid_error(hidDev));
        return -1;
    }

    controller->hidDev = hidDev;
    controller->Update = HIDUpdateSwitchController;
    controller->Close = HIDCloseSwitchController;

    controller->SetPlayer = HIDSetPlayerSwitchCon;
    controller->SetLights = HIDSetLightsSwitchCon;
    controller->DoRumble = HIDDoRumbleSwitchCon;

    SwitchController* con = (SwitchController *)&controller->data;
    memset(con, 0, sizeof *con);

    SetNoRumble(&con->rumble[tL]);
    SetNoRumble(&con->rumble[tR]);

    // Setup device

    switch (devInfo->productID)
    {
    case pidNintendoSwitchJoyConGrip:
        con->isGrip = 1;
        con->isRight = devInfo->interface == 0;
        con->isLeft = devInfo->interface == 1;
        break;
    case pidNintendoSwitchJoyConRight:
        con->isRight = 1;
        break;
    case pidNintendoSwitchJoyConLeft:
        con->isLeft = 1;
        break;
    default:
        break;
    }

    int res = DetermineConnectionType(devInfo, controller);
    if (res < 0)
    {
        if (res == -1)
            LOGF(LogLevel::eINFO, "No valid usb or bluetooth connection state! - %s", devInfo->logicalSystemPath);
        else if (res == -2)
            LOGF(LogLevel::eINFO, "Switch Controller is already connected!");
        HIDCloseSwitchController(controller);
        return -1;
    }

    if (InitializeConnection(controller) < 0)
    {
        LOGF(LogLevel::eINFO, "Failed to initialize connection! - %s", devInfo->logicalSystemPath);
        HIDCloseSwitchController(controller);
        return -1;
    }

    // the right controller doesn't respond to the rest of setup after this when in the grip, but works
    if (con->isRight && con->isGrip)
        return 0;


    // Enable features

    // Activate rumble
    uint8_t rumbleData[] = { 1 };
    if (SendCommand(controller, SW_COMMAND_ENABLE_RUMBLE, rumbleData, sizeof rumbleData, NULL, 0) < 0)
    {
        LOGF(LogLevel::eINFO, "Failed to initialize rumble! - %s", devInfo->logicalSystemPath);
        HIDCloseSwitchController(controller);
        return -1;
    }

    // Enable IMU
    uint8_t imuData[] = { 1 };
    if (SendCommand(controller, SW_COMMAND_ENABLE_IMU, imuData, sizeof imuData, NULL, 0) < 0)
    {
        LOGF(LogLevel::eINFO, "Failed to enable imu! - %s", devInfo->logicalSystemPath);
        HIDCloseSwitchController(controller);
        return -1;
    }

    // Set input packet format
    uint8_t inputFormatData[] = { SW_PACKET_TYPE_IN_FULL_STATE };
    if (SendCommand(controller, SW_COMMAND_SET_INPUT_MODE, inputFormatData, sizeof inputFormatData, NULL, 0) < 0)
    {
        LOGF(LogLevel::eINFO, "Failed to set input format! - %s", devInfo->logicalSystemPath);
        HIDCloseSwitchController(controller);
        return -1;
    }

    if (!con->bluetooth)
    {
        // If USB prompt to continue sending input reports
        SendSetup(controller, SW_SETUP_FORCE_USB, false, NULL, 0);
    }

    SetHomeButtonLightPattern(controller, 100);
    HIDSetPlayerSwitchCon(controller, playerNum);
    GetTriggerCalibration(controller);

    return 0;
}
