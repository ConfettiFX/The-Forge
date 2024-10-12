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

#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/joystick.h>
#include <sys/stat.h>
#include <libudev.h>
#include <dirent.h>

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

#define BITS_PER_LONG             (sizeof(uint64_t) * 8)
#define BITS_TO_LONGS(x)          ((((x)-1) / BITS_PER_LONG) + 1)
#define EVDEV_OFF(x)              ((x) % BITS_PER_LONG)
#define EVDEV_LONG(x)             ((x) / BITS_PER_LONG)
#define TEST_EVDEV_BIT(bit, bits) ((bits[EVDEV_LONG(bit)] >> EVDEV_OFF(bit)) & 1)

#define MAX_RUMBLE_DURATION_MS    0xFFFF

typedef struct GamepadDeviceInfo
{
    char     pName[128];
    /*
     * EV_ABS:
        - Used to describe absolute axis value changes, e.g. describing the
          coordinates of a touch on a touchscreen.
     * EV_FF:
        - Used to send force feedback commands to an input device.
     */
    uint64_t mFfBits[BITS_TO_LONGS(FF_CNT)];
    bool     mHasAbs[ABS_CNT];
    bool     mHasKey[KEY_CNT];

    struct ff_effect mRumbleEffect;
    uint32_t         mFfRumbleEnabled : 1;
    uint32_t         mFfRumbleSineEnabled : 1;

    uint32_t mIsConnected : 1;
    uint32_t mIsBufferOverflowed : 1;
    uint32_t mIsSteamVirtualGamepad : 1;

    // Need mapings for axis.
    InputEnum mAbsMapping[ABS_CNT] = { INPUT_NONE };
    bool      mAbsHasHat[4] = { false };
    bool      mIsDigitalDPad = false;

    uint16_t mVendorID;
    uint16_t mProductID;

    InputPortIndex mPortIdx;
    int32_t        mID;
    int32_t        mEventFileHandle;
} GamepadDeviceInfo;
static GamepadDeviceInfo gGamepadDevices[MAX_GAMEPADS];
static uint32_t          gGamepadDeviceCount = 0;
static bool              gGamepadDevicesAdded = false;
static bool              gGamepadDevicesRemoved = false;

// Mappings created from linux event code file ( 'linux/input-event-codes.h' )
static InputEnum gGamepadBtnMappings[] = { GPAD_A,  GPAD_B,  INPUT_NONE, GPAD_X,    GPAD_Y,     INPUT_NONE, GPAD_L1,
                                           GPAD_R1, GPAD_L2, GPAD_R2,    GPAD_BACK, GPAD_START, INPUT_NONE, GPAD_L3,
                                           GPAD_R3, GPAD_UP, GPAD_DOWN,  GPAD_LEFT, GPAD_RIGHT };

// Normalizes values from 'input_event::value' for axis/trigger type events..
constexpr float gNormThumbstickValueRecip = 1.0f / 32768.0f;
constexpr float gNormTriggerValue_1023_Recip = 1.0f / 1023.0f;
constexpr float gNormTriggerValue_255_Recip = 1.0f / 255.0f;

#define GAMEPAD_DETECT_INTERVAL_MS 3000 // Update every 3 seconds
static Timer gGamepadCheckTimerHandle;

#define USB_VENDOR_MICROSOFT              0x045e
#define USB_VENDOR_VALVE                  0x28de

#define USB_PRODUCT_STEAM_VIRTUAL_GAMEPAD 0x11ff

/************************************************************************/
// Gamepad
/************************************************************************/
static int32_t GamepadHIDFindEmptySlot()
{
    for (uint32_t portIndex = 0; portIndex < MAX_GAMEPADS; ++portIndex)
    {
        if (!inputGamepadIsActive(portIndex))
            return portIndex;
    }

    return -1;
}

static int32_t GamepadDeviceFindEmptySlot()
{
    for (uint32_t portIndex = 0; portIndex < MAX_GAMEPADS; ++portIndex)
    {
        if (gGamepadDevices[portIndex].mID == -1)
            return portIndex;
    }

    return -1;
}

static bool GamepadIsActivated(int32_t gpdID)
{
    for (uint32_t i = 0; i < MAX_GAMEPADS; ++i)
    {
        if (gGamepadDevices[i].mID == gpdID)
        {
            return true;
        }
    }

    return false;
}

InputPortIndex GamepadAddHIDController(const HIDDeviceInfo* pDeviceInfo)
{
    InputPortIndex portIndex = GamepadHIDFindEmptySlot();
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

static bool GamepadIsSupportedVendor(uint16_t vendor)
{
    switch (vendor)
    {
    case USB_VENDOR_VALVE:
    case USB_VENDOR_MICROSOFT:
        return true;
    default:
        return false;
    }
}

static bool IsJoystick(const char* pDevicePath, struct udev* pUdev)
{
    bool        bIsJoystick = false;
    struct stat stats;
    if (stat(pDevicePath, &stats) != -1)
    {
        char                type;
        struct udev_device* pDev;

        if (S_ISBLK(stats.st_mode))
        {
            type = 'b';
        }
        else if (S_ISCHR(stats.st_mode))
        {
            type = 'c';
        }
        else
        {
            return false;
        }

        pDev = udev_device_new_from_devnum(pUdev, type, stats.st_rdev);
        if (pDev)
        {
            const char* subsystem = udev_device_get_subsystem(pDev);
            if (subsystem)
            {
                if (strcmp(subsystem, "input") == 0)
                {
                    // udev rules reference: http://cgit.freedesktop.org/systemd/systemd/tree/src/udev/udev-builtin-input_id.c
                    const char* pVal = udev_device_get_property_value(pDev, "ID_INPUT_JOYSTICK");
                    if (pVal && pVal[0] == '1')
                    {
                        bIsJoystick = true;
                    }
                }
            }

            udev_device_unref(pDev);
        }
    }
    return bIsJoystick;
}

static void GamepadInfoReset(int32_t gpIdx)
{
    GamepadDeviceInfo& gpInfo = gGamepadDevices[gpIdx];
    // Reset game pad information..
    memset(gpInfo.mAbsHasHat, 0, sizeof(gpInfo.mAbsHasHat));
    memset(gpInfo.mHasAbs, 0, sizeof(gpInfo.mHasAbs));
    memset(gpInfo.mHasKey, 0, sizeof(gpInfo.mHasKey));
    memset(gpInfo.mFfBits, 0, sizeof(gpInfo.mFfBits));
    memset(gpInfo.mAbsMapping, INPUT_NONE, sizeof(gpInfo.mAbsMapping));
    gpInfo.mRumbleEffect.id = -1;
    gpInfo.mVendorID = 0;
    gpInfo.mProductID = 0;
    gpInfo.mPortIdx = PORT_INDEX_INVALID;
    gpInfo.mID = -1;
    gpInfo.mIsBufferOverflowed = false;
    gpInfo.mIsSteamVirtualGamepad = false;
    gpInfo.mIsDigitalDPad = false;

    if (gpInfo.mEventFileHandle)
        close(gpInfo.mEventFileHandle);
    gpInfo.mEventFileHandle = -1;
    gpInfo.pName[0] = '\0';
}

static bool GamepadQueryDeviceInfo(int32_t fileHandle, GamepadDeviceInfo& device)
{
    input_id inputIDs = {};

    // Query device info..
    if (ioctl(fileHandle, EVIOCGID, &inputIDs) < 0)
        return false;

    if (!GamepadIsSupportedVendor(inputIDs.vendor))
        return false;

    device.mVendorID = inputIDs.vendor;
    device.mProductID = inputIDs.product;

    if (device.mVendorID == USB_VENDOR_VALVE && device.mProductID == USB_PRODUCT_STEAM_VIRTUAL_GAMEPAD)
    {
        uint32_t numVirtualGamepads = 1;
        for (uint32_t portIndex = 0; portIndex < MAX_GAMEPADS; ++portIndex)
            if (gGamepadDevices[portIndex].mIsSteamVirtualGamepad)
                numVirtualGamepads++;

        device.mIsSteamVirtualGamepad = true;
        snprintf(device.pName, sizeof(device.pName) - 1, "Steam Virtual Gamepad %u", numVirtualGamepads);
    }
    else
    {
        ioctl(fileHandle, EVIOCGNAME(sizeof(device.pName) - 1), device.pName);
    }
    return true;
}

static void GamepadScan()
{
    // Check for game pads every 3 seconds..
    if (getTimerMSec(&gGamepadCheckTimerHandle, false) < GAMEPAD_DETECT_INTERVAL_MS)
        return;
    resetTimer(&gGamepadCheckTimerHandle);

    // Search for device files with "-event-joystick" suffix.
    const char* pInputFileDir = "/dev/input/";
    const char* pInputFileSuffix = "event";
    // Open the search directory.
    DIR*        dir = opendir(pInputFileDir);
    if (dir == nullptr)
    {
#if defined(LINUX_INPUT_LOG_VERBOSE)
        LOGF(eERROR, "Failed to open directory '%s'", inputFileDir);
#endif
        return;
    }

    struct udev*   pUDev = udev_new();
    struct dirent* pDEntry = NULL;
    while ((pDEntry = readdir(dir)) != NULL && gGamepadDeviceCount < MAX_GAMEPADS)
    {
        char filePathToInputFile[512];
        snprintf(filePathToInputFile, sizeof(filePathToInputFile), "%s%s", pInputFileDir, pDEntry->d_name);

        // Skip files without the joystick suffix.
        if (strstr(pDEntry->d_name, pInputFileSuffix) == NULL)
        {
            continue;
        }

        // Check if device is a joystick first...
        if (!IsJoystick(filePathToInputFile, pUDev))
            continue;

        // open the event file..
        // try rw first rumble...
        int32_t fileHandle = open(filePathToInputFile, O_RDWR | O_CLOEXEC | O_NONBLOCK);
        if (fileHandle < 0)
        {
            // try again with Read only after read-write does not work
            if ((fileHandle = open(filePathToInputFile, O_RDONLY | O_CLOEXEC | O_NONBLOCK)) < 0)
                continue;
#if defined(LINUX_INPUT_LOG_VERBOSE)
            LOGF(eERROR, "Failed to open gamepad event file %s with error '%s'", filePathToInputFile, strerror(errno));
#endif
        }

        // Find available gamepad slot
        int32_t deviceHandle = GamepadDeviceFindEmptySlot();
        if (deviceHandle == -1)
        {
#if defined(LINUX_INPUT_LOG_VERBOSE)
            LOGF(eWARNING, "Max gamepads connected (Connected: %u, Max: %u) or an error has occurred.", gGamepadDeviceCount, MAX_GAMEPADS);
#endif
            close(fileHandle);
            break;
        }

        GamepadDeviceInfo& deviceInfo = gGamepadDevices[deviceHandle];
        if (!GamepadQueryDeviceInfo(fileHandle, deviceInfo))
        {
            close(fileHandle);
            continue;
        }

        // TODO::
        int32_t deviceID = tf_mem_hash<char>(filePathToInputFile, strlen(filePathToInputFile));
        if (GamepadIsActivated(deviceID))
        {
            close(fileHandle);
            continue;
        }

        uint64_t absBits[BITS_TO_LONGS(ABS_CNT)] = { 0 };
        uint64_t keyBits[BITS_TO_LONGS(KEY_CNT)] = { 0 };

        int absBitsSuccess = ioctl(fileHandle, EVIOCGBIT(EV_ABS, sizeof(absBits)), absBits) >= 0;
        int keyBitsSuccess = ioctl(fileHandle, EVIOCGBIT(EV_KEY, sizeof(keyBits)), keyBits) >= 0;
        if (!absBitsSuccess || !keyBitsSuccess)
        {
#if defined(LINUX_INPUT_LOG_VERBOSE)
            LOGF(eERROR, "Failed to get gamepad abs/key event bits from: %s. Event file-path: %s", gpInfo.pName, filePathToInputFile);
#endif
            close(fileHandle);
            continue;
        }
        // if it doesn't have 'BTN_GAMEPAD' bit it is not a gamepad according to specs..
#if defined(LINUX_INPUT_LOG_VERBOSE)
        if (!TEST_EVDEV_BIT(BTN_GAMEPAD, absBits))
            LOGF(eWARNING, "%s is not recognized as a gamepad acccording to linux gamepad specs.", gpInfo.pName);
#endif

        if (ioctl(fileHandle, EVIOCGBIT(EV_FF, sizeof(deviceInfo.mFfBits)), deviceInfo.mFfBits) >= 0)
        {
            deviceInfo.mFfRumbleEnabled = TEST_EVDEV_BIT(FF_RUMBLE, deviceInfo.mFfBits);
            deviceInfo.mFfRumbleSineEnabled = TEST_EVDEV_BIT(FF_SINE, deviceInfo.mFfBits);
        }
        else
        {
            deviceInfo.mFfRumbleEnabled = false;
            deviceInfo.mFfRumbleSineEnabled = false;
#if defined(LINUX_INPUT_LOG_VERBOSE)
            LOGF(eWARNING, "Failed to get gamepad force feedback event bits from: %s. Event file-path: %s", gpInfo.pName,
                 filePathToInputFile);
#endif
        }

        deviceInfo.mID = deviceID;
        deviceInfo.mEventFileHandle = fileHandle;
        deviceInfo.mIsConnected = true;

        // Update gamepads information values..
        gGamepadDeviceCount++;
        gGamepadDevicesAdded = true;

        // Set all bits values
        for (int32_t i = ABS_HAT0X; i <= ABS_HAT3Y; ++i)
        {
            int32_t hatIdx = (i - ABS_HAT0X) / 2;
            ASSERT(hatIdx >= 0);
            deviceInfo.mAbsHasHat[hatIdx] = TEST_EVDEV_BIT(i, absBits);
        }

        for (int32_t i = ABS_X; i < ABS_MAX; ++i)
            deviceInfo.mHasAbs[i] = TEST_EVDEV_BIT(i, absBits);

        for (int32_t i = 0; i < KEY_MAX; ++i)
            deviceInfo.mHasKey[i] = TEST_EVDEV_BIT(i, keyBits);

        deviceInfo.mIsDigitalDPad = true;
        if (!(deviceInfo.mHasKey[BTN_DPAD_UP] && deviceInfo.mHasKey[BTN_DPAD_DOWN] && deviceInfo.mHasKey[BTN_DPAD_LEFT] &&
              deviceInfo.mHasKey[BTN_DPAD_RIGHT]))
        {
            deviceInfo.mIsDigitalDPad = false;

            // Positive value mappings..
            deviceInfo.mAbsMapping[ABS_HAT0X] = GPAD_RIGHT;
            deviceInfo.mAbsMapping[ABS_HAT0Y] = GPAD_UP;
        }

        /*
         * Fix mappings for axes..
         * Omitting (Shoulder/D-Pad btns are treated as digital btns..):
         *  - Linux input specs: D-Pad, shoulder buttons and triggers can be digital, analog, or both at the same time.
         * Several conventions for how analog triggers absolute axes:
         *  - Linux Gamepad Specs: LT = ABS_HAT2Y, RT = ABS_HAT2X
         *  - Android, Bluetooth controllers: LT = ABS_BRAKE, RT = ABS_GAS
         *  - Older Xbox and Playstation controllers: LT = ABS_Z, RT = ABS_RZ
         */

        if (TEST_EVDEV_BIT(ABS_HAT2Y, absBits))
            deviceInfo.mAbsMapping[ABS_HAT2Y] = GPAD_L2; // Mapped LTRIGGER to axis ABS_HAT2Y
        else if (TEST_EVDEV_BIT(ABS_BRAKE, absBits))
            deviceInfo.mAbsMapping[ABS_BRAKE] = GPAD_L2; // Mapped LTRIGGER to axis ABS_BRAKE
        else if (TEST_EVDEV_BIT(ABS_Z, absBits))
            deviceInfo.mAbsMapping[ABS_Z] = GPAD_L2; // Mapped LTRIGGER to axis ABS_Z
        else
            LOGF(eWARNING, "LeftTrigger bit not found for %s.", deviceInfo.pName);

        if (TEST_EVDEV_BIT(ABS_HAT2X, absBits))
            deviceInfo.mAbsMapping[ABS_HAT2X] = GPAD_R2; // Mapped RTRIGGER to axis ABS_HAT2X
        else if (TEST_EVDEV_BIT(ABS_GAS, absBits))
            deviceInfo.mAbsMapping[ABS_GAS] = GPAD_R2; // Mapped RTRIGGER to axis ABS_GAS
        else if (TEST_EVDEV_BIT(ABS_RZ, absBits))
            deviceInfo.mAbsMapping[ABS_RZ] = GPAD_R2; // Mapped RTRIGGER to axis ABS_RZ
        else
            LOGF(eWARNING, "RightTrigger bit not found for %s.", deviceInfo.pName);

        if (TEST_EVDEV_BIT(ABS_X, absBits) && TEST_EVDEV_BIT(ABS_Y, absBits))
        {
            deviceInfo.mAbsMapping[ABS_X] = GPAD_LX; // Mapped LTHUMBSTICK_X to axis ABS_X
            deviceInfo.mAbsMapping[ABS_Y] = GPAD_LY; // Mapped LTHUMBSTICK_Y to axis ABS_Y
        }
        else
        {
            LOGF(eWARNING, "LeftThumbstick (X/Y axis) bit not found for %s.", deviceInfo.pName);
        }

        /* Linux Spec. uses the RX and RY axes. Common for USB gamepads, and also many Bluetooth gamepads (older ones).
         * Android convention uses the Z axis as X axis, and RZ as Y axis.
         */
        if (TEST_EVDEV_BIT(ABS_RX, absBits) && TEST_EVDEV_BIT(ABS_RY, absBits))
        {
            deviceInfo.mAbsMapping[ABS_RX] = GPAD_RX; // Mapped RTHUMBSTICK_X to axis ABS_RX
            deviceInfo.mAbsMapping[ABS_RY] = GPAD_RY; // Mapped RTHUMBSTICK_Y to axis ABS_RY
        }
        else if (TEST_EVDEV_BIT(ABS_Z, absBits) && TEST_EVDEV_BIT(ABS_RZ, absBits))
        {
            deviceInfo.mAbsMapping[ABS_Z] = GPAD_RX;  // Mapped RTHUMBSTICK_X to axis ABS_Z
            deviceInfo.mAbsMapping[ABS_RZ] = GPAD_RY; // Mapped RTHUMBSTICK_Y to axis ABS_RZ
        }
        else
        {
            LOGF(eWARNING, "RightThumbstick (X/Y axis) bit not found for %s.", deviceInfo.pName);
        }

        LOGF(eINFO, "%s detected:   0x%.4x - 0x%.4x   Path: %s", deviceInfo.pName, deviceInfo.mVendorID, deviceInfo.mProductID,
             filePathToInputFile);
    }

    udev_unref(pUDev);
    closedir(dir);
}

static void GamepadRefresh()
{
    // Remove connected pads
    if (gGamepadDevicesRemoved)
    {
        for (uint32_t i = 0; i < MAX_GAMEPADS; ++i)
        {
            GamepadDeviceInfo& gpInfo = gGamepadDevices[i];
            if (!gpInfo.mIsConnected && gpInfo.mPortIdx != PORT_INDEX_INVALID)
            {
                // Disconnect this pad..
                gGamepads[gpInfo.mPortIdx].mActive = false;
                GamepadResetState(gpInfo.mPortIdx);
                if (gGamepadRemovedCb)
                    gGamepadRemovedCb(gpInfo.mPortIdx);

                gGamepadDeviceCount--;
                LOGF(eINFO, "%s Has been removed.", gpInfo.pName);
                GamepadInfoReset(i);
            }
        }

        gGamepadDevicesRemoved = false;
    }

    // Add connected pads
    if (gGamepadDevicesAdded)
    {
        for (uint32_t i = 0; i < MAX_GAMEPADS; ++i)
        {
            GamepadDeviceInfo& gpInfo = gGamepadDevices[i];

            // Enable all new pads
            if (gpInfo.mIsConnected && gpInfo.mPortIdx == PORT_INDEX_INVALID)
            {
                InputPortIndex ipHandle = PORT_INDEX_INVALID;
                for (uint32_t pi = 0; pi < MAX_GAMEPADS; ++pi)
                {
                    if (!inputGamepadIsActive(pi))
                    {
                        ipHandle = pi;
                        break;
                    }
                }
                if (ipHandle == PORT_INDEX_INVALID)
                {
                    LOGF(eERROR, "Input Port Index is invalid while gamepad device has inactive gamepads connected...");
                    ASSERT(false);
                }

                gpInfo.mPortIdx = ipHandle;
                // Update gamepad data
                Gamepad& gpad = gGamepads[ipHandle];
                gpad.mActive = true;
                gpad.pName = gpInfo.pName;
                if (gGamepadAddedCb)
                    gGamepadAddedCb(ipHandle);
                LOGF(eINFO, "Gamepad %s connected.", gpInfo.pName);
            }
        }

        gGamepadDevicesAdded = false;
    }
}

static void GamepadPollAll(GamepadDeviceInfo& gDeviceInfo, Gamepad& gPad)
{
    struct input_absinfo absInfo;
    constexpr float      nAxisValueRecip = 1.0f / 32768.0f;

    // Poll all axis
    for (int32_t i = ABS_X; i < ABS_MAX; ++i)
    {
        if (gDeviceInfo.mHasAbs[i])
        {
            int32_t axisIdx = gDeviceInfo.mAbsMapping[i] - GPAD_AXIS_FIRST;
            if (axisIdx > 0 && ioctl(gDeviceInfo.mEventFileHandle, EVIOCGABS(i), &absInfo) >= 0)
            {
                float normalizedAxisValue = absInfo.value * nAxisValueRecip;

                gPad.mAxis[axisIdx] = normalizedAxisValue;
                GamepadProcessStick(gDeviceInfo.mPortIdx, gDeviceInfo.mAbsMapping[i]);
            }
        }
    }

    // Poll all triggers (Digital Hats)
    for (int32_t i = ABS_HAT0X; i <= ABS_HAT3Y; ++i)
    {
        const int32_t hatIdx = (i - ABS_HAT0X) / 2;
        // We don't need to test for analog axes here, they won't have has_hat[] set
        if (gDeviceInfo.mAbsHasHat[hatIdx])
        {
            int32_t axisIdx = gDeviceInfo.mAbsMapping[i] - GPAD_AXIS_FIRST;
            if (axisIdx > 0 && ioctl(gDeviceInfo.mEventFileHandle, EVIOCGABS(i), &absInfo) >= 0)
            {
                float normalizedAxisValue = absInfo.value * nAxisValueRecip;

                gPad.mAxis[axisIdx] = normalizedAxisValue;
                GamepadProcessTrigger(gDeviceInfo.mPortIdx, gDeviceInfo.mAbsMapping[i]);
            }
        }
    }

    // Poll all buttons
    uint64_t keyInfo[BITS_TO_LONGS(KEY_CNT)] = { 0 };
    if (ioctl(gDeviceInfo.mEventFileHandle, EVIOCGKEY(sizeof(keyInfo)), keyInfo) >= 0)
    {
        for (int32_t i = 0; i < KEY_MAX + 1; i++)
        {
            if (gDeviceInfo.mHasKey[i])
            {
                const uint8_t bIsPressed = TEST_EVDEV_BIT(i, keyInfo) ? 1 : 0;
                int32_t       mappedEventCode = -1;

                // X, Y, A, B btns..
                if (i >= BTN_A && i <= BTN_THUMBR)
                    mappedEventCode = i - BTN_A;
                // DPAD
                else if (i >= BTN_DPAD_UP && i <= BTN_DPAD_RIGHT)
                    mappedEventCode = i - BTN_DPAD_UP + 15; // Skip non-DPAD buttons..

                if (mappedEventCode >= 0)
                {
                    InputEnum btn = gGamepadBtnMappings[mappedEventCode];
                    if (btn != INPUT_NONE)
                        gPad.mButtons[btn - GPAD_BTN_FIRST] = bIsPressed;
                }
            }
        }
    }
}

/************************************************************************/
// Gamepad state
/************************************************************************/
#if defined(LINUX_INPUT_LOG_VERBOSE)
static const char* GetInputEnumStr(InputEnum icode)
{
    if (icode == GPAD_A)
        return "GPAD_A";
    else if (icode == GPAD_B)
        return "GPAD_B";
    else if (icode == GPAD_X)
        return "GPAD_X";
    else if (icode == GPAD_Y)
        return "GPAD_Y";
    else if (icode == GPAD_L1)
        return "GPAD_L1";
    else if (icode == GPAD_R1)
        return "GPAD_R1";
    else if (icode == GPAD_L2)
        return "GPAD_L2";
    else if (icode == GPAD_R2)
        return "GPAD_R2";
    else if (icode == GPAD_BACK)
        return "GPAD_BACK";
    else if (icode == GPAD_START)
        return "GPAD_START";
    else if (icode == INPUT_NONE)
        return "INPUT_NONE";
    else if (icode == GPAD_L3)
        return "GPAD_L3";
    else if (icode == GPAD_R3)
        return "GPAD_R3";
    else if (icode == GPAD_UP)
        return "GPAD_UP";
    else if (icode == GPAD_DOWN)
        return "GPAD_DOWN";
    else if (icode == GPAD_LEFT)
        return "GPAD_LEFT";
    else if (icode == GPAD_RIGHT)
        return "GPAD_RIGHT";
    else if (icode == GPAD_LX)
        return "GPAD_LX";
    else if (icode == GPAD_LY)
        return "GPAD_LY";
    else if (icode == GPAD_RX)
        return "GPAD_RX";
    else if (icode == GPAD_RY)
        return "GPAD_RY";
    else if (icode == GPAD_L2)
        return "GPAD_L2";
    else if (icode == GPAD_R2)
        return "GPAD_R2";
    else
        return "INPUT_NONE";
}
static const char* GetLinuxInputKeyStr(int32_t ecode)
{
    if (ecode == 0x130)
        return "BTN_A";
    else if (ecode == 0x131)
        return "BTN_B";
    else if (ecode == 0x132)
        return "BTN_C";
    else if (ecode == 0x133)
        return "BTN_X";
    else if (ecode == 0x134)
        return "BTN_Y";
    else if (ecode == 0x135)
        return "BTN_Z";
    else if (ecode == 0x136)
        return "BTN_TL";
    else if (ecode == 0x137)
        return "BTN_TR";
    else if (ecode == 0x138)
        return "BTN_TL2";
    else if (ecode == 0x139)
        return "BTN_TR2";
    else if (ecode == 0x13a)
        return "BTN_SELECT";
    else if (ecode == 0x13b)
        return "BTN_START";
    else if (ecode == 0x13c)
        return "BTN_MODE";
    else if (ecode == 0x13d)
        return "BTN_THUMBL";
    else if (ecode == 0x13e)
        return "BTN_THUMBR";
    else if (ecode == 0x220) // BTN_DPAD_UP
        return "BTN_DPAD_UP";
    else if (ecode == 0x221) // BTN_DPAD_DOWN
        return "BTN_DPAD_DOWN";
    else if (ecode == 0x222) // BTN_DPAD_LEFT
        return "BTN_DPAD_LEFT";
    else if (ecode == 0x223) // BTN_DPAD_RIGHT
        return "BTN_DPAD_RIGHT";
    else
        return "BTN_NONE";
}
#endif

static float GamepadCorrectAxisValue(const GamepadDeviceInfo& gDeviceInfo, int32_t value)
{
    float fval = (float)value;
    switch (gDeviceInfo.mVendorID)
    {
    case USB_VENDOR_MICROSOFT:
    case USB_VENDOR_VALVE:
        return fval * gNormThumbstickValueRecip;
    default:
        return 0.0f;
    };
}
static float GamepadCorrectTriggerValue(const GamepadDeviceInfo& gDeviceInfo, int32_t value)
{
    float fval = (float)value;
    switch (gDeviceInfo.mVendorID)
    {
    case USB_VENDOR_MICROSOFT:
        return fval * gNormTriggerValue_1023_Recip;
    case USB_VENDOR_VALVE:
        return fval * gNormTriggerValue_255_Recip;
    default:
        return 0.0f;
    };
}
static void GamepadUpdateButtonState(const GamepadDeviceInfo& gDeviceInfo, Gamepad& gPad, struct input_event* pInputEvent)
{
    int32_t mappedEventCode = -1;
    int32_t ecode = pInputEvent->code;

    if (ecode >= BTN_A && ecode <= BTN_THUMBR)
        mappedEventCode = ecode - BTN_A;
    else if (ecode >= BTN_DPAD_UP && ecode <= BTN_DPAD_RIGHT)
        mappedEventCode = ecode - BTN_DPAD_UP + 15; // Skip non-DPAD buttons..

    // Unknown button
    if (mappedEventCode < 0)
        return;

    InputEnum mappedECodeToEnum = gGamepadBtnMappings[mappedEventCode];
    if (mappedECodeToEnum != INPUT_NONE)
        gPad.mButtons[mappedECodeToEnum - GPAD_BTN_FIRST] = pInputEvent->value;

#if defined(LINUX_INPUT_LOG_VERBOSE)
    LOGF(eINFO, "%s button event: [ecode: %i, %s] -> [mappedECode: %i, %s], [val: %i]", gDeviceInfo.pName, ecode,
         GetLinuxInputKeyStr(ecode), mappedECodeToEnum, GetInputEnumStr(mappedECodeToEnum), pInputEvent->value);
#endif
}
static void GamepadUpdateAxisState(const GamepadDeviceInfo& gDeviceInfo, Gamepad& gPad, struct input_event* pInputEvent)
{
    int32_t   ecode = pInputEvent->code;
    InputEnum mappedAbsCodeToEnum = gDeviceInfo.mAbsMapping[ecode];
    if (mappedAbsCodeToEnum == INPUT_NONE)
        return;

    int32_t axisIdx = mappedAbsCodeToEnum - GPAD_AXIS_FIRST;
    switch (ecode)
    {
    case ABS_HAT0X:
    {
        if (!gDeviceInfo.mIsDigitalDPad)
        {
            gPad.mButtons[GPAD_RIGHT - GPAD_BTN_FIRST] = pInputEvent->value > 0;
            gPad.mButtons[GPAD_LEFT - GPAD_BTN_FIRST] = pInputEvent->value < 0;

            break;
        }
    }
    case ABS_HAT0Y:
    {
        if (!gDeviceInfo.mIsDigitalDPad)
        {
            // Values are inverted. So condition checks are also inverted..
            gPad.mButtons[GPAD_UP - GPAD_BTN_FIRST] = pInputEvent->value < 0;
            gPad.mButtons[GPAD_DOWN - GPAD_BTN_FIRST] = pInputEvent->value > 0;

            break;
        }
    }
    case ABS_HAT1X:
    case ABS_HAT1Y:
    case ABS_HAT2X:
    case ABS_HAT2Y:
    case ABS_HAT3X:
    case ABS_HAT3Y:
    {
        // Triggers..
        uint32_t hatIdx = (ecode - ABS_HAT0X) / 2;
        if (gDeviceInfo.mAbsHasHat[hatIdx] && axisIdx >= 0)
        {
            gPad.mAxis[axisIdx] = GamepadCorrectTriggerValue(gDeviceInfo, pInputEvent->value);
            GamepadProcessTrigger(gDeviceInfo.mPortIdx, mappedAbsCodeToEnum);
#if defined(LINUX_INPUT_LOG_VERBOSE)
            LOGF(eINFO, "%s TriggerHat 0x%.2x (%s): [%d -> %f]", gDeviceInfo.pName, ecode, GetInputEnumStr(mappedAbsCodeToEnum),
                 pInputEvent->value, gPad.mAxis[axisIdx]);
#endif
        }
        break;
    }
    default:
    {
        if (axisIdx >= 0)
        {
            if (ecode == ABS_Z || ecode == ABS_RZ)
            {
                // LT/RT Trigger..
                gPad.mAxis[axisIdx] = GamepadCorrectTriggerValue(gDeviceInfo, pInputEvent->value);
                GamepadProcessTrigger(gDeviceInfo.mPortIdx, mappedAbsCodeToEnum);
#if defined(LINUX_INPUT_LOG_VERBOSE)
                LOGF(eINFO, "%s Trigger 0x%.2x (%s): [%d -> %f]", gDeviceInfo.pName, ecode, GetInputEnumStr(mappedAbsCodeToEnum),
                     pInputEvent->value, gPad.mAxis[axisIdx]);
#endif
            }
            else
            {
                // Thumb Sticks..
                // Inverted Y axes..
                if (mappedAbsCodeToEnum == GPAD_LY || mappedAbsCodeToEnum == GPAD_RY)
                    gPad.mAxis[axisIdx] = -GamepadCorrectAxisValue(gDeviceInfo, pInputEvent->value);
                else
                    gPad.mAxis[axisIdx] = GamepadCorrectAxisValue(gDeviceInfo, pInputEvent->value);

                GamepadProcessStick(gDeviceInfo.mPortIdx, mappedAbsCodeToEnum);
#if defined(LINUX_INPUT_LOG_VERBOSE)
                LOGF(eINFO, "%s Thumbstick 0x%.2x (%s): [%d -> %f]", gDeviceInfo.pName, ecode, GetInputEnumStr(mappedAbsCodeToEnum),
                     pInputEvent->value, gPad.mAxis[axisIdx]);
#endif
            }
        }
        break;
    }
    }
}

static void GamepadUpdateState(int32_t gpdIdx)
{
    GamepadDeviceInfo& gDeviceInfo = gGamepadDevices[gpdIdx];
    if (!gDeviceInfo.mIsConnected || !inputGamepadIsActive(gDeviceInfo.mPortIdx))
        return;

    Gamepad& gPad = gGamepads[gDeviceInfo.mPortIdx];
    GamepadUpdateLastState(gDeviceInfo.mPortIdx);

    // Reset errors
    errno = 0;

    struct input_event ievents[32];
    int32_t            ieventsLen;
    while ((ieventsLen = read(gDeviceInfo.mEventFileHandle, ievents, sizeof(ievents))) > 0)
    {
        ieventsLen /= sizeof(ievents[0]);
        for (int32_t i = 0; i < ieventsLen; ++i)
        {
            struct input_event* ievent = &ievents[i];
            int32_t             ecode = ievent->code;

            /* If the kernel sent a SYN_DROPPED, we are supposed to ignore the
               rest of the packet (the end of it signified by a SYN_REPORT) */
            if (gDeviceInfo.mIsBufferOverflowed && ((ievent->type != EV_SYN) || (ecode != SYN_REPORT)))
                continue;

            switch (ievent->type)
            {
            case EV_KEY:
            {
                GamepadUpdateButtonState(gDeviceInfo, gPad, ievent);
                break;
            }
            case EV_ABS:
            {
                GamepadUpdateAxisState(gDeviceInfo, gPad, ievent);
                break;
            }
            case EV_SYN:
            {
                switch (ecode)
                {
                case SYN_DROPPED:
                    gDeviceInfo.mIsBufferOverflowed = true;
#if defined(LINUX_INPUT_LOG_VERBOSE)
                    LOGF(eINFO, "%s: SYN_DROPPED detected...", gDeviceInfo.pName);
#endif
                    break;
                case SYN_REPORT:
                    if (gDeviceInfo.mIsBufferOverflowed)
                    {
                        // sync up to current state now
                        GamepadPollAll(gDeviceInfo, gPad);
                        gDeviceInfo.mIsBufferOverflowed = false;
                    }
                    break;
                default:
                    break;
                }
                break;
            }
            default:
                break;
            }
        }
    }

    bool stopRumble = false;

    // Detect if current controller was forcibly disconnected by user..
    if (errno == ENODEV)
    {
        gGamepadDevicesRemoved = true;
        gDeviceInfo.mIsConnected = false;

        // Forcibly stop rumbling as controller disconnected..
        stopRumble = true;
        LOGF(eINFO, "%s has been disconnected by user.", gDeviceInfo.pName);
    }

    if (gPad.mRumbleHigh == 0.0f && gPad.mRumbleLow == 0.0f)
    {
        stopRumble = true;
    }
    // Dont keep setting zero rumble
    if (!gPad.mRumbleStopped && gDeviceInfo.mFfRumbleEnabled)
    {
        struct ff_effect effect = {};
        effect.type = FF_RUMBLE;
        effect.replay.length = MAX_RUMBLE_DURATION_MS;
        effect.u.rumble.strong_magnitude = (uint16_t)(gPad.mRumbleLow * UINT16_MAX);
        effect.u.rumble.weak_magnitude = (uint16_t)(gPad.mRumbleHigh * UINT16_MAX);

        gDeviceInfo.mRumbleEffect = effect;
        if (ioctl(gDeviceInfo.mEventFileHandle, EVIOCSFF, &gDeviceInfo.mRumbleEffect) < 0)
        {
            // The kernel may have lost this effect, try to allocate a new one
            gDeviceInfo.mRumbleEffect.id = -1;
            if (ioctl(gDeviceInfo.mEventFileHandle, EVIOCSFF, &gDeviceInfo.mRumbleEffect) < 0)
            {
#if defined(LINUX_INPUT_LOG_VERBOSE)
                LOGF(eERROR, "Couldn't update/set rumble effect for controller '%s' with error '%s'", gDeviceInfo.pName, strerror(errno));
#endif
            }
        }

        struct input_event ievent = {};
        ievent.type = EV_FF;
        ievent.code = gDeviceInfo.mRumbleEffect.id;
        ievent.value = 1;
        if (write(gDeviceInfo.mEventFileHandle, &ievent, sizeof(ievent)) < 0)
        {
#if defined(LINUX_INPUT_LOG_VERBOSE)
            LOGF(eERROR, "Couldn't start rumble effect for controller '%s'. Maybe file could not open as read-write: %s", gDeviceInfo.pName,
                 strerror(errno));
#endif
        }
    }
    gPad.mRumbleStopped = stopRumble;
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

    for (uint32_t i = 0; i < MAX_GAMEPADS; ++i)
        GamepadInfoReset(i);
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
    GamepadScan();
    GamepadRefresh();
    for (uint32_t i = 0; i < MAX_GAMEPADS; ++i)
        GamepadUpdateState(i);

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