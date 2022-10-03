
#ifndef GAINPUTHIDTYPES_H_
#define GAINPUTHIDTYPES_H_

#include "../../../../../../../../Application/Config.h"

// While the standard says this is the max packet size, there is no enforcement
//   so some products may have larger packets
#define HID_SPECIFICATION_MAX_PACKET_SIZE 64

struct hid_device_;
typedef struct hid_device_ hid_device;
struct HIDController;


// --- Device Tracker ---------------------------------------------------------

// NOTE: see if this cap can be lowered
#define MAX_PATH_LENGTH 247

typedef struct HIDDeviceInfo
{
    // Identifiers needed for uniquely identifying a connected device
    uint16_t vendorID;
    uint16_t productID;

    uint8_t active : 1;
    uint8_t wasActive : 1;
    uint8_t isOpen : 1;
    uint8_t id : 5;
    uint8_t type : 5;

    uint8_t __pad[2];

    union
    {
        // Needed to distinguish when multiple controllers of the same type are added.
        // Also for when one physical device has multiple logical devices
        char logicalSystemPath[MAX_PATH_LENGTH];
        // used for storing a linked list of free indicies
        HIDDeviceInfo * next;
    };
} HIDDeviceInfo;
// Not a strict req, just to keep size in check
COMPILE_ASSERT(sizeof(HIDDeviceInfo) - 1 <= 0xFF);


// --- Opaque Controller ------------------------------------------------------

struct HIDController;
namespace gainput { class InputDeltaState; }

typedef int(*fnHIDUpdate)(HIDController *, gainput::InputDeltaState *);
typedef void(*fnHIDClose)(HIDController *);
typedef void(*fnHIDSetPlayer)(HIDController *, uint8_t);
typedef void(*fnHIDSetLights)(HIDController *, uint8_t, uint8_t, uint8_t);
typedef void(*fnHIDDoRumble)(HIDController *, uint16_t, uint16_t);

#define HID_CONTROLLER_BUFFER_SIZE 44

struct HIDController
{
    hid_device * hidDev;

    fnHIDUpdate Update;
    fnHIDClose Close;

    fnHIDSetPlayer SetPlayer;
    fnHIDSetLights SetLights;
    fnHIDDoRumble DoRumble;

    uint32_t rumbleEndTime = 0;

    uint8_t data[HID_CONTROLLER_BUFFER_SIZE];
};

#endif
