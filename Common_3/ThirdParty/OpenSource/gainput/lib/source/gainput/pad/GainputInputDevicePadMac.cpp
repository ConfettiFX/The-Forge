
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

static const unsigned kMaxPads = 16;
static bool usedPadIndices_[kMaxPads] = { false };

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

    InputManager& manager = device->manager_;
	CFIndex state = (int)IOHIDValueGetIntegerValue(value);
	float analog = IOHIDValueGetScaledValue(value, kIOHIDValueScaleTypePhysical);

	if (usagePage == kHIDPage_Button && device->buttonDialect_.count(usage))
	{
		const DeviceButtonId buttonId = device->buttonDialect_[usage];
		manager.EnqueueConcurrentChange(device->device_, device->nextState_, device->delta_, buttonId, state != 0);
	}
	else if (usagePage == kHIDPage_GenericDesktop)
	{
		if (usage == kHIDUsage_GD_Hatswitch)
		{
			int dpadX = 0;
			int dpadY = 0;
			switch(state)
			{
				case  0: dpadX =  0; dpadY =  1; break;
				case  1: dpadX =  1; dpadY =  1; break;
				case  2: dpadX =  1; dpadY =  0; break;
				case  3: dpadX =  1; dpadY = -1; break;
				case  4: dpadX =  0; dpadY = -1; break;
				case  5: dpadX = -1; dpadY = -1; break;
				case  6: dpadX = -1; dpadY =  0; break;
				case  7: dpadX = -1; dpadY =  1; break;
				default: dpadX =  0; dpadY =  0; break;
			}
			manager.EnqueueConcurrentChange(device->device_, device->nextState_, device->delta_, PadButtonLeft, dpadX < 0);
			manager.EnqueueConcurrentChange(device->device_, device->nextState_, device->delta_, PadButtonRight, dpadX > 0);
			manager.EnqueueConcurrentChange(device->device_, device->nextState_, device->delta_, PadButtonUp, dpadY > 0);
			manager.EnqueueConcurrentChange(device->device_, device->nextState_, device->delta_, PadButtonDown, dpadY < 0);
		}
		else if (device->axisDialect_.count(usage))
		{
			const DeviceButtonId buttonId = device->axisDialect_[usage];
			if (buttonId == PadButtonAxis4 || buttonId == PadButtonAxis5)
			{
				analog = FixUpAnalog(analog, device->minTriggerAxis_, device->maxTriggerAxis_, false);
			}
			else if (buttonId == PadButtonLeftStickY || buttonId == PadButtonRightStickY)
			{
				analog = -FixUpAnalog(analog, device->minAxis_, device->maxAxis_, true);
			}
			else
			{
				analog = FixUpAnalog(analog, device->minAxis_, device->maxAxis_, true);
			}
			manager.EnqueueConcurrentChange(device->device_, device->nextState_, device->delta_, buttonId, analog);
		}
		else if (device->buttonDialect_.count(usage))
		{
			const DeviceButtonId buttonId = device->buttonDialect_[usage];
			manager.EnqueueConcurrentChange(device->device_, device->nextState_, device->delta_, buttonId, state != 0);
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

	for (unsigned i = 0; i < device->index_ && i < kMaxPads; ++i)
	{
		if (!usedPadIndices_[i])
		{
			return;
		}
	}

	if (device->index_ < kMaxPads)
	{
		usedPadIndices_[device->index_] = true;
	}
	device->deviceState_ = InputDevice::DS_OK;

	long vendorId = 0;
	long productId = 0;

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

	if (vendorId == 0x054c && (productId == 0x5c4 || productId == 0x9cc)) // Sony DualShock 4
	{
		device->minAxis_ = 0;
		device->maxAxis_ = 256;
		device->minTriggerAxis_ = device->minAxis_;
		device->maxTriggerAxis_ = device->maxAxis_;
		device->axisDialect_[kHIDUsage_GD_X] = PadButtonLeftStickX;
		device->axisDialect_[kHIDUsage_GD_Y] = PadButtonLeftStickY;
		device->axisDialect_[kHIDUsage_GD_Z] = PadButtonRightStickX;
		device->axisDialect_[kHIDUsage_GD_Rz] = PadButtonRightStickY;
		device->axisDialect_[kHIDUsage_GD_Rx] = PadButtonAxis4;
		device->axisDialect_[kHIDUsage_GD_Ry] = PadButtonAxis5;
		device->buttonDialect_[0x09] = PadButtonSelect;
		device->buttonDialect_[0x0b] = PadButtonL3;
		device->buttonDialect_[0x0c] = PadButtonR3;
		device->buttonDialect_[0x0A] = PadButtonStart;
		device->buttonDialect_[0xfffffff0] = PadButtonUp;
		device->buttonDialect_[0xfffffff1] = PadButtonRight;
		device->buttonDialect_[0xfffffff2] = PadButtonDown;
		device->buttonDialect_[0xfffffff3] = PadButtonLeft;
		device->buttonDialect_[0x05] = PadButtonL1;
        device->buttonDialect_[0x07] = PadButtonL2;
		device->buttonDialect_[0x06] = PadButtonR1;
		device->buttonDialect_[0x08] = PadButtonR2;
		device->buttonDialect_[0x04] = PadButtonY;
		device->buttonDialect_[0x03] = PadButtonB;
		device->buttonDialect_[0x02] = PadButtonA;
		device->buttonDialect_[0x01] = PadButtonX;
		device->buttonDialect_[0x0d] = PadButtonHome;
        device->buttonDialect_[0x0e] = PadButton17; // Touch pad
	}
	else if (vendorId == 0x054c && productId == 0x268) // Sony DualShock 3
	{
		device->minAxis_ = 0;
		device->maxAxis_ = 256;
		device->minTriggerAxis_ = device->minAxis_;
		device->maxTriggerAxis_ = device->maxAxis_;
		device->axisDialect_[kHIDUsage_GD_X] = PadButtonLeftStickX;
		device->axisDialect_[kHIDUsage_GD_Y] = PadButtonLeftStickY;
		device->axisDialect_[kHIDUsage_GD_Z] = PadButtonRightStickX;
		device->axisDialect_[kHIDUsage_GD_Rz] = PadButtonRightStickY;
		device->axisDialect_[kHIDUsage_GD_Rx] = PadButtonAxis4;
		device->axisDialect_[kHIDUsage_GD_Ry] = PadButtonAxis5;
		//device->buttonDialect_[0] = PadButtonSelect;
		device->buttonDialect_[2] = PadButtonL3;
		device->buttonDialect_[3] = PadButtonR3;
		device->buttonDialect_[4] = PadButtonStart;
		device->buttonDialect_[5] = PadButtonUp;
		device->buttonDialect_[6] = PadButtonRight;
		device->buttonDialect_[7] = PadButtonDown;
		device->buttonDialect_[8] = PadButtonLeft;
		device->buttonDialect_[11] = PadButtonL1;
		device->buttonDialect_[9] = PadButtonL2;
		device->buttonDialect_[12] = PadButtonR1;
		device->buttonDialect_[10] = PadButtonR2;
		device->buttonDialect_[13] = PadButtonY;
		device->buttonDialect_[14] = PadButtonB;
		device->buttonDialect_[15] = PadButtonA;
		device->buttonDialect_[16] = PadButtonX;
		device->buttonDialect_[17] = PadButtonHome;
	}
	else if (vendorId == 0x045e && (productId == 0x028E || productId == 0x028F || productId == 0x02D1)) // Microsoft 360 Controller wired/wireless, Xbox One Controller
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
}

static void OnDeviceRemoved(void* inContext, IOReturn inResult, void* inSender, IOHIDDeviceRef inIOHIDDeviceRef)
{
	InputDevicePadImplMac* device = reinterpret_cast<InputDevicePadImplMac*>(inContext);
	GAINPUT_ASSERT(device);
	device->deviceState_ = InputDevice::DS_UNAVAILABLE;
	if (device->index_ < kMaxPads)
	{
		usedPadIndices_[device->index_] = true;
	}
}

}

InputDevicePadImplMac::InputDevicePadImplMac(InputManager& manager, InputDevice& device, unsigned index, InputState& state, InputState& previousState) :
	buttonDialect_(manager.GetAllocator()),
	axisDialect_(manager.GetAllocator()),
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

}

#endif

