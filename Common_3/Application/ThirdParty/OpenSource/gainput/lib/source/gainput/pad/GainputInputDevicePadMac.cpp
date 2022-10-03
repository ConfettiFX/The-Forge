
#include "../../../include/gainput/gainput.h"

#ifdef GAINPUT_PLATFORM_MAC

#include "GainputInputDevicePadImpl.h"
#include "../../../include/gainput/GainputInputDeltaState.h"
#include "../../../include/gainput/GainputHelpers.h"
#include "../../../include/gainput/GainputLog.h"

#include "GainputInputDevicePadMac.h"

#import <CoreFoundation/CoreFoundation.h>
#import <IOKit/hid/IOHIDManager.h>
#import <IOKit/hid/IOHIDUsageTables.h>


namespace gainput
{

extern bool MacIsApplicationKey();

namespace {
// no need for more than 8 pads.
static const unsigned kMaxPads = 8;
// older xbox controllers use this to determine dpad values
static const int dpadScheme[] = { 0,1,  1,1,  1,0,
							    1,-1, 0,-1, -1,-1,
							   -1,0, -1,1,   0,0};

// newer controllers such as xbox series use this scheme.
static const int altDpadScheme[] = { 0,0,  0,1, 1,1,
								   1,0,  1,-1,  0,-1,
								  -1,-1, -1,0, -1,1};

// store serial numbers of controller to avoid rgistering them multiple times.
// we map a serial number to a specific gainput pad user id.
static const char* sDeviceIdToPadSerials[kMaxPads] = { 0 };

static inline float FixUpAnalog(float analog, const float minAxis, const float maxAxis, bool symmetric)
{
	analog = analog < minAxis ? minAxis : analog > maxAxis ? maxAxis : analog; // clamp
	analog -= minAxis;
	analog /= (Abs(minAxis) + Abs(maxAxis))*(symmetric ? 0.5f : 1.0f);
	if (symmetric)
	{
		analog -= 1.0f;
	}
	return analog;
}

static void OnDeviceInput(void* inContext, IOReturn inResult, void* inSender, IOHIDValueRef value)
{
	if (!MacIsApplicationKey())
	{
		return;
	}

	IOHIDElementRef elem = IOHIDValueGetElement(value);

	InputDevicePadImplMac* device = reinterpret_cast<InputDevicePadImplMac*>(inContext);
	GAINPUT_ASSERT(device);

	uint32_t usagePage = IOHIDElementGetUsagePage(elem);
	uint32_t usage = IOHIDElementGetUsage(elem);

	if (IOHIDElementGetReportCount(elem) > 1
			|| (usagePage == kHIDPage_GenericDesktop && usage == kHIDUsage_GD_Pointer) )
	{
		return;
	}

    if (usagePage >= kHIDPage_VendorDefinedStart)
    {
        return;
    }

    //InputManager& manager = device->manager_;
	int state = (int)IOHIDValueGetIntegerValue(value);
	float analog = IOHIDValueGetScaledValue(value, kIOHIDValueScaleTypePhysical);

	if (usagePage == kHIDPage_Button && device->buttonDialect_.count(usage))
	{
		const DeviceButtonId buttonId = device->buttonDialect_[usage];
        HandleButton(device->device_, device->nextState_, device->delta_, buttonId, state != 0);
	}
	else if (usagePage == kHIDPage_GenericDesktop || usagePage == kHIDPage_Simulation)
	{
		if (usage == kHIDUsage_GD_Hatswitch)
		{
			int dpadX = 0;
			int dpadY = 0;

			// 8 different direction consider diagonals
			int indexDpad = state < 9 ? state * 2 : 0;
			// newer xbox controllers have different dpad combos
			if(device->alternativeDPadScheme)
			{
				dpadX = altDpadScheme[indexDpad];
				dpadY = altDpadScheme[indexDpad + 1];
			}
			else
			{
				dpadX = dpadScheme[indexDpad];
				dpadY = dpadScheme[indexDpad + 1];
			}
			HandleButton(device->device_, device->nextState_, device->delta_, PadButtonLeft, dpadX < 0);
			HandleButton(device->device_, device->nextState_, device->delta_, PadButtonRight, dpadX > 0);
			HandleButton(device->device_, device->nextState_, device->delta_, PadButtonUp, dpadY > 0);
			HandleButton(device->device_, device->nextState_, device->delta_, PadButtonDown, dpadY < 0);
		}
		else if (device->axisDialect_.count(usage))
		{
			const DeviceButtonId buttonId = device->axisDialect_[usage];
            float leftStickX = device->nextState_.GetFloat(PadButtonLeftStickX);
            float leftStickY = device->nextState_.GetFloat(PadButtonLeftStickY);
            float rightStickX = device->nextState_.GetFloat(PadButtonRightStickX);
            float rightStickY = device->nextState_.GetFloat(PadButtonRightStickY);
			if (buttonId == PadButtonAxis4 || buttonId == PadButtonAxis5)
			{
				analog = FixUpAnalog(analog, IOHIDElementGetPhysicalMin(elem), IOHIDElementGetPhysicalMax(elem), false);
                HandleAxis(device->device_, device->nextState_, device->delta_, buttonId, analog);
			}
			else if (buttonId == PadButtonLeftStickY || buttonId == PadButtonRightStickY)
			{
				analog = -FixUpAnalog(analog, IOHIDElementGetPhysicalMin(elem), IOHIDElementGetPhysicalMax(elem), true);
                if(buttonId == PadButtonLeftStickY)
                {
                    HandleAxis(device->device_, device->nextState_, device->delta_, buttonId, analog);
                    HandleAxis(device->device_, device->nextState_, device->delta_, PadButtonLeftStickX, leftStickX);
                }
                else
                {
                    HandleAxis(device->device_, device->nextState_, device->delta_, buttonId, analog);
                    HandleAxis(device->device_, device->nextState_, device->delta_, PadButtonRightStickX, rightStickX);
                }
			}
			else if (buttonId == PadButtonLeftStickX || buttonId == PadButtonRightStickX)
			{
                analog = FixUpAnalog(analog, IOHIDElementGetPhysicalMin(elem), IOHIDElementGetPhysicalMax(elem), true);
                
                if(buttonId == PadButtonLeftStickX)
                {
                    HandleAxis(device->device_, device->nextState_, device->delta_, buttonId, analog);
                    HandleAxis(device->device_, device->nextState_, device->delta_, PadButtonLeftStickY, leftStickY);
                }
                else
                {
                    HandleAxis(device->device_, device->nextState_, device->delta_, buttonId, analog);
                    HandleAxis(device->device_, device->nextState_, device->delta_, PadButtonRightStickY, rightStickY);
                }
			}
		}
		else if (device->buttonDialect_.count(usage))
		{
			const DeviceButtonId buttonId = device->buttonDialect_[usage];
            HandleButton(device->device_, device->nextState_, device->delta_, buttonId, state != 0);
		}
#ifdef GAINPUT_DEBUG
		else
		{
			GAINPUT_LOG("Unmapped button (generic): %d\n", usage);
		}
#endif
	}
#ifdef GAINPUT_DEBUG
	else
	{
		GAINPUT_LOG("Unmapped button: %d\n", usage);
	}
#endif
}

static void OnDeviceConnected(void* inContext, IOReturn inResult, void* inSender, IOHIDDeviceRef inIOHIDDeviceRef)
{
	InputDevicePadImplMac* device = reinterpret_cast<InputDevicePadImplMac*>(inContext);
	GAINPUT_ASSERT(device);

	if (device->deviceState_ != InputDevice::DS_UNAVAILABLE)
	{
		return;
	}

	long vendorId = 0;
	long productId = 0;
	device->deviceName = "";
	device->serialNumber = "";

	if (CFTypeRef tCFTypeRef = IOHIDDeviceGetProperty(inIOHIDDeviceRef, CFSTR(kIOHIDVendorIDKey) ))
	{
		if (CFNumberGetTypeID() == CFGetTypeID(tCFTypeRef))
		{
			CFNumberGetValue((CFNumberRef)tCFTypeRef, kCFNumberSInt32Type, &vendorId);
		}
	}
	if (CFTypeRef tCFTypeRef = IOHIDDeviceGetProperty(inIOHIDDeviceRef, CFSTR(kIOHIDProductIDKey) ))
	{
		if (CFNumberGetTypeID() == CFGetTypeID(tCFTypeRef))
		{
			CFNumberGetValue((CFNumberRef)tCFTypeRef, kCFNumberSInt32Type, &productId);
		}
	}
	if (CFTypeRef tCFTypeRef = IOHIDDeviceGetProperty(inIOHIDDeviceRef, CFSTR(kIOHIDSerialNumberKey) ))
	{
		if (CFStringGetTypeID() == CFGetTypeID(tCFTypeRef))
		{
			device->serialNumber = CFStringGetCStringPtr((CFStringRef)tCFTypeRef, kCFStringEncodingASCII);
		}
	}
	if (CFTypeRef tCFTypeRef = IOHIDDeviceGetProperty(inIOHIDDeviceRef, CFSTR(kIOHIDProductKey) ))
	{
		if (CFStringGetTypeID() == CFGetTypeID(tCFTypeRef))
		{
			device->deviceName = CFStringGetCStringPtr((CFStringRef)tCFTypeRef, kCFStringEncodingASCII);
		}
	}


	for (unsigned i = 0; i < device->index_ && i < kMaxPads; ++i)
	{
		if (sDeviceIdToPadSerials[i] == 0 || strcmp(device->serialNumber, sDeviceIdToPadSerials[i]) == 0)
		{
			return;
		}
	}

	//This impl only processes xbox controllers.
	//playstations are handlded with gainputHID
	if (vendorId == 0x045e && (productId == 0x028E || productId == 0x028F || productId == 0x02D1)) // Microsoft 360 Controller wired/wireless, Xbox One Controller
	{
		device->minAxis_ = -(1<<15);
		device->maxAxis_ = 1<<15;
		device->minTriggerAxis_ = 0;
		device->maxTriggerAxis_ = 255;
		device->axisDialect_[kHIDUsage_GD_X] = PadButtonLeftStickX;
		device->axisDialect_[kHIDUsage_GD_Y] = PadButtonLeftStickY;
		device->axisDialect_[kHIDUsage_GD_Rx] = PadButtonRightStickX;
		device->axisDialect_[kHIDUsage_GD_Ry] = PadButtonRightStickY;
		device->axisDialect_[kHIDUsage_GD_Z] = PadButtonAxis4;
		device->axisDialect_[kHIDUsage_GD_Rz] = PadButtonAxis5;
		device->buttonDialect_[0x0a] = PadButtonSelect;
		device->buttonDialect_[0x07] = PadButtonL3;
		device->buttonDialect_[0x08] = PadButtonR3;
		device->buttonDialect_[0x09] = PadButtonStart;
		device->buttonDialect_[0x0c] = PadButtonUp;
		device->buttonDialect_[0x0f] = PadButtonRight;
		device->buttonDialect_[0x0d] = PadButtonDown;
		device->buttonDialect_[0x0e] = PadButtonLeft;
		device->buttonDialect_[0x05] = PadButtonL1;
		device->buttonDialect_[0x06] = PadButtonR1;
		device->buttonDialect_[0x04] = PadButtonY;
		device->buttonDialect_[0x02] = PadButtonB;
		device->buttonDialect_[0x01] = PadButtonA;
		device->buttonDialect_[0x03] = PadButtonX;
		device->buttonDialect_[0x0b] = PadButtonHome;
	}
	else if(vendorId == 0x045e && (productId == 0x0B13 || productId == 0x02FD || productId == 0x02E0 || productId == 0x0B20|| productId == 0x0B12)) //Xbox series S/X Wireless
	{
		device->alternativeDPadScheme = true;
		device->minAxis_ = 0;
		device->maxAxis_ = 1<<16;
		device->minTriggerAxis_ = 0;
		device->maxTriggerAxis_ = 255;
		device->axisDialect_[kHIDUsage_GD_X] = PadButtonLeftStickX;
		device->axisDialect_[kHIDUsage_GD_Y] = PadButtonLeftStickY;
		device->axisDialect_[0x32] = PadButtonRightStickX;
		device->axisDialect_[0x35] = PadButtonRightStickY;
		device->axisDialect_[0xc5] = PadButtonAxis4;
		device->axisDialect_[0xc4] = PadButtonAxis5;

		device->buttonDialect_[0x07] = PadButtonL1;
		device->buttonDialect_[0x08] = PadButtonR1;
		device->buttonDialect_[0x0b] = PadButtonSelect;
		device->buttonDialect_[0x0c] = PadButtonStart;
		device->buttonDialect_[0x0d] = PadButtonHome;
		device->buttonDialect_[0x0e] = PadButtonL3;
		device->buttonDialect_[0x0f] = PadButtonR3;

		device->buttonDialect_[kHIDUsage_GD_Hatswitch] = PadButtonUp;
		device->buttonDialect_[kHIDUsage_GD_Hatswitch] = PadButtonRight;
		device->buttonDialect_[kHIDUsage_GD_Hatswitch] = PadButtonDown;
		device->buttonDialect_[kHIDUsage_GD_Hatswitch] = PadButtonLeft;
		device->buttonDialect_[0x01] = PadButtonA;
		device->buttonDialect_[0x02] = PadButtonB;
		device->buttonDialect_[0x04] = PadButtonX;
		device->buttonDialect_[0x05] = PadButtonY;
	}
	else
	{
		device->deviceName = "";
		device->serialNumber = "";
		//unsupported controller for this pad implementation
		//will fallback to gainputhid
		return;
	}

	// we've assigned the virtual keys and it's a valid controller
	if (device->index_ < kMaxPads)
	{
		sDeviceIdToPadSerials[device->index_] = device->serialNumber;
	}
	device->deviceState_ = InputDevice::DS_OK;
	if(device->deviceChangeCb)
		(*(device->deviceChangeCb))(device->GetDeviceName(), true, device->index_);
}

static void OnDeviceRemoved(void* inContext, IOReturn inResult, void* inSender, IOHIDDeviceRef inIOHIDDeviceRef)
{
	InputDevicePadImplMac* device = reinterpret_cast<InputDevicePadImplMac*>(inContext);
	GAINPUT_ASSERT(device);
	if(device->deviceState_ != InputDevice::DS_OK)
		return;

	const char * disconnectedSerial = "";
	// get the serial number for the disconnected controller
	if (CFTypeRef tCFTypeRef = IOHIDDeviceGetProperty(inIOHIDDeviceRef, CFSTR(kIOHIDSerialNumberKey) ))
	{
		if (CFStringGetTypeID() == CFGetTypeID(tCFTypeRef))
		{
			// get const char * for serial number
			disconnectedSerial = CFStringGetCStringPtr((CFStringRef)tCFTypeRef, kCFStringEncodingASCII);
		}
	}
	//check that index of pad is still valid
	if(device->index_ >= kMaxPads)
	{
		device->deviceState_ = InputDevice::DS_UNAVAILABLE;
		device->deviceChangeCb = nil;
		return;
	}
	// check that our serial number has ben disconnected.
	if(strcmp(device->serialNumber, disconnectedSerial ) != 0)
	{
		return;
	}


	device->deviceState_ = InputDevice::DS_UNAVAILABLE;
	if(device->deviceChangeCb)
		(*(device->deviceChangeCb))(device->GetDeviceName(), false, device->index_);
	if (device->index_ < kMaxPads)
	{
		sDeviceIdToPadSerials[device->index_] = 0;
	}
}

}

InputDevicePadImplMac::InputDevicePadImplMac(InputManager& manager, InputDevice& device, unsigned index, InputState& state, InputState& previousState) :
	buttonDialect_(manager.GetAllocator()),
	axisDialect_(manager.GetAllocator()),
	alternativeDPadScheme(false),
	minAxis_(-FLT_MAX),
	maxAxis_(FLT_MAX),
	minTriggerAxis_(-FLT_MAX),
	maxTriggerAxis_(FLT_MAX),
	manager_(manager),
	device_(device),
	index_(index),
	state_(state),
	previousState_(previousState),
	nextState_(manager.GetAllocator(), PadButtonCount_ + PadButtonAxisCount_),
    delta_(NULL),
	deviceState_(InputDevice::DS_UNAVAILABLE),
    deviceChangeCb(NULL),
	deviceName(""),
    ioManager_(0)
{
	IOHIDManagerRef ioManager = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDManagerOptionNone);

	if (CFGetTypeID(ioManager) != IOHIDManagerGetTypeID())
	{
		IOHIDManagerClose(ioManager, 0);
		CFRelease(ioManager);
		return;
	}

	ioManager_ = ioManager;

	static const unsigned kKeyCount = 2;

	CFStringRef keys[kKeyCount] = {
		CFSTR(kIOHIDDeviceUsagePageKey),
		CFSTR(kIOHIDDeviceUsageKey),
	};

	int usagePage = kHIDPage_GenericDesktop;
	int usage = kHIDUsage_GD_GamePad;
	CFNumberRef values[kKeyCount] = {
		CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &usagePage),
		CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &usage),
	};

	CFMutableArrayRef matchingArray = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);

	CFDictionaryRef matchingDict = CFDictionaryCreate(kCFAllocatorDefault,
			(const void **) keys, (const void **) values, kKeyCount,
			&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	CFArrayAppendValue(matchingArray, matchingDict);
	CFRelease(matchingDict);
	CFRelease(values[1]);

	usage = kHIDUsage_GD_MultiAxisController;
	values[1] = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &usage);
	
	matchingDict = CFDictionaryCreate(kCFAllocatorDefault,
			(const void **) keys, (const void **) values, kKeyCount,
			&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	CFArrayAppendValue(matchingArray, matchingDict);
	CFRelease(matchingDict);
	CFRelease(values[1]);

	usage = kHIDUsage_GD_Joystick;
	values[1] = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &usage);
	
	matchingDict = CFDictionaryCreate(kCFAllocatorDefault,
			(const void **) keys, (const void **) values, kKeyCount,
			&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	CFArrayAppendValue(matchingArray, matchingDict);
	CFRelease(matchingDict);

	for (unsigned i = 0; i < kKeyCount; ++i)
	{
		CFRelease(keys[i]);
		CFRelease(values[i]);
	}

	IOHIDManagerSetDeviceMatchingMultiple(ioManager, matchingArray);
	CFRelease(matchingArray);

	IOHIDManagerRegisterDeviceMatchingCallback(ioManager, OnDeviceConnected, this);
	IOHIDManagerRegisterDeviceRemovalCallback(ioManager, OnDeviceRemoved, this);
	IOHIDManagerRegisterInputValueCallback(ioManager, OnDeviceInput, this);

	IOHIDManagerOpen(ioManager, kIOHIDOptionsTypeNone);

	IOHIDManagerScheduleWithRunLoop(ioManager, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
}

InputDevicePadImplMac::~InputDevicePadImplMac()
{
	IOHIDManagerRef ioManager = reinterpret_cast<IOHIDManagerRef>(ioManager_);
	IOHIDManagerUnscheduleFromRunLoop(ioManager, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
	IOHIDManagerClose(ioManager, 0);
	CFRelease(ioManager);
}

void InputDevicePadImplMac::Update(InputDeltaState* delta)
{
	delta_ = delta;
	state_ = nextState_;
}

bool InputDevicePadImplMac::IsValidButton(DeviceButtonId deviceButton) const
{
	if (buttonDialect_.empty())
	{
		return deviceButton < PadButtonMax_;
	}

	for (HashMap<unsigned, DeviceButtonId>::const_iterator it = buttonDialect_.begin();
			it != buttonDialect_.end();
			++it)
	{
		if (it->second == deviceButton)
		{
			return true;
		}
	}

	for (HashMap<unsigned, DeviceButtonId>::const_iterator it = axisDialect_.begin();
			it != axisDialect_.end();
			++it)
	{
		if (it->second == deviceButton)
		{
			return true;
		}
	}

	return false;
}

void InputDevicePadImplMac::SetOnDeviceChangeCallBack(void(*onDeviceChange)(const char*, bool added, int controllerId))
{
	deviceChangeCb = onDeviceChange;
	if(deviceState_ == InputDevice::DS_OK)
	{
		if(deviceChangeCb)
			(*deviceChangeCb)(GetDeviceName(), true, index_);
	}
}

const char* InputDevicePadImplMac::GetDeviceName()
{
	return deviceName;
}

}

#endif

