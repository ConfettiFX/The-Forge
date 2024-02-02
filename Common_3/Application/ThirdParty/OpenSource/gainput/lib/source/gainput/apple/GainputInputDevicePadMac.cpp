
#include "../../../include/gainput/gainput.h"

#ifdef GAINPUT_PLATFORM_MAC

#include "../pad/GainputInputDevicePadImpl.h"
#include "../../../include/gainput/GainputInputDeltaState.h"
#include "../../../include/gainput/GainputHelpers.h"
#include "../../../include/gainput/GainputLog.h"

#include "GainputInputDevicePadMac.h"

#import <CoreFoundation/CoreFoundation.h>
#import <IOKit/hid/IOHIDManager.h>
#import <IOKit/hid/IOHIDUsageTables.h>

#include "../../../../../../../../Utilities/Interfaces/IThread.h"


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

// takes into account 2d vector of an axis instead of just the single float value for improved deadzone
inline void HandleCustomAxis(InputDevice& device, InputState& state, InputDeltaState* delta, DeviceButtonId buttonId, float value, DeviceButtonId secondaryButtonId, float secondValue)
{
	const float deadZone = device.GetDeadZone(buttonId);
	if (deadZone > 0.0f)
	{
		//use scaled radial dead zone instead of simple axial dead zone
		//fixes issues with precision
		const float magnitude = sqrtf(value*value + secondValue*secondValue);
		if (magnitude < deadZone)
		{
			value = 0.0f;
		}
		else
		{
			value = (value / magnitude) * ((magnitude - deadZone) / (1.0f - deadZone));
		}
	}

#ifdef GAINPUT_DEBUG
	if (value != state.GetFloat(buttonId))
	{
		GAINPUT_LOG("Axis changed: %d, %f\n", buttonId, value);
	}
#endif

	//primary axis change
	if (delta)
	{
		const float oldValue = state.GetFloat(buttonId);
		delta->AddChange(device.GetDeviceId(), buttonId, oldValue, value);
	}
	state.Set(buttonId, value);


	// Secondary axis value
	if (delta)
	{
		const float oldValue = state.GetFloat(secondaryButtonId);
		delta->AddChange(device.GetDeviceId(), secondaryButtonId, oldValue, secondValue);
	}
	state.Set(secondaryButtonId, secondValue);
}

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
	InputDevicePadImplMac* device = reinterpret_cast<InputDevicePadImplMac*>(inContext);
	GAINPUT_ASSERT(device);
	if (!MacIsApplicationKey())
	{
		return;
	}

	if (device->deviceState_ != InputDevice::DS_OK)
	{
		return;
	}

	IOHIDElementRef elem = IOHIDValueGetElement(value);
	// get the actual IOHID device that triggered a value update
	IOHIDDeviceRef  deviceRefIn = IOHIDElementGetDevice(elem);
	if(device->pIOHIDRef_ == NULL || device->pIOHIDRef_ != deviceRefIn)
		return;

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
	// select button on some Xbox variants uses KHIDPage_Consumer
	if ((usagePage == kHIDPage_Button || usagePage == kHIDPage_Consumer) && device->buttonDialect_.count(usage))
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
					HandleCustomAxis(device->device_, device->nextState_, device->delta_, buttonId, analog, PadButtonLeftStickX, leftStickX);
				}
				else
				{
					HandleCustomAxis(device->device_, device->nextState_, device->delta_, buttonId, analog, PadButtonRightStickX, rightStickX);
				}
			}
			else if (buttonId == PadButtonLeftStickX || buttonId == PadButtonRightStickX)
			{
				analog = FixUpAnalog(analog, IOHIDElementGetPhysicalMin(elem), IOHIDElementGetPhysicalMax(elem), true);

				if(buttonId == PadButtonLeftStickX)
				{
					HandleCustomAxis(device->device_, device->nextState_, device->delta_, buttonId, analog, PadButtonLeftStickY, leftStickY);
				}
				else
				{
					HandleCustomAxis(device->device_, device->nextState_, device->delta_, buttonId, analog, PadButtonRightStickY, rightStickY);
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

	//already assigned to another IOHID device
	if(device->pIOHIDRef_ != NULL)
		return;

	long vendorId = 0;
	long productId = 0;
	device->deviceName[0] = device->deviceName[InputDevicePadImplMac::MAX_NAME_SIZE - 1] = '\0';
	device->serialNumber[0] = device->serialNumber[InputDevicePadImplMac::MAX_NAME_SIZE - 1] = '\0';

	CFStringEncoding encodingMethod = CFStringGetSystemEncoding();
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
			CFStringGetCString((CFStringRef)tCFTypeRef, &device->serialNumber[0], (CFIndex)InputDevicePadImplMac::MAX_NAME_SIZE, encodingMethod);
		}
	}
	if (CFTypeRef tCFTypeRef = IOHIDDeviceGetProperty(inIOHIDDeviceRef, CFSTR(kIOHIDProductKey) ))
	{
		if (CFStringGetTypeID() == CFGetTypeID(tCFTypeRef))
		{
			CFStringGetCString((CFStringRef)tCFTypeRef, &device->deviceName[0], (CFIndex)InputDevicePadImplMac::MAX_NAME_SIZE, encodingMethod);
		}
	}
	// MAke sure we terminate by null character in case of crazy name length
	device->deviceName[InputDevicePadImplMac::MAX_NAME_SIZE - 1] = '\0';
	device->serialNumber[InputDevicePadImplMac::MAX_NAME_SIZE - 1] = '\0';

	if(device->serialNumber[0] == '\0' || device->deviceName[0] == '\0')
		return;

	for (unsigned i = 0; i < device->index_ && i < kMaxPads; ++i)
	{
		if (sDeviceIdToPadSerials[i] != NULL && strcmp(device->serialNumber, sDeviceIdToPadSerials[i]) == 0)
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
		// different virtual keycodes mapped to select on different xbox controllers
		device->buttonDialect_[0x0b] = PadButtonSelect;
		device->buttonDialect_[0x224] = PadButtonSelect;
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
		device->deviceName[0] = '\0';
		device->serialNumber[0] = '\0';
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
	device->pIOHIDRef_ = inIOHIDDeviceRef;
	DeviceId newId = device->manager_.GetNextId();
	device->device_.OverrideDeviceId(newId);
	device->manager_.AddDevice(newId, &device->device_);
}

static void OnDeviceRemoved(void* inContext, IOReturn inResult, void* inSender, IOHIDDeviceRef inIOHIDDeviceRef)
{
	InputDevicePadImplMac* device= reinterpret_cast<InputDevicePadImplMac*>(inContext);
	GAINPUT_ASSERT(device);
	if(device->pIOHIDRef_ != inIOHIDDeviceRef)
		return;

	if(device->deviceState_ != InputDevice::DS_OK)
		return;

	//check that index of pad is still valid
	if(device->index_ >= kMaxPads)
	{
		device->deviceState_ = InputDevice::DS_UNAVAILABLE;
		device->deviceChangeCb = nil;
		return;
	}
	device->deviceState_ = InputDevice::DS_UNAVAILABLE;
	device->pIOHIDRef_ = NULL;
	device->manager_.RemoveDevice(device->device_.GetDeviceId());
	device->device_.OverrideDeviceId(-1);

	if (device->index_ < kMaxPads)
	{
		sDeviceIdToPadSerials[device->index_] = 0;
	}
}

}

static bool gHIDThreadInit = false;
static bool gQuitRumbleThread = false;
Mutex rumbleLockMutex = {PTHREAD_MUTEX_INITIALIZER, 0};
ConditionVariable rumbleDataAcquiredLock = PTHREAD_COND_INITIALIZER;
uint32_t mRumbleDataQueued = 0;
ThreadHandle mRumbleThreadHandle;
// We shouldn't be calling this multiple times in a single frames
// Add 4 just in case
InputDevicePadImplMac::RumbleData rumbleDataToPush[kMaxPads];
static void RumbleThreadfunc(void* pThreadData)
{
	while(!gQuitRumbleThread)
	{
		acquireMutex(&rumbleLockMutex);
		waitConditionVariable(&rumbleDataAcquiredLock, &rumbleLockMutex, TIMEOUT_INFINITE);
		while(mRumbleDataQueued > 0)
		{
			int lRumbleDataIndex = --mRumbleDataQueued;
			InputDevicePadImplMac::RumbleData * rumbleData = &rumbleDataToPush[lRumbleDataIndex];
			if(rumbleData->pPadRef->GetState() != InputDevice::DS_OK)
				continue;
			// magic packet that seems to work with bt xbox one controller
			UInt8 rumble_packet[] = { 0x03, 0x0F, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00 };
			// packet at index 4 is left motor.
			rumble_packet[4] = (uint8_t)(rumbleData->fLeftMotor * 255);
			// packet at 5 is right motor.
			rumble_packet[5] = (uint8_t)(rumbleData->fRightMotor * 255);

			uint32_t duration_packet = (rumbleData->durationMs / 10);
			uint8_t loopCount = duration_packet ;
			uint8_t duration10Ms = 1; // every unit here is 10ms
			// if we're overflowing (checking on uint32 instead of uint8)
			if (duration_packet > 255)
			{
				duration_packet = duration_packet / 255;
				// in case of huge duration packet
				// if duration packet / 255 > 255, then use max value of 255
				duration10Ms = (uint8_t)(duration_packet > 255 ? 255 : duration_packet);
				loopCount = 255;
			}
			else
			{
				duration10Ms = duration_packet;
				loopCount = 0;
			}
			// 255 = 2.55 seconds max value
			rumble_packet[6] = duration10Ms; // 10ms
			// loop count.
			rumble_packet[8] = loopCount;

			if(rumbleData->pIOHIDRef_ == NULL)
				continue;
			IOReturn ret = IOHIDDeviceSetReport((IOHIDDeviceRef)rumbleData->pIOHIDRef_, kIOHIDReportTypeOutput, rumble_packet[0], rumble_packet, sizeof(rumble_packet));
			if(ret != kIOReturnSuccess)
			{
				printf("Error sending rumble report for controller. check return code %d\n", ret);
				continue;
			}
		}
		releaseMutex(&rumbleLockMutex);
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
	ioManager_(0),
	pIOHIDRef_(NULL)
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

	/* Thread objects */
	if(!gHIDThreadInit)
	{
		gHIDThreadInit = true;
		gQuitRumbleThread = false;
		initMutex(&rumbleLockMutex);
		initConditionVariable(&rumbleDataAcquiredLock);
		mRumbleDataQueued = 0;
		ThreadDesc desc{};
		snprintf(desc.mThreadName, 31, "HID Rumble");
		desc.pData = NULL;
		desc.pFunc = RumbleThreadfunc;
		initThread(&desc, &mRumbleThreadHandle);
	}
}

InputDevicePadImplMac::~InputDevicePadImplMac()
{
	if(gHIDThreadInit)
	{
		gQuitRumbleThread = true;
		wakeOneConditionVariable(&rumbleDataAcquiredLock);
		joinThread(mRumbleThreadHandle);
		destroyConditionVariable(&rumbleDataAcquiredLock);
		destroyMutex(&rumbleLockMutex);
		gHIDThreadInit = false;
	}
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

bool InputDevicePadImplMac::SetRumbleEffect(float leftMotor, float rightMotor, uint32_t duration_ms, bool targetOwningDevice)
{
	if(deviceState_ == InputDevice::DS_UNAVAILABLE)
		return false;
	if(!pIOHIDRef_)
		return false;

	if(mRumbleDataQueued < kMaxPads)
	{
		acquireMutex(&rumbleLockMutex);
		rumbleDataToPush[mRumbleDataQueued].durationMs = duration_ms;
		rumbleDataToPush[mRumbleDataQueued].fLeftMotor = leftMotor;
		rumbleDataToPush[mRumbleDataQueued].fRightMotor = rightMotor;
		rumbleDataToPush[mRumbleDataQueued].pIOHIDRef_ = pIOHIDRef_;
		rumbleDataToPush[mRumbleDataQueued].pPadRef = this;
		mRumbleDataQueued++;
		releaseMutex(&rumbleLockMutex);
		wakeOneConditionVariable(&rumbleDataAcquiredLock);
	}
	return true;
}

const char* InputDevicePadImplMac::GetDeviceName()
{
	return deviceName;
}

}

#endif

