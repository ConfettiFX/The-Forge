
#include "../../include/gainput/gainput.h"

#ifdef GAINPUT_PLATFORM_MAC

#include "../../include/gainput/GainputMac.h"

#include "../../source/gainput/mouse/GainputInputDeviceMouseMac.h"
#include "../../source/gainput/mouse/GainputInputDeviceMouseMacRaw.h"
#include "../../source/gainput/keyboard/GainputInputDeviceKeyboardMac.h"
#import <AppKit/AppKit.h>
#import <MetalKit/MetalKit.h>

namespace gainput
{
	bool MacIsApplicationKey()
	{
		return [[NSApplication sharedApplication] keyWindow ] != nil;
	}
}


@implementation GainputMacInputView
{
	gainput::InputManager* inputManager_;
	NSWindow * parentWindow;
	NSView * parentView;
	NSTrackingArea * pTrackingArea;
	float mRetinaScale;
}

- (id)initWithFrame:(NSRect)frame window:(NSWindow *)window retinaScale:(float)retinaScale inputManager:(gainput::InputManager &)inputManager
{
	self = [super initWithFrame:window.contentView.frame];
	if(self)
	{
		parentWindow = window;
		parentView = window.contentView;
		pTrackingArea = nil;
		inputManager_ = &inputManager;
		mRetinaScale = retinaScale;
		[self bounds] = parentView.bounds;
		[self updateTrackingAreas];
	}
	
	return self;
}

- (void) dealloc
{
	[pTrackingArea release];
	[super dealloc];
}

- (void) updateTrackingAreas
{
	if (pTrackingArea != nil)
	{
		[self removeTrackingArea: pTrackingArea];
		[pTrackingArea release];
		pTrackingArea = nil;
	}

	MTKView * mtkView = (__bridge MTKView*)parentView;
	NSRect bounds = [mtkView bounds];
	NSTrackingAreaOptions options =
	(NSTrackingActiveAlways | NSTrackingInVisibleRect | NSTrackingMouseEnteredAndExited | NSTrackingMouseMoved);
	pTrackingArea = [[NSTrackingArea alloc] initWithRect: bounds
												 options: options
												   owner: self
												userInfo: nil];

	[self addTrackingArea: pTrackingArea];
	[super updateTrackingAreas];
}

// Helps with performance
- (BOOL) isOpaque
{
	return YES;
}

- (BOOL) canBecomeKeyView
{
	return YES;
}

- (BOOL) acceptsFirstResponder
{
	return YES;
}

- (gainput::InputDeviceMouseImplMac *) GetMouse
{
	if(!inputManager_)
		return NULL;
	
	gainput::DeviceId deviceId = inputManager_->FindDeviceId(gainput::InputDevice::DT_MOUSE, 0);
	if (deviceId == gainput::InvalidDeviceId)
	{
		return NULL;
	}
	gainput::InputDeviceMouse* device = static_cast<gainput::InputDeviceMouse*>(inputManager_->GetDevice(deviceId));
	if (!device)
	{
		return NULL;
	}
	gainput::InputDeviceMouseImplMac* deviceImpl = static_cast<gainput::InputDeviceMouseImplMac*>(device->GetPimpl());
	if (!deviceImpl)
	{
		return NULL;
	}
	
	return deviceImpl;
}

- (gainput::InputDeviceMouseImplMacRaw *) GetMouseRaw
{
	if(!inputManager_)
		return NULL;
	
	gainput::DeviceId deviceId = inputManager_->FindDeviceId(gainput::InputDevice::DT_MOUSE, 1);
	if (deviceId == gainput::InvalidDeviceId)
	{
		return NULL;
	}
	gainput::InputDeviceMouse* device = static_cast<gainput::InputDeviceMouse*>(inputManager_->GetDevice(deviceId));
	if (!device)
	{
		return NULL;
	}
	gainput::InputDeviceMouseImplMacRaw* deviceImpl = static_cast<gainput::InputDeviceMouseImplMacRaw*>(device->GetPimpl());
	if (!deviceImpl)
	{
		return NULL;
	}
	
	return deviceImpl;
}

- (gainput::InputDeviceKeyboardImplMac *) GetKeyboard
{
	if(!inputManager_)
		return NULL;
	
	gainput::DeviceId deviceId = inputManager_->FindDeviceId(gainput::InputDevice::DT_KEYBOARD, 0);
	if (deviceId == gainput::InvalidDeviceId)
	{
		return NULL;
	}
	gainput::InputDeviceKeyboard* device = static_cast<gainput::InputDeviceKeyboard*>(inputManager_->GetDevice(deviceId));
	if (!device)
	{
		return NULL;
	}
	gainput::InputDeviceKeyboardImplMac * deviceImpl = static_cast<gainput::InputDeviceKeyboardImplMac*>(device->GetPimpl());
	if (!deviceImpl)
	{
		return NULL;
	}
	
	return deviceImpl;
}


- (void) reportMouseButton:(gainput::MouseButton)mouseButton isPressed:(bool)isPressed
{
	gainput::InputDeviceMouseImplMac * mouse = [self GetMouse];
	
	if(!mouse)
		return;
	
	mouse->HandleMouseButton(isPressed, mouseButton);
}

- (void) reportMouseMove:(NSEvent*) nsEvent
{
	gainput::InputDeviceMouseImplMac * mouse = [self GetMouse];
	gainput::InputDeviceMouseImplMacRaw * mouseRaw = [self GetMouseRaw];
	
	if(!mouse && !mouseRaw)
		return;
	
	MTKView * mtkView = (__bridge MTKView*)parentView;
	mRetinaScale = mtkView.drawableSize.width / mtkView.frame.size.width;
	
	// Translate the cursor position into view coordinates, accounting for the fact that
	// App Kit's default window coordinate space has its origin in the bottom left
	CGPoint location = [mtkView convertPoint:[nsEvent locationInWindow] fromView:nil];
	location.y = mtkView.bounds.size.height - location.y;

	
	// Multiply the mouse coordinates by the retina scale factor.
	location.x *= mRetinaScale;
	location.y *= mRetinaScale;
	
	if(mouse)
		mouse->HandleMouseMove(location.x, location.y);
	
	if(mouseRaw)
		mouseRaw->HandleMouseMove([nsEvent deltaX], [nsEvent deltaY]);
}

- (void) mouseDown: (NSEvent*) nsEvent
{
	[self reportMouseButton:gainput::MouseButtonLeft isPressed:true];
}

- (void) mouseUp: (NSEvent*) nsEvent
{
	[self reportMouseButton:gainput::MouseButtonLeft isPressed:false];
}

- (void) mouseDragged: (NSEvent*) nsEvent
{
	[self reportMouseButton:gainput::MouseButtonLeft isPressed:true];
	[self reportMouseMove:nsEvent];
}

- (void) mouseMoved: (NSEvent*) nsEvent
{
	[self reportMouseMove:nsEvent];
}
//
//- (void) mouseEntered: (NSEvent*) nsEvent
//{
//	[self reportMouseMove:nsEvent];
//}
//
//- (void) mouseExited: (NSEvent*) nsEvent
//{
//	[self reportMouseMove:nsEvent];
//}

- (void)rightMouseDown:(NSEvent *)nsEvent
{
	[self reportMouseButton:gainput::MouseButtonRight isPressed:true];
}

- (void) rightMouseDragged: (NSEvent*) nsEvent
{
	//TODO:Differentiate from drag and click
	[self reportMouseButton:gainput::MouseButtonRight isPressed:true];
	[self reportMouseMove:nsEvent];
}

- (void) rightMouseUp: (NSEvent*)nsEvent
{
	[self reportMouseButton:gainput::MouseButtonRight isPressed:false];
}

- (void) scrollWheel: (NSEvent*) nsEvent
{
	if (static_cast<int>([nsEvent deltaY]) == 0)
	{
		return;
	}
	
	
	gainput::InputDeviceMouseImplMac * mouse = [self GetMouse];
	
	if(!mouse)
		return;
	
	mouse->HandleMouseWheel(static_cast<int>([nsEvent deltaY]));
	
}

// Handle key events from the NSResponder protocol
- (void) keyDown: (NSEvent*) nsEvent
{
	gainput::InputDeviceKeyboardImplMac * keyboard = [self GetKeyboard];
	
	if(!keyboard)
		return;
	
	char currentChar = '\0';
	//if using characters and we have modifiers such as ctrl where it shouldn't print text
	//we won't have any input in characters string but it will be in charactersIgnoringModifiers
	//It will still have correct Case if shift is pressed.
	if([nsEvent.charactersIgnoringModifiers length] > 0)
		currentChar =[nsEvent.charactersIgnoringModifiers characterAtIndex:0];
	
	keyboard->HandleKey((int)nsEvent.keyCode,true, &currentChar);
}

- (void) keyUp: (NSEvent*) nsEvent
{
	gainput::InputDeviceKeyboardImplMac * keyboard = [self GetKeyboard];
	
	if(!keyboard)
		return;
	
	keyboard->HandleKey((int)nsEvent.keyCode,false, NULL);
}

// Modifier keys do not trigger keyUp/Down events but only flagsChanged events.
- (void) flagsChanged: (NSEvent*) nsEvent
{
	//unsigned short keyCode = [event nsEvent];
	gainput::InputDeviceKeyboardImplMac * keyboard = [self GetKeyboard];
	
	if(!keyboard)
		return;
	
	keyboard->HandleFlagsChanged((int)nsEvent.keyCode);
}

@end
#endif
