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

#pragma once

#include "../../../Utilities/Interfaces/ITime.h"
#include "../../../Utilities/Interfaces/ILog.h"

#include "../../../OS/ThirdParty/OpenSource/hidapi/hidapi.h"

#if defined(_WINDOWS)
#undef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#undef STRICT
#define STRICT
#undef UNICODE
#define UNICODE 1
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x501 // for raw input
#include <windows.h>
#include <Dbt.h>
#include <initguid.h>
DEFINE_GUID(GUID_DEVINTERFACE_USB_DEVICE, 0xA5DCBF10L, 0x6530, 0x11D2, 0x90, 0x1F, 0x00, 0xC0, 0x4F, 0xB9, 0x51, 0xED);
#elif defined(__linux__)
#include <libudev.h>
#include <poll.h>
#include <math.h>
#endif

// While the standard says this is the max packet size, there is no enforcement
//   so some products may have larger packets
#define HID_DEFAULT_PACKET_SIZE 64

typedef struct hid_device_ hid_device;

// --- Device Tracker ---------------------------------------------------------

// NOTE: see if this cap can be lowered
#define MAX_PATH_LENGTH 228

typedef struct HIDDeviceInfo
{
    // Identifiers needed for uniquely identifying a connected device
    uint16_t mVendorID;
    uint16_t mProductID;
    int32_t  mInterface; // used by the Switch Grip
    uint64_t mMac;

    uint8_t mActive : 1;
    uint8_t mWasActive : 1;
    uint8_t mOpen : 1;
    uint8_t mId : 5;
    uint8_t mType : 5;

    char mName[MAX_PATH_LENGTH];

    union
    {
        // Needed to distinguish when multiple controllers of the same type are added.
        // Also for when one physical device has multiple logical devices
        char           mLogicalSystemPath[MAX_PATH_LENGTH];
        // used for storing a linked list of free indicies
        HIDDeviceInfo* pNext;
    };
} HIDDeviceInfo;
// Not a strict req, just to keep size in check
COMPILE_ASSERT(sizeof(HIDDeviceInfo) < 512);

// --- Opaque Controller ------------------------------------------------------

struct HIDController;

typedef int (*fnHIDUpdate)(HIDController*);
typedef void (*fnHIDClose)(HIDController*);
typedef void (*fnHIDSetLights)(HIDController*, uint8_t, uint8_t, uint8_t);
typedef void (*fnHIDDoRumble)(HIDController*, uint16_t, uint16_t);

#define HID_CONTROLLER_BUFFER_SIZE 60

struct HIDController
{
    hid_device*    pDevice;
    fnHIDUpdate    Update;
    fnHIDClose     Close;
    fnHIDSetLights SetLights;
    fnHIDDoRumble  DoRumble;
    uint8_t        mData[HID_CONTROLLER_BUFFER_SIZE];
    InputPortIndex mPortIndex;
};

enum UsagePage
{
    USAGE_PAGE_UNDEF = 0x0000,
    USAGE_PAGE_DESKTOP = 0x0001,
};

enum Usage
{
    USAGE_NONE = 0x0000,
    USAGE_POINTER = 0x0001,
    USAGE_MOUSE = 0x0002,
    // ?
    USAGE_JOYSTICK = 0x0004,
    USAGE_GPAD = 0x0005,
    USAGE_KEYBOARD = 0x0006,
    USAGE_KPAD = 0x0007,
    USAGE_MULTI_AXIS_CONTROLLER = 0x0008,
};

// Can be found at: https://www.usb.org/sites/default/files/vendor_ids111121_0.pdf
enum Vendor
{
    HID_VENDOR_MICROSOFT = 0x045E,
    HID_VENDOR_NINTENDO = 0x057E,
    HID_VENDOR_SONY = 0x054C,
    HID_VENDOR_VALVE = 0x28DE,
};

// Can be found at: http://www.linux-usb.org/usb.ids
enum ProductID
{
    HID_PID_SWITCH_PRO = 0x2009,
    HID_PID_SWITCH_JOYCON_LEFT = 0x2006,
    HID_PID_SWITCH_JOYCON_RIGHT = 0x2007,
    HID_PID_SWITCH_JOYCON_DUAL = 0x200E,

    HID_PID_DS4 = 0x05C4,
    HID_PID_DS4_DONGLE = 0x0BA0,
    HID_PID_DS4_SLIM = 0x09CC,
    HID_PID_DS5 = 0x0CE6,
};

enum ControllerType : uint8_t
{
    CONTROLLER_TYPE_NONE = 0,
    CONTROLLER_TYPE_PS4,
    CONTROLLER_TYPE_PS5,
    CONTROLLER_TYPE_SWITCH_PRO,
    CONTROLLER_TYPE_SWITCH_JOYCON_DUAL,
    CONTROLLER_TYPE_COUNT,
};
// if this is exceeded the bitfield HIDDeviceInfo::type needs to be expanded
COMPILE_ASSERT(CONTROLLER_TYPE_COUNT - 1 <= 0x1F);

/************************************************************************/
// HID interface used by platform for init, exit, update, ...
/************************************************************************/
// Initialize the HID driver, controller parsers, and request the system
//   for prompts when devices are fully available (after driver installation,
//   not merely after being connected)
int  hidInit(const struct WindowHandle* window);
// Clean up everything initialized above
int  hidExit();
// Used to prompt the HID subsystem to fetch the input state for each HID
// device and apply changes
void hidUpdate();

#if defined(_WINDOWS)
// Used to detect if there is system message indicating a device has been
//   installed or removed
void hidHandleMessage(const MSG* msg);
#endif

/************************************************************************/
// Implementation
/************************************************************************/
/************************************************************************/
// Data
/************************************************************************/
static Timer gHIDTimer;
static bool  gInitialized = false;
/************************************************************************/
// Notifications
/************************************************************************/
static bool  gNotifierInitialized = false;
#if defined(_WINDOWS)
static HDEVNOTIFY gNotifier;
#elif defined(__linux__)
static struct udev*         pUDev;
static struct udev_monitor* pUDevMonitor;
static int                  gUDevFD;
#endif
// backup if system has no notification system or fails to initialize
static Timer    gForceRecheckTimer;
static uint32_t gLastCheck = 0;
/************************************************************************/
// Device Tracking
/************************************************************************/
// This is used for initializing the device tracker
// The tracker uses a linked list for ease of addition and removal, but the
//   data is stored in an array for faster memory loads and adjacency
//
// NOTE: One peripheral can have multiple HID devices. This is set to 20 since
//   most programs won't go over 16 local players, if even get close, with some
//   leeway if multiple devices are connected while swtiching controllers.
//   If the target controllers being used have multiple logical devices,
//   though, this may need to be increased.
#define MAX_DEVICES_TRACKED 20
COMPILE_ASSERT(MAX_DEVICES_TRACKED <= 0x1F); // HIDDeviceInfo::id field size

static HIDDeviceInfo  gDeviceBuffer[MAX_DEVICES_TRACKED];
static HIDDeviceInfo* pFreeList = nullptr;
static uint32_t       gActiveDevices = 0;
// This is equal to the max number of devices connnected over the life of the program
static uint32_t       gActiveSlots = 0;
static bool           gDevicesChanged = true;

// --- Controllers
static HIDController gControllerBuffer[MAX_DEVICES_TRACKED];
/************************************************************************/
// Controller
/************************************************************************/
#define CONCAT_VID_PID(vid, pid) (((uint32_t)vid << 16) + (uint32_t)pid)

struct WhitelistedItem
{
    uint32_t       mUid;
    ControllerType mType;
};

static WhitelistedItem gWhitelistedControllerIDs[] = {
    // Playstation
    { CONCAT_VID_PID(HID_VENDOR_SONY, HID_PID_DS4), CONTROLLER_TYPE_PS4 },
    { CONCAT_VID_PID(HID_VENDOR_SONY, HID_PID_DS4_SLIM), CONTROLLER_TYPE_PS4 },
    { CONCAT_VID_PID(HID_VENDOR_SONY, HID_PID_DS5), CONTROLLER_TYPE_PS5 },
    // Switch
    { CONCAT_VID_PID(HID_VENDOR_NINTENDO, HID_PID_SWITCH_PRO), CONTROLLER_TYPE_SWITCH_PRO },
    { CONCAT_VID_PID(HID_VENDOR_NINTENDO, HID_PID_SWITCH_JOYCON_DUAL), CONTROLLER_TYPE_SWITCH_JOYCON_DUAL },
};

static bool HIDIsController(hid_device_info* dev)
{
    if (dev->vendor_id != HID_VENDOR_VALVE)
    {
        if (dev->usage_page > USAGE_PAGE_DESKTOP)
        {
            return false;
        }

        switch (dev->usage)
        {
        case USAGE_NONE:
        case USAGE_JOYSTICK:
        case USAGE_GPAD:
        case USAGE_MULTI_AXIS_CONTROLLER:
            return true;

        default:
            // filter out all the devices we do not want
            return false;
        }
    }

    return false;
}

static bool HIDIsSupported(hid_device_info* dev, uint8_t* outType)
{
    uint32_t id = CONCAT_VID_PID(dev->vendor_id, dev->product_id);
    for (uint32_t it = 0; it < TF_ARRAY_COUNT(gWhitelistedControllerIDs); ++it)
    {
        if (gWhitelistedControllerIDs[it].mUid == id)
        {
            if (outType)
            {
                *outType = gWhitelistedControllerIDs[it].mType;
            }
            return true;
        }
    }
    return false;
}

// static bool HIDIsSupported(uint16_t vendor, uint16_t product)
//{
//    uint32_t id = CONCAT_VID_PID(vendor, product);
//    for (uint32_t it = 0; it < TF_ARRAY_COUNT(gWhitelistedControllerIDs); ++it)
//    {
//        if (gWhitelistedControllerIDs[it].mUid == id)
//        {
//            return true;
//        }
//    }
//    return false;
//}

static inline void UnsupportedControllerDebug(HIDDeviceInfo* dev, char const* brand)
{
    LOGF(LogLevel::eDEBUG, "Unsupported %s controller:   0x%.4X - 0x%.4X   Path: %s", brand, dev->mVendorID, dev->mProductID,
         dev->mLogicalSystemPath);
}

static inline void FailedToOpenDebug(HIDDeviceInfo* dev, char const* brand)
{
    LOGF(LogLevel::eDEBUG, "Failed to open %s controller:   0x%.4X - 0x%.4X   Path: %s", brand, dev->mVendorID, dev->mProductID,
         dev->mLogicalSystemPath);
}

static inline void SuccessfulOpenDebug(HIDDeviceInfo* dev, char const* brand)
{
    LOGF(LogLevel::eDEBUG, "Successfully opened %s controller:   0x%.4X - 0x%.4X   Path: %s", brand, dev->mVendorID, dev->mProductID,
         dev->mLogicalSystemPath);
}

static void HIDLoadController(uint8_t devID)
{
    // #TODO: Check the isSupported functions - Looks like they are redundant
    extern bool HIDIsSupportedPS4Controller(HIDDeviceInfo * devInfo);
    extern bool HIDIsSupportedPS5Controller(HIDDeviceInfo * devInfo);
    extern bool HIDIsSupportedSwitchController(HIDDeviceInfo * devInfo);
    extern int  HIDOpenPS4Controller(HIDDeviceInfo * devInfo, HIDController * controller);
    extern int  HIDOpenPS5Controller(HIDDeviceInfo * devInfo, HIDController * controller);
    extern int  HIDOpenSwitchController(HIDDeviceInfo * devInfo, HIDController * controller);

    ASSERT(devID > 0 && devID <= MAX_DEVICES_TRACKED);
    uint8_t index = devID - 1;

    if (!gDeviceBuffer[index].mActive)
    {
        LOGF(LogLevel::eWARNING, "Tried to load non-existant device at index %i", index);
        return;
    }

    static const char* controllerTypeStr[] = {
        "NONE", "PS4 Controller", "PS5 Controller", "Switch Pro Controller", "Switch Joycon Dual",
    };
    COMPILE_ASSERT(TF_ARRAY_COUNT(controllerTypeStr) == CONTROLLER_TYPE_COUNT);

    HIDDeviceInfo* dev = gDeviceBuffer + index;
    HIDController* con = gControllerBuffer + index;
    ASSERT(dev->mType < TF_ARRAY_COUNT(controllerTypeStr));
    const char* name = controllerTypeStr[dev->mType];

    switch (dev->mType)
    {
    case CONTROLLER_TYPE_PS4:
    {
        if (!HIDIsSupportedPS4Controller(dev))
        {
            UnsupportedControllerDebug(dev, name);
            return;
        }
        if (HIDOpenPS4Controller(dev, con) == -1)
        {
            FailedToOpenDebug(dev, name);
            return;
        }
        SuccessfulOpenDebug(dev, name);
        break;
    }
    case CONTROLLER_TYPE_PS5:
    {
        if (!HIDIsSupportedPS5Controller(dev))
        {
            UnsupportedControllerDebug(dev, name);
            return;
        }
        if (HIDOpenPS5Controller(dev, con) == -1)
        {
            FailedToOpenDebug(dev, name);
            return;
        }
        SuccessfulOpenDebug(dev, name);
        break;
    }
    case CONTROLLER_TYPE_SWITCH_PRO:
    case CONTROLLER_TYPE_SWITCH_JOYCON_DUAL:
    {
        if (!HIDIsSupportedSwitchController(dev))
        {
            UnsupportedControllerDebug(dev, name);
            return;
        }
        if (HIDOpenSwitchController(dev, con) == -1)
        {
            FailedToOpenDebug(dev, name);
            return;
        }
        SuccessfulOpenDebug(dev, name);
        break;
    }
    default:
        break;
    }

    ASSERT(con->Close && con->Update && "All controllers must set update & close methods.");
    dev->mOpen = 1;
}

static void HIDUnloadController(uint8_t devID)
{
    ASSERT(devID > 0 && devID <= MAX_DEVICES_TRACKED);
    uint8_t index = devID - 1;

    HIDController* con = gControllerBuffer + index;
    if (!con->pDevice)
    {
        LOGF(LogLevel::eWARNING, "Tried to unload non-existant controller at index %i", index);
        return;
    }

    ASSERT(con->Close && "All controllers must set a close method.");

    con->Close(con);
    con->pDevice = NULL;

    // wipe data
    memset((void*)con, 0, sizeof *con);
}

static bool HIDAddController(const HIDDeviceInfo* pDeviceInfo)
{
    extern InputPortIndex GamepadAddHIDController(const HIDDeviceInfo* pDeviceInfo);
    InputPortIndex        portIndex = GamepadAddHIDController(pDeviceInfo);
    if (PORT_INDEX_INVALID == portIndex)
    {
        return false;
    }

    const uint8_t  devID = pDeviceInfo->mId;
    uint8_t        index = devID - 1;
    // set manID for passing back buttons
    HIDController* con = gControllerBuffer + index;
    con->mPortIndex = portIndex;
    return true;
}

static void HIDRemoveController(uint8_t devID)
{
    uint8_t index = devID - 1;

    HIDController* con = gControllerBuffer + index;
    extern void    GamepadRemoveHIDController(InputPortIndex portIndex);
    GamepadRemoveHIDController(con->mPortIndex);
    con->mPortIndex = PORT_INDEX_INVALID;
}
/************************************************************************/
// Device tracking
/************************************************************************/
bool hidGetIDByMac(uint64_t mac, uint8_t* outID, uint8_t devIDToIgnore)
{
    for (uint32_t i = 0; i < gActiveSlots; ++i)
    {
        HIDDeviceInfo* dev = gDeviceBuffer + i;
        if ((dev->mActive || dev->mWasActive) && dev->mId != devIDToIgnore && dev->mMac == mac)
        {
            if (outID)
            {
                *outID = (uint8_t)(i + 1);
            }
            return true;
        }
    }

    return false;
}

bool hidGetIDByMac(uint64_t mac, uint8_t* outID) { return hidGetIDByMac(mac, outID, 0); }

static void HIDRemoveDevice(HIDDeviceInfo* dev)
{
    if (dev->mOpen)
    {
        HIDRemoveController(dev->mId);
        HIDUnloadController(dev->mId);
    }

    dev->pNext = pFreeList;
    pFreeList = dev;

    dev->mVendorID = 0;
    dev->mProductID = 0;
    dev->mActive = 0;
    dev->mWasActive = 0;
    dev->mOpen = 0;
    dev->mType = 0;

    --gActiveDevices;
}

static void HIDAddDevice(hid_device_info* dev, uint8_t controllertype)
{
    if (!pFreeList)
    {
        // If this is hit the MAX_DEVICES_TRACKED cap needs to be increased
        LOGF(LogLevel::eWARNING, "Failed to add controller due to lack of room: \"%ls - %ls\" at path: %s", dev->manufacturer_string,
             dev->product_string, dev->path);
        return;
    }
    ASSERT(strlen(dev->path) < MAX_PATH_LENGTH);

    LOGF(LogLevel::eDEBUG, "Adding controller \"%ls - %ls\" at path: %s", dev->manufacturer_string, dev->product_string, dev->path);

    // pull a new registry slot off the pFreeList
    HIDDeviceInfo* newDev = pFreeList;
    pFreeList = pFreeList->pNext;

    newDev->mVendorID = dev->vendor_id;
    newDev->mProductID = dev->product_id;
    newDev->mInterface = dev->interface_number;
    newDev->mActive = 1;
    newDev->mWasActive = 1;
    newDev->mOpen = 0;
    newDev->mType = controllertype;
    strcpy(newDev->mLogicalSystemPath, dev->path);
    snprintf(newDev->mName, TF_ARRAY_COUNT(newDev->mName), "%ls %ls", dev->manufacturer_string, dev->product_string);

    ++gActiveDevices;

    int prevActiveCount = gActiveSlots;
    if (gActiveDevices > gActiveSlots)
        gActiveSlots = gActiveDevices;

    HIDLoadController(newDev->mId);
    if (newDev->mOpen) //-V547
    {
        bool added = HIDAddController(newDev);
        // No free slots - Remove
        if (!added)
        {
            gActiveSlots = prevActiveCount;
            HIDRemoveDevice(newDev);
        }
    }
    else // abort, failed to open device
    {
        gActiveSlots = prevActiveCount;
        HIDRemoveDevice(newDev);
    }
}

static void HIDInitDeviceTracking()
{
    memset(gDeviceBuffer, 0, sizeof(gDeviceBuffer));

    HIDDeviceInfo* it = gDeviceBuffer;
    HIDDeviceInfo* end = it + MAX_DEVICES_TRACKED - 1;

    uint8_t idGen = 1;
    // link the array into a list
    for (; it < end; ++it, ++idGen)
    {
        it->mId = idGen;
        it->pNext = it + 1;
    }
    it->pNext = NULL;

    // set the HEAD
    pFreeList = gDeviceBuffer;

    memset((void*)gControllerBuffer, 0, sizeof(gControllerBuffer));
}

static void HIDExitDeviceTracking()
{
    HIDDeviceInfo* devIt = gDeviceBuffer;
    HIDDeviceInfo* devEnd = devIt + gActiveSlots;

    for (; devIt < devEnd; ++devIt)
    {
        if (devIt->mOpen)
        {
            HIDRemoveDevice(devIt);
        }
    }
}

static void HIDLoadDevices()
{
    // mark all devices inactive
    for (uint32_t i = 0; i < gActiveSlots; ++i)
    {
        gDeviceBuffer[i].mActive = 0;
    }

    // get the list of all currently connect HID devices
    hid_device_info* deviceList = hid_enumerate(0, 0);

    if (deviceList)
    {
        hid_device_info* devicesToAdd = NULL;
        hid_device_info* devicesToFree = NULL;

        HIDDeviceInfo*   devIt = gDeviceBuffer;
        HIDDeviceInfo*   devEnd = devIt + gActiveSlots;
        hid_device_info* next = NULL;

        // check which devices need to be added or removed
        for (hid_device_info* it = deviceList; it; it = next)
        {
            next = it->next;

            for (devIt = gDeviceBuffer; devIt < devEnd; ++devIt)
            {
                if (devIt->mWasActive && devIt->mVendorID == it->vendor_id && devIt->mProductID == it->product_id &&
                    !strcmp(devIt->mLogicalSystemPath, it->path))
                {
                    break;
                }
            }

            if (devIt != devEnd)
            {
                devIt->mActive = 1;
                it->next = devicesToFree;
                devicesToFree = it;
            }
            else
            {
                it->next = devicesToAdd;
                devicesToAdd = it;
            }
        }

        // devices must be removed before new ones are added since gActiveSlots is
        //   dependant on the list being as compact as possible
        for (devIt = gDeviceBuffer; devIt < devEnd; ++devIt)
        {
            if (devIt->mWasActive && !devIt->mActive)
            {
                HIDRemoveDevice(devIt);
            }
        }

        // add any qualifying devices
        for (hid_device_info* dev = devicesToAdd; dev; dev = dev->next)
        {
            if (!HIDIsController(dev))
            {
#if defined(HID_VERBOSE_LOGGING)
                LOGF(LogLevel::eDEBUG, "Ignoring non-controller \"%ls - %ls\" at path: %s", dev->manufacturer_string, dev->product_string,
                     dev->path);
#endif
                continue;
            }

            uint8_t type;
            if (!HIDIsSupported(dev, &type))
            {
#if defined(HID_VERBOSE_LOGGING)
                LOGF(LogLevel::eDEBUG, "Ignoring unsupported controller \"%ls - %ls\" at path: %s", dev->manufacturer_string,
                     dev->product_string, dev->path);
#endif
                continue;
            }

            HIDAddDevice(dev, type);
        }

        // deviceList should have been cleanly divided between these two at this point
        hid_free_enumeration(devicesToAdd);
        hid_free_enumeration(devicesToFree);
    }
}

static void HIDUpdateDevices()
{
    HIDController* conIt = gControllerBuffer;
    HIDController* conEnd = conIt + gActiveSlots;
    for (; conIt < conEnd; ++conIt)
    {
        if (!conIt->pDevice)
        {
            continue;
        }

        conIt->Update(conIt);

        // Rumble
        Gamepad& gpad = gGamepads[conIt->mPortIndex];
        bool     stopRumble = false;
        if (gpad.mRumbleHigh == 0.0f && gpad.mRumbleLow == 0.0f)
        {
            stopRumble = true;
        }
        // Dont keep setting zero rumble
        if (!gpad.mRumbleStopped)
        {
            uint16_t left = (uint16_t)(0xFFFFu * clamp(gpad.mRumbleLow, 0.0f, 1.0f));
            uint16_t right = (uint16_t)(0xFFFFu * clamp(gpad.mRumbleHigh, 0.0f, 1.0f));
            conIt->DoRumble(conIt, left, right);
        }
        gpad.mRumbleStopped = stopRumble;

        // LEDs
        if (gpad.mLightReset || gpad.mLightUpdate)
        {
            // Sony-hid colors
            static const uint8_t colors[][3] = {
                { 0x00, 0x00, 0x40 }, /* Blue */
                { 0x40, 0x00, 0x00 }, /* Red */
                { 0x00, 0x40, 0x00 }, /* Green */
                { 0x20, 0x00, 0x20 }, /* Pink */
                { 0x02, 0x01, 0x00 }, /* Orange */
                { 0x00, 0x01, 0x01 }, /* Teal */
                { 0x01, 0x01, 0x01 }, /* White */
                { 0x00, 0x00, 0x40 }, /* Blue */
            };
            COMPILE_ASSERT(TF_ARRAY_COUNT(colors) == MAX_GAMEPADS);

            const uint8_t  gpadLight[] = { (uint8_t)(gpad.mLight[0] * 255.0f), (uint8_t)(gpad.mLight[1] * 255.0f),
                                          (uint8_t)(gpad.mLight[2] * 255.0f) };
            const uint8_t* lights = gpad.mLightReset ? colors[conIt->mPortIndex] : gpadLight;
            conIt->SetLights(conIt, lights[0], lights[1], lights[2]);
            gpad.mLightReset = false;
            gpad.mLightUpdate = false;
        }
    }
}
/************************************************************************/
// System notifications
/************************************************************************/
static void InitSystemNotificationReceiver(const WindowHandle* pWindow)
{
    ASSERT(!gNotifierInitialized);

    bool systemNotifier = false;
#if defined(_WINDOWS)
    DEV_BROADCAST_DEVICEINTERFACE_A devBroadcast = {};
    devBroadcast.dbcc_size = sizeof(devBroadcast);
    devBroadcast.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    devBroadcast.dbcc_classguid = GUID_DEVINTERFACE_USB_DEVICE;

    // DEVICE_NOTIFY_ALL_INTERFACE_CLASSES allows for notification when devices are installed
    gNotifier =
        RegisterDeviceNotification(pWindow->window, &devBroadcast, DEVICE_NOTIFY_WINDOW_HANDLE | DEVICE_NOTIFY_ALL_INTERFACE_CLASSES);
    systemNotifier = gNotifier != NULL;
#elif defined(__linux__)
    pUDev = udev_new();
    pUDevMonitor = NULL;
    gUDevFD = -1;
    if (pUDev)
    {
        pUDevMonitor = udev_monitor_new_from_netlink(pUDev, "udev");

        if (pUDevMonitor)
        {
            udev_monitor_enable_receiving(pUDevMonitor);
            gUDevFD = udev_monitor_get_fd(pUDevMonitor);
            systemNotifier = true;
        }
    }
#endif

    // backup timer system
    if (!systemNotifier)
    {
        initTimer(&gForceRecheckTimer);
    }

    gNotifierInitialized = true;
}

static void ExitSystemNotificationReceiver()
{
    if (!gNotifierInitialized)
    {
        return;
    }

#if defined(_WINDOWS)
    ASSERT(gNotifier && "gNotifierInitialized should have guaranteed this was not null.");
    UnregisterDeviceNotification(gNotifier);
    gNotifier = NULL;
#elif defined(__linux__)
    ASSERT(pUDev && pUDevMonitor && "gNotifierInitialized should have guaranteed these were not null.");
    udev_monitor_unref(pUDevMonitor);
    udev_unref(pUDev);
    pUDevMonitor = NULL;
    pUDev = NULL;
#endif

    gNotifierInitialized = false;
}

static bool HIDCheckForDeviceInstallation()
{
    if (!gNotifierInitialized)
    {
        uint32_t currentTime = getTimerMSec(&gForceRecheckTimer, false);

        // update every 4 seconds
        if ((currentTime - (int32_t)gLastCheck) > 4000)
        {
            gDevicesChanged = true;
            gLastCheck = currentTime;
        }
        // prevent a clock wrap around from locking out devices
        else if (currentTime < gLastCheck)
        {
            gLastCheck = currentTime;
        }

        return false;
    }

#if defined(__linux__)
    if (gUDevFD >= 0)
    {
        struct pollfd pollFD;
        pollFD.fd = gUDevFD;
        pollFD.events = POLLIN;

        if (poll(&pollFD, 1, 0) == 1)
        {
            gDevicesChanged = true;

            // Can't leave yet, need to drain all events on the FD
            struct udev_device* udevDev;
            while (poll(&pollFD, 1, 0) == 1)
            {
                if ((udevDev = udev_monitor_receive_device(pUDevMonitor)) != NULL)
                {
                    udev_device_unref(udevDev);
                }
            }
        }
    }
#endif
    return false;
}
/************************************************************************/
// HID Input implementation
/************************************************************************/
int hidInit(const WindowHandle* pWindow)
{
    ASSERT(!gInitialized);

    if (hid_init() != 0)
    {
        return -1;
    }

    HIDInitDeviceTracking();
    InitSystemNotificationReceiver(pWindow);

    initTimer(&gHIDTimer);

    gDevicesChanged = true;
    gInitialized = true;

    return 0;
}

int hidExit()
{
    HIDExitDeviceTracking();
    ExitSystemNotificationReceiver();

    if (hid_exit() != 0)
    {
        return -1;
    }

    gInitialized = false;

    return 0;
}

void hidUpdate()
{
    HIDUpdateDevices();

    HIDCheckForDeviceInstallation();
    if (gDevicesChanged)
    {
        HIDLoadDevices();
        gDevicesChanged = false;
    }
}

uint64_t hidGetTime()
{
    // Elapsed time
    return getTimerMSec(&gHIDTimer, false);
}

#if defined(_WINDOWS)
void hidHandleMessage(const MSG* msg)
{
    if (!msg || msg->message != WM_DEVICECHANGE)
    {
        return;
    }

    WPARAM wParam = msg->wParam;
    LPARAM lParam = msg->lParam;
    if ((wParam == DBT_DEVICEARRIVAL || wParam == DBT_DEVICEREMOVECOMPLETE) &&
        ((DEV_BROADCAST_HDR*)lParam)->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE)
    {
        gDevicesChanged = true;
    }
}
#endif

static uint32_t crc32(uint32_t crc, const void* data, int len)
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

#include "HIDParserPS4Controller.h"
#include "HIDParserPS5Controller.h"
#include "HIDParserSwitchController.h"
