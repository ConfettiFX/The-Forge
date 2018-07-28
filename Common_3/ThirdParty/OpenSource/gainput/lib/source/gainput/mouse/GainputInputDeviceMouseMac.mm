
#include "../../../include/gainput/gainput.h"

#ifdef GAINPUT_PLATFORM_MAC

#include "GainputInputDeviceMouseMac.h"

#include "../../../include/gainput/GainputInputDeltaState.h"
#include "../../../include/gainput/GainputHelpers.h"
#include "../../../include/gainput/GainputLog.h"

#import <AppKit/AppKit.h>
#import <Carbon/Carbon.h>
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>

#import <CoreFoundation/CoreFoundation.h>
#import <IOKit/hid/IOHIDManager.h>
#import <IOKit/hid/IOHIDUsageTables.h>

namespace {

	static void OnDeviceInput(void* inContext, IOReturn inResult, void* inSender, IOHIDValueRef inValue)
	{
		gainput::InputDeviceMouseImplMac* device = reinterpret_cast<gainput::InputDeviceMouseImplMac*>(inContext);
		GAINPUT_ASSERT(device);
		
		IOHIDElementRef elem = IOHIDValueGetElement(inValue);
		
		const CFIndex value = IOHIDValueGetIntegerValue(inValue);
		const uint32_t page = IOHIDElementGetUsagePage(elem);
		const uint32_t usage = IOHIDElementGetUsage(elem);
		
		
		if (page == kHIDPage_GenericDesktop)
		{
			
			/*
			 * some devices (two-finger-scroll trackpads?) seem to give
			 *  a flood of events with values of zero for every legitimate
			 *  event. Throw these zero events out.
			 */
			if (value != 0)
			{
				switch (usage)
				{
					case kHIDUsage_GD_X:
					{
						
						gainput::InputManager * manager = &device->manager_;
						if(!manager->uiView_)
							return;
						
						NSWindow * currWind = nil;
						for (NSWindow *window in [NSApplication sharedApplication].windows) {
							if(window.isKeyWindow)
							{
								currWind = window;
								break;
							}
						}
						
						if(!currWind)
							return;
						
						//convert to window coordinates
						NSPoint mouseLoc = [NSEvent mouseLocation];
						NSRect screenRect = CGRectMake(mouseLoc.x, mouseLoc.y, 1.0, 1.0);
						NSRect baseRect = [currWind convertRectFromScreen:screenRect];
						MTKView * view = (__bridge MTKView*)(manager->uiView_);
						NSPoint pointInView = [view convertPoint:baseRect.origin fromView:view];
						
						float retinaScale = view.drawableSize.width / view.frame.size.width;
						
						manager->EnqueueConcurrentChange(device->device_, device->nextState_, device->delta_, gainput::MouseAxisX,(float)pointInView.x * retinaScale);
						
					}
						break;
					case kHIDUsage_GD_Y:
					{
						gainput::InputManager * manager = &device->manager_;
						if(!manager->uiView_)
							return;
						
						NSWindow * currWind = nil;
						for (NSWindow *window in [NSApplication sharedApplication].windows) {
							if(window.isKeyWindow)
							{
								currWind = window;
								break;
							}
						}
						
						if(!currWind)
							return;
						
						//convert to window coordinates
						NSPoint mouseLoc = [NSEvent mouseLocation];
						NSRect screenRect = CGRectMake(mouseLoc.x, mouseLoc.y, 1.0, 1.0);
						NSRect baseRect = [currWind convertRectFromScreen:screenRect];
						MTKView * view = (__bridge MTKView*)(manager->uiView_);
						NSPoint pointInView = [view convertPoint:baseRect.origin fromView:view];
						//CGPoint location = [view convertPoint:mouseLoc fromView:nil];
						
						float retinaScale = view.drawableSize.width / view.frame.size.width;
						
						manager->EnqueueConcurrentChange(device->device_, device->nextState_, device->delta_, gainput::MouseAxisY, ((float)view.bounds.size.height - (float)pointInView.y) * retinaScale);
					}
						break;
					case kHIDUsage_GD_Wheel:
						//TODO: Fix
						break;
					default:
						break;
				}
			}
		}
		else if (page == kHIDPage_Button)
		{
			const uint32_t buttonID = gainput::MouseButtonLeft + (usage - kHIDUsage_Button_1);
			if(buttonID >= gainput::MouseButtonMax)
				return;
			
			gainput::InputManager * manager = &device->manager_;
			manager->EnqueueConcurrentChange(device->device_, device->nextState_, device->delta_, buttonID, value > 0 ? true : false);
		}
	}
	
	static void OnDeviceConnected(void* inContext, IOReturn inResult, void* inSender, IOHIDDeviceRef inIOHIDDeviceRef)
	{
		gainput::InputDeviceMouseImplMac* device = reinterpret_cast<gainput::InputDeviceMouseImplMac*>(inContext);
		GAINPUT_ASSERT(device);
		device->deviceState_ = gainput::InputDevice::DS_OK;
	}
	
	static void OnDeviceRemoved(void* inContext, IOReturn inResult, void* inSender, IOHIDDeviceRef inIOHIDDeviceRef)
	{
		gainput::InputDeviceMouseImplMac* device = reinterpret_cast<gainput::InputDeviceMouseImplMac*>(inContext);
		GAINPUT_ASSERT(device);
		device->deviceState_ = gainput::InputDevice::DS_UNAVAILABLE;
	}
}

namespace gainput
{
	
	InputDeviceMouseImplMac::InputDeviceMouseImplMac(InputManager& manager, InputDevice& device, InputState& state, InputState& previousState) :
	manager_(manager),
	device_(device),
	state_(&state),
	previousState_(&previousState),
	nextState_(manager.GetAllocator(), MouseButtonCount + MouseAxisCount),
	delta_(0)
	{
		
		IOHIDManagerRef ioManager = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDManagerOptionNone);
		
		if (CFGetTypeID(ioManager) != IOHIDManagerGetTypeID())
		{
			return;
		}
		
		ioManager_ = ioManager;
		
		static const unsigned kKeyCount = 2;
		
		int usagePage = kHIDPage_GenericDesktop;
		int usage = kHIDUsage_GD_Mouse;
		
		CFStringRef keys[kKeyCount] = {
			CFSTR(kIOHIDDeviceUsagePageKey),
			CFSTR(kIOHIDDeviceUsageKey),
		};
		
		CFNumberRef values[kKeyCount] = {
			CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &usagePage),
			CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &usage),
		};
		
		CFDictionaryRef matchingDict = CFDictionaryCreate(kCFAllocatorDefault,
														  (const void **) keys, (const void **) values, kKeyCount,
														  &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
		
		for (unsigned i = 0; i < kKeyCount; ++i)
		{
			CFRelease(keys[i]);
			CFRelease(values[i]);
		}
		
		IOHIDManagerSetDeviceMatching(ioManager, matchingDict);
		CFRelease(matchingDict);
		
		IOHIDManagerRegisterDeviceMatchingCallback(ioManager, OnDeviceConnected, this);
		IOHIDManagerRegisterDeviceRemovalCallback(ioManager, OnDeviceRemoved, this);
		IOHIDManagerRegisterInputValueCallback(ioManager, OnDeviceInput, this);
		IOHIDManagerOpen(ioManager, kIOHIDManagerOptionNone);
		
		IOHIDManagerScheduleWithRunLoop(ioManager, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
	}
	
	InputDeviceMouseImplMac::~InputDeviceMouseImplMac()
	{
		
		IOHIDManagerRef ioManager = reinterpret_cast<IOHIDManagerRef>(ioManager_);
		IOHIDManagerUnscheduleFromRunLoop(ioManager, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
		IOHIDManagerClose(ioManager, 0);
		CFRelease(ioManager);
	}
	
	
	void InputDeviceMouseImplMac::Update(InputDeltaState* delta)
	{
		delta_ = delta;
		
		// Reset mouse wheel buttons
		//    HandleButton(device_, nextState_, delta_, MouseButtonWheelDown, false);
		//    HandleButton(device_, nextState_, delta_, MouseButtonWheelUp, false);
		//    HandleButton(device_, nextState_, delta_, MouseButtonWheelDown, false);
		
		*state_ = nextState_;
	}
	
}
#endif

