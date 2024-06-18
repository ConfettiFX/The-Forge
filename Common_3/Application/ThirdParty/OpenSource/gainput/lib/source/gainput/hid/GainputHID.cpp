
#include "GainputHID.h"

#include "GainputHIDTypes.h"
#include "GainputHIDWhitelist.h"
#include "../../hidapi/hidapi.h"
#include "../../../include/gainput/gainput.h"
#include "../../../../../../Utilities/Interfaces/ITime.h"
#include "../../../../../../Utilities/Interfaces/ILog.h"

#include "hidparsers/HIDParserPS4Controller.h"
#include "hidparsers/HIDParserPS5Controller.h"
#include "hidparsers/HIDParserSwitchController.h"

#if defined(_WINDOWS)
#undef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#undef STRICT
#define STRICT
#undef UNICODE
#define UNICODE 1
#undef _WIN32_WINNT
#define _WIN32_WINNT  0x501  // for raw input
#include <windows.h>
#include <Dbt.h>
#include <initguid.h>
DEFINE_GUID(GUID_DEVINTERFACE_USB_DEVICE, 0xA5DCBF10L, 0x6530, 0x11D2, 0x90, 0x1F, 0x00, 0xC0, 0x4F, 0xB9, 0x51, 0xED);

#elif defined(__APPLE__)
#include <IOKit/hid/IOHIDDevice.h>

#elif defined(__linux__)
#include <libudev.h>
#include <poll.h>
#include <math.h>
#endif

// --- Types ------------------------------------------------------------------


// --- Data -------------------------------------------------------------------

Timer gHIDTimer;
static bool gHIDInitialized = false;
static gainput::InputManager* gManager = NULL;


// --- Notifications

static bool gNotifierInitialized = false;

static bool gDevicesChanged = true;

#if defined(_WINDOWS)
HWND gHWND;
WNDCLASSEXW gWND;
HDEVNOTIFY gNotifier;
#elif defined(__APPLE__)
IONotificationPortRef gNotificationPort;
mach_port_t gNotificationMach;
struct OutMessage
{
    mach_msg_header_t hdr;
    char data[4096];
};
#elif defined(__linux__)
struct udev* pUDev;
struct udev_monitor* pUDevMonitor;
int gUDevFD;
#endif

// backup if system has no notification system or fails to initialize
Timer gForceRecheckTimer;
static uint32_t gLastCheck = 0;


// --- Device Tracking

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

HIDDeviceInfo gDeviceBuffer[MAX_DEVICES_TRACKED];
HIDDeviceInfo* pFreeList = nullptr;
uint32_t gActiveDevices = 0;
// This is equal to the max number of devices connnected over the life of the program
uint32_t gActiveSlots = 0;


// --- Controllers

HIDController gControllerBuffer[MAX_DEVICES_TRACKED];
gainput::InputDevicePad* gPassThroughPads[MAX_DEVICES_TRACKED];


// --- Forward Declarations ---------------------------------------------------

static void HIDInitDeviceTracking();
static void HIDExitDeviceTracking();
static void HIDRemoveDevice(HIDDeviceInfo *dev);
static void HIDInitControllerTracking();
void HIDLoadController(uint8_t devID, uint8_t playerNum);
void HIDUnloadController(uint8_t devID);
void HIDHandToInputManager(uint8_t devID);
void HIDRetreiveFromInputManager(uint8_t devID);


// --- System Communication ---------------------------------------------------

// Notifications are needed so that we try to connect to a device only when
// the drivers are properly installed and not just when connected


#if defined(__APPLE__)
static void CallbackIOServiceFunc(void* context, io_iterator_t portIterator)
{
    io_object_t entry = IOIteratorNext(portIterator);
    if (entry)
    {
        gDevicesChanged = true;
        IOObjectRelease(entry);
    }

    // Drain the iterator to continue receiving new notifications
    while ((entry = IOIteratorNext(portIterator)) != 0)
       IOObjectRelease(entry);
}
#elif defined(__linux__)
// mirror of getSystemTime in LinuxTime.c to get around cross compile issues
uint32_t getHIDTime()
{
    long            ms;    // Milliseconds
    time_t          s;     // Seconds
    struct timespec spec;

    clock_gettime(CLOCK_REALTIME, &spec);

    s = spec.tv_sec;
    ms = round(spec.tv_nsec / 1.0e6);    // Convert nanoseconds to milliseconds

    ms += s * 1000;

    return (uint32_t)ms;
}

void resetHIDTimer(Timer* pTimer) { pTimer->mStartTime = getHIDTime(); }
void initHIDTimer(Timer* pTimer) { resetHIDTimer(pTimer); }
unsigned getHIDTimerMSec(Timer* pTimer, bool reset)
{
    unsigned currentTime = getHIDTime();
    unsigned elapsedTime = currentTime - pTimer->mStartTime;
    if (reset)
        pTimer->mStartTime = currentTime;

    return elapsedTime;
}
#endif

static void InitSystemNotificationReceiver(void* window)
{
    if (gNotifierInitialized)
        return;

#if defined(_WINDOWS)
    gHWND = (HWND)window;

    memset(&gWND, 0, sizeof(gWND));
    gWND.hInstance = (HINSTANCE)GetWindowLongPtr(gHWND, GWLP_HINSTANCE);
    gWND.lpszClassName = L"The Forge";

    WNDCLASSW registeredClassData;
    GetClassInfoW(gWND.hInstance, gWND.lpszClassName, &registeredClassData);
    gWND.lpfnWndProc = registeredClassData.lpfnWndProc;
    gWND.cbSize = sizeof(registeredClassData);

    DEV_BROADCAST_DEVICEINTERFACE_A devBroadcast;
    memset(&devBroadcast, 0, sizeof(devBroadcast));
    devBroadcast.dbcc_size = sizeof(devBroadcast);
    devBroadcast.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    devBroadcast.dbcc_classguid = GUID_DEVINTERFACE_USB_DEVICE;

    // DEVICE_NOTIFY_ALL_INTERFACE_CLASSES allows for notification when devices are installed
    gNotifier = RegisterDeviceNotification(gHWND, &devBroadcast, DEVICE_NOTIFY_WINDOW_HANDLE | DEVICE_NOTIFY_ALL_INTERFACE_CLASSES);
    gNotifierInitialized = gNotifier != NULL;
#elif defined(__APPLE__)
    gNotificationPort = IONotificationPortCreate(kIOMasterPortDefault);

    if (gNotificationPort)
    {
        // Drain first match notifications, or we won't receive device added notifications
        io_iterator_t portIterator = 0;
        IOReturn result = IOServiceAddMatchingNotification(gNotificationPort, kIOFirstMatchNotification,
            IOServiceMatching(kIOHIDDeviceKey), CallbackIOServiceFunc, &gDevicesChanged, &portIterator);

        if (result == 0)
        {
            io_object_t entry;
            while ((entry = IOIteratorNext(portIterator)) != 0)
                IOObjectRelease(entry);
        }
        else
        {
            IONotificationPortDestroy(gNotificationPort);
            gNotificationPort = NULL;
        }

        // Drain termination notifications, or we won't receive device removed notifications
        portIterator = 0;
        result = IOServiceAddMatchingNotification(gNotificationPort, kIOTerminatedNotification,
            IOServiceMatching(kIOHIDDeviceKey), CallbackIOServiceFunc, &gDevicesChanged, &portIterator);

        if (result == 0)
        {
            io_object_t entry;
            while ((entry = IOIteratorNext(portIterator)) != 0)
                IOObjectRelease(entry);
        }
        else
        {
            IONotificationPortDestroy(gNotificationPort);
            gNotificationPort = NULL;
        }
    }

    gNotificationMach = MACH_PORT_NULL;
    if (gNotificationPort)
    {
        gNotificationMach = IONotificationPortGetMachPort(gNotificationPort);
    }

    gNotifierInitialized = gNotificationMach != MACH_PORT_NULL;
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
            gNotifierInitialized = true;
        }
    }
#endif


    // backup timer system
    if (!gNotifierInitialized)
    {
#if defined(__linux__)
        initHIDTimer(&gForceRecheckTimer);
#else
        initTimer(&gForceRecheckTimer);
#endif
    }
}

static void ExitSystemNotificationReceiver()
{
    if (!gNotifierInitialized)
        return;

#if defined(_WINDOWS)
    ASSERT(gNotifier && "gNotifierInitialized should have guaranteed this was not null.");
    UnregisterDeviceNotification(gNotifier);
    gNotifier = NULL;
#elif defined(__APPLE__)
    ASSERT(gNotificationPort && "gNotifierInitialized should have guaranteed this was not null.");
    IONotificationPortDestroy(gNotificationPort);
    gNotificationPort = NULL;
#elif defined(__linux__)
    ASSERT(pUDev && pUDevMonitor && "gNotifierInitialized should have guaranteed these were not null.");
    udev_monitor_unref(pUDevMonitor);
    udev_unref(pUDev);
    pUDevMonitor = NULL;
    pUDev = NULL;
#endif

    gNotifierInitialized = false;
}

static bool CheckForDeviceInstallation(void const* message)
{
    if (!gNotifierInitialized)
    {
#if defined(__linux__)
        uint32_t currentTime = getHIDTimerMSec(&gForceRecheckTimer, false);
#else
        uint32_t currentTime = getTimerMSec(&gForceRecheckTimer, false);
#endif

        // update every 4 seconds
        if ((currentTime - (int32_t)gLastCheck) > 4000)
        {
            gDevicesChanged = true;
            gLastCheck = currentTime;
        }
        // prevent a clock wrap around from locking out devices
        else if (currentTime < gLastCheck)
            gLastCheck = currentTime;

        return false;
    }

#if defined(_WINDOWS)
    MSG const * msg = (MSG const*)message;
    if (message && msg->message == WM_DEVICECHANGE)
    {
        WPARAM wParam = msg->wParam;
        LPARAM lParam = msg->lParam;

        if ((wParam == DBT_DEVICEARRIVAL || wParam == DBT_DEVICEREMOVECOMPLETE) &&
            ((DEV_BROADCAST_HDR*)lParam)->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE)
        {
            gDevicesChanged = true;
            return true;
        }
    }
#elif defined(__APPLE__)
    OutMessage outMSG;
    while (mach_msg(&outMSG.hdr, MACH_RCV_MSG | MACH_RCV_TIMEOUT, 0, sizeof(OutMessage),
                    gNotificationMach, 0, MACH_PORT_NULL) == KERN_SUCCESS)
        IODispatchCalloutFromMessage(NULL, &outMSG.hdr, gNotificationPort);
#elif defined(__linux__)
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
                if ((udevDev = udev_monitor_receive_device(pUDevMonitor)) != NULL)
                    udev_device_unref(udevDev);
        }
    }
#endif
    return false;
}


// --- HID Management ---------------------------------------------------------

int HIDInit(void* window, gainput::InputManager* man)
{
	if (gHIDInitialized)
		return 0;

	if (hid_init() != 0)
		return -1;

	gManager = man;

	HIDInitDeviceTracking();
	HIDInitControllerTracking();
	InitSystemNotificationReceiver(window);

	initTimer(&gHIDTimer);

	gHIDInitialized = true;
	gDevicesChanged = true;

	return 0;
}

int HIDExit()
{
	if (!gHIDInitialized)
		return 0;

	gHIDInitialized = false;

	HIDExitDeviceTracking();
	ExitSystemNotificationReceiver();

	gManager = NULL;

	if (hid_exit() != 0)
		return -1;

	return 0;
}

// --- HID Device Tracking

static void HIDInitDeviceTracking()
{
    memset(gDeviceBuffer, 0, sizeof(gDeviceBuffer));

    HIDDeviceInfo* it = gDeviceBuffer;
    HIDDeviceInfo* end = it + MAX_DEVICES_TRACKED - 1;

    uint8_t idGen = 1;
    // link the array into a list
    for (; it < end; ++it, ++idGen)
    {
        it->id = idGen;
        it->next = it + 1;
    }
    it->next = NULL;

    // set the HEAD
    pFreeList = gDeviceBuffer;
}

static void HIDExitDeviceTracking()
{
    HIDDeviceInfo * devIt = gDeviceBuffer;
    HIDDeviceInfo * devEnd = devIt + gActiveSlots;

    for (; devIt < devEnd; ++devIt)
        if (devIt->isOpen)
            HIDRemoveDevice(devIt);
}

static void HIDAddDevice(hid_device_info *dev, uint8_t controllertype)
{
    if (!pFreeList)
    {
        // If this is hit the MAX_DEVICES_TRACKED cap needs to be increased
        LOGF(LogLevel::eWARNING, "Failed to add controller due to lack of room: \"%ls - %ls\" at path: %s",
            dev->manufacturer_string,
            dev->product_string,
            dev->path);
        return;
    }
    ASSERT(strlen(dev->path) < MAX_PATH_LENGTH);

    LOGF(LogLevel::eDEBUG, "Adding controller \"%ls - %ls\" at path: %s",
        dev->manufacturer_string,
        dev->product_string,
        dev->path);

    // pull a new registry slot off the pFreeList
    HIDDeviceInfo *newDev = pFreeList;
    pFreeList = pFreeList->next;

    newDev->vendorID = dev->vendor_id;
    newDev->productID = dev->product_id;
	newDev->interface = dev->interface_number;
    newDev->active = 1;
    newDev->wasActive = 1;
    newDev->isOpen = 0;
    newDev->type = controllertype;
    strcpy(newDev->logicalSystemPath, dev->path);

    ++gActiveDevices;

    int prevActiveCount = gActiveSlots;
	if (gActiveDevices > gActiveSlots)
		gActiveSlots = gActiveDevices;

	HIDLoadController(newDev->id, newDev->id - 1);
    if (newDev->isOpen)
    {
        HIDHandToInputManager(newDev->id);
    }
    else // abort, failed to open device
    {
        gActiveSlots = prevActiveCount;
        HIDRemoveDevice(newDev);
    }
}

static void HIDRemoveDevice(HIDDeviceInfo *dev)
{
    if (dev->isOpen)
    {
        HIDRetreiveFromInputManager(dev->id);
        HIDUnloadController(dev->id);
    }

    dev->next = pFreeList;
    pFreeList = dev;

    dev->vendorID = 0;
    dev->productID = 0;
    dev->active = 0;
    dev->wasActive = 0;
    dev->isOpen = 0;
    dev->type = 0;

    --gActiveDevices;
}

static void HIDLoadDevices()
{
    // mark all devices inactive
    for (uint32_t i = 0; i < gActiveSlots; ++i)
        gDeviceBuffer[i].active = 0;

    // get the list of all currently connect HID devices
    hid_device_info *deviceList = hid_enumerate(0, 0);

    if (deviceList)
    {
        hid_device_info* devicesToAdd = NULL;
        hid_device_info* devicesToFree = NULL;

        HIDDeviceInfo * devIt = gDeviceBuffer;
        HIDDeviceInfo * devEnd = devIt + gActiveSlots;
        hid_device_info* next = NULL;

        // check which devices need to be added or removed
        for (hid_device_info *it = deviceList; it; it = next)
        {
            next = it->next;

            for (devIt = gDeviceBuffer; devIt < devEnd; ++devIt)
                if (devIt->wasActive &&
                    devIt->vendorID == it->vendor_id &&
                    devIt->productID == it->product_id &&
                    !strcmp(devIt->logicalSystemPath, it->path))
                    break;

            if (devIt != devEnd)
            {
                devIt->active = 1;
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
            if (devIt->wasActive && !devIt->active)
                HIDRemoveDevice(devIt);

        // add any qualifying devices
        for (hid_device_info* dev = devicesToAdd; dev; dev = dev->next)
        {
            if (!HIDIsController(dev))
            {
#if defined(HID_VERBOSE_LOGGING)
                LOGF(LogLevel::eDEBUG, "Ignoring non-controller \"%ls - %ls\" at path: %s",
                    dev->manufacturer_string,
                    dev->product_string,
                    dev->path);
#endif
                continue;
            }

            uint8_t type;
            if (!HIDIsSupported(dev, &type))
            {
#if defined(HID_VERBOSE_LOGGING)
                LOGF(LogLevel::eDEBUG, "Ignoring unsupported controller \"%ls - %ls\" at path: %s",
                    dev->manufacturer_string,
                    dev->product_string,
                    dev->path);
#endif
                continue;
            }

            if ( !gManager->IsHIDEnabled() )
            {
                // HID Disabled so just don't add devices
                continue;
            }
            
            HIDAddDevice(dev, type);
        }

        // deviceList should have been cleanly divided between these two at this point
        hid_free_enumeration(devicesToAdd);
        hid_free_enumeration(devicesToFree);
    }
}

// Currently only detects controllers
static void HIDDetectDevices()
{
#if !defined(_WINDOWS)
    CheckForDeviceInstallation(NULL);
#endif

    if (gDevicesChanged)
    {
        HIDLoadDevices();

        gDevicesChanged = false;
    }
}

bool HIDHandleSystemMessage(void const* message)
{
    ASSERT(gHIDInitialized);

    bool result = false;
#if defined(_WINDOWS)
    result = CheckForDeviceInstallation(message);
#endif
    return result;
}


// --- HID Controller Setup

// Returns 0 if none available, devID otherwise
//   Sets given pointers if not null
uint8_t HIDGetNextNewControllerID(uint8_t* outPlatform, uint16_t* outVendorID, uint16_t* outproductID)
{
    if (!gActiveSlots)
        return (uint8_t)-1;

    static uint8_t i = 0;
    if (i >= gActiveSlots)
        i = 0;

    for (uint8_t initialSlot = i;;)
    {
        if (gDeviceBuffer[i].active && !gDeviceBuffer[i].isOpen)
            break;

        i = (i + 1) % gActiveSlots;
        if (i == initialSlot)
            return INVALID_DEV_ID;
    }

    if (outproductID)
        *outproductID = gDeviceBuffer[i].productID;
    if (outVendorID)
        *outVendorID = gDeviceBuffer[i].vendorID;
    if (outPlatform)
        *outPlatform = gDeviceBuffer[i].type;

    // Device id is 1 higher than its index, plus this prepares for the next call
    return ++i;
}

static void HIDInitControllerTracking()
{
    memset((void *)gControllerBuffer, 0, sizeof(gControllerBuffer));
    memset((void *)gPassThroughPads, 0, sizeof(gPassThroughPads));
}

static inline void UnsupportedControllerDebug(HIDDeviceInfo* dev, char const* brand)
{
    LOGF(LogLevel::eDEBUG, "Unsupported %s controller:   0x%.4X - 0x%.4X   Path: %s",
        brand,
        dev->vendorID,
        dev->productID,
        dev->logicalSystemPath);
}

static inline void FailedToOpenDebug(HIDDeviceInfo* dev, char const* brand)
{
    LOGF(LogLevel::eDEBUG, "Failed to open %s controller:   0x%.4X - 0x%.4X   Path: %s",
        brand,
        dev->vendorID,
        dev->productID,
        dev->logicalSystemPath);
}

static inline void SuccessfulOpenDebug(HIDDeviceInfo* dev, char const* brand)
{
    LOGF(LogLevel::eDEBUG, "Successfully opened %s controller:   0x%.4X - 0x%.4X   Path: %s",
        brand,
        dev->vendorID,
        dev->productID,
        dev->logicalSystemPath);
}

void HIDLoadController(uint8_t devID, uint8_t playerNum)
{
    ASSERT(devID > 0 && devID <= MAX_DEVICES_TRACKED);
    uint8_t index = devID - 1;

    if (!gDeviceBuffer[index].active)
    {
        LOGF(LogLevel::eWARNING, "Tried to load non-existant device at index %i", index);
        return;
    }

    HIDDeviceInfo* dev = gDeviceBuffer + index;
    HIDController* con = gControllerBuffer + index;
    char const* name = ControllerName(dev->type);

    switch (dev->type)
    {
    case ctPS4:
        if (!HIDIsSupportedPS4Controller(dev))
        {
            UnsupportedControllerDebug(dev, name);
            return;
        }
        if (HIDOpenPS4Controller(dev, con, playerNum) == -1)
        {
            FailedToOpenDebug(dev, name);
            return;
        }
        SuccessfulOpenDebug(dev, name);
        break;
    case ctPS5:
        if (!HIDIsSupportedPS5Controller(dev))
        {
            UnsupportedControllerDebug(dev, name);
            return;
        }
        if (HIDOpenPS5Controller(dev, con, playerNum) == -1)
        {
            FailedToOpenDebug(dev, name);
            return;
        }
        SuccessfulOpenDebug(dev, name);
        break;
	case ctSwitchPro:
	case ctSwitchJoyConL:
	case ctSwitchJoyConR:
	case ctSwitchJoyConPair:
		if (!HIDIsSupportedSwitchController(dev))
		{
			UnsupportedControllerDebug(dev, name);
			return;
		}
		if (HIDOpenSwitchController(dev, con, playerNum) == -1)
		{
			FailedToOpenDebug(dev, name);
			return;
		}
		SuccessfulOpenDebug(dev, name);
		break;
    default:
        break;
    }

    ASSERT(con->Close && con->Update && "All controllers must set update & close methods.");
    dev->isOpen = 1;
}

void HIDUnloadController(uint8_t devID)
{
    ASSERT(devID > 0 && devID <= MAX_DEVICES_TRACKED);
    uint8_t index = devID - 1;

    HIDController* con = gControllerBuffer + index;
    if (!con->hidDev)
    {
        LOGF(LogLevel::eWARNING, "Tried to unload non-existant controller at index %i", index);
        return;
    }

    ASSERT(con->Close && "All controllers must set a close method.");

    con->Close(con);
    con->hidDev = NULL;

    // wipe data
    memset((void *)con, 0, sizeof *con);
}

void HIDHandToInputManager(uint8_t devID)
{
	uint8_t index = devID - 1;

	gainput::DeviceId manId = gManager->GetNextId();
	gPassThroughPads[index] = tf_new(gainput::InputDevicePad, *gManager, manId, devID);

	gManager->AddDevice(manId, gPassThroughPads[index]);

	// set manID for passing back buttons
	HIDController* con = gControllerBuffer + index;
	con->manID = manId;
}

void HIDRetreiveFromInputManager(uint8_t devID)
{
	uint8_t index = devID - 1;

	HIDController* con = gControllerBuffer + index;
	con->manID = ~0u;

    if (gPassThroughPads[index] != NULL)
    {
        gManager->RemoveDevice(gPassThroughPads[index]->GetDeviceId());
        tf_delete(gPassThroughPads[index]);
        gPassThroughPads[index] = NULL;
    }
}


// --- HID Controller Utils

static bool GetController(uint8_t devID, HIDController** outCon)
{
    ASSERT(devID > 0 && devID <= MAX_DEVICES_TRACKED);
    uint8_t index = devID - 1;

    HIDController* con = gControllerBuffer + index;
    if (!con->hidDev)
    {
        LOGF(LogLevel::eWARNING, "Tried to retrieve non-existant controller at index %i", index);
        return false;
    }

    *outCon = con;
    return true;
}

bool GetIDByMac(uint64_t mac, uint8_t* outID)
{
    return GetIDByMac(mac, outID, 0);
}

bool GetIDByMac(uint64_t mac, uint8_t* outID, uint8_t devIDToIgnore)
{
    for (uint32_t i = 0; i < gActiveSlots; ++i)
    {
        HIDDeviceInfo* dev = gDeviceBuffer + i;
        if ((dev->active || dev->wasActive) &&
            dev->id != devIDToIgnore &&
            dev->mac == mac)
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

bool HIDControllerIsConnected(uint8_t devID)
{
	uint8_t index = devID - 1;
	if (index >= gActiveSlots)
		return false;

	return gDeviceBuffer[index].active;
}

void HIDUpdate(uint8_t devID, gainput::InputDeltaState * state)
{
	HIDController* con = nullptr;
	if (!GetController(devID, &con))
		return;

	con->Update(con, state);

	if (con->rumbleEndTime &&
		con->rumbleEndTime <= getTimerMSec(&gHIDTimer, false))
	{
		con->DoRumble(con, 0, 0);
		con->rumbleEndTime = 0;
	}
}

char const * HIDControllerName(uint8_t devID)
{
	uint8_t index = devID - 1;
	if (index >= gActiveSlots)
		return "[No controller at index]";

	return ControllerName(gDeviceBuffer[index].type);
}

uint64_t HIDControllerMACAddress(uint8_t devID)
{
    uint8_t index = devID - 1;
    if (index >= gActiveSlots)
        return INVALID_MAC;

    return gDeviceBuffer[index].mac;
}

void HIDSetPlayer(uint8_t devID, uint8_t playerNum)
{
    HIDController* con = nullptr;
    if (!GetController(devID, &con))
        return;

    if (con->SetPlayer)
        con->SetPlayer(con, playerNum);
}

void HIDSetLights(uint8_t devID, uint8_t r, uint8_t g, uint8_t b)
{
    HIDController* con = nullptr;
    if (!GetController(devID, &con))
        return;

    if (con->SetLights)
        con->SetLights(con, r, g, b);
}

void HIDDoRumble(uint8_t devID, float leftFlt, float rightFlt, uint32_t durationMS)
{
    HIDController* con = nullptr;
    if (!GetController(devID, &con))
        return;

    if (durationMS)
        con->rumbleEndTime = getTimerMSec(&gHIDTimer, false) + durationMS;

    if (leftFlt < 0)
        leftFlt = 0;
    if (leftFlt > 1)
        leftFlt = 1;
    if (rightFlt < 0)
        rightFlt = 0;
    if (rightFlt > 1)
        rightFlt = 1;

    //if both motors are set to 0 but we have a duration set
    // just set duration to 0 and cancel out the rumble itself
    if(durationMS && (rightFlt + leftFlt) <= 0.0001f)
        con->rumbleEndTime = 0;
    
    uint16_t left = (uint16_t)(0xFFFFu * leftFlt);
    uint16_t right = (uint16_t)(0xFFFFu * rightFlt);

    if (con->DoRumble)
        con->DoRumble(con, left, right);
}


// --- HID Input Fetching

static void HIDUpdateDevices(gainput::InputDeltaState * state)
{
    HIDController * conIt = gControllerBuffer;
    HIDController * conEnd = conIt + gActiveSlots;
    for (; conIt < conEnd; ++conIt)
        if (conIt->hidDev)
        {
            conIt->Update(conIt, state);

            if (conIt->rumbleEndTime &&
                conIt->rumbleEndTime <= getTimerMSec(&gHIDTimer, false))
            {
                conIt->DoRumble(conIt, 0, 0);
                conIt->rumbleEndTime = 0;
            }
        }
}

void HIDPromptForDeviceStateReports(gainput::InputDeltaState * state)
{
    ASSERT(gHIDInitialized);

    HIDUpdateDevices(state);

    // Don't want to risk device connection events being intermixed with input 
    //   events from the same devices
    HIDDetectDevices();
}

uint64_t HIDGetTime()
{
	return getTimerMSec(&gHIDTimer, false);
}
