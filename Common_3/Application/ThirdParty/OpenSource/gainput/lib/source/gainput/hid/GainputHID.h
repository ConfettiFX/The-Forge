
#ifndef GAINPUTHID_H_
#define GAINPUTHID_H_

#include "../../../../../../../../Application/Config.h"

//#define HID_VERBOSE_LOGGING

namespace gainput { class InputManager;  class InputDeltaState; }

// Initialize the HID driver, controller parsers, and request the system
//   for prompts when devices are fully available (after driver installation,
//   not merely after being connected)
int HIDInit(void* window, gainput::InputManager* man);
// Clean up everything initialized above
int HIDExit();

// Used to detect if there is system message indicating a device has been
//   installed or removed
bool HIDHandleSystemMessage(void const* message);
// Used to prompt the HID subsystem to fetch the input state for each HID
//   device and forward changes to Gainput
void HIDPromptForDeviceStateReports(gainput::InputDeltaState* state);
// Live time for HID code that needs to wait on packets
uint64_t HIDGetTime();


// --- Controller Management

#define INVALID_DEV_ID 0

// button_any devID should be retrieved through input events
//   For opening, though, they should be opened when a device connection
//   event is pushed and/or through polling

bool GetIDByMac(uint64_t mac, uint8_t* outID);
bool GetIDByMac(uint64_t mac, uint8_t* outID, uint8_t devIDToIgnore);
bool HIDControllerIsConnected(uint8_t devId);
void HIDUpdate(uint8_t devID, gainput::InputDeltaState * state);
char const * HIDControllerName(uint8_t devId);
void HIDSetPlayer(uint8_t devID, uint8_t playerNum);
void HIDSetLights(uint8_t devID, uint8_t r, uint8_t g, uint8_t b);
void HIDDoRumble(uint8_t devID, float left, float right, uint32_t durationMS);
#endif
