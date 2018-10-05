
#include "../../include/gainput/gainput.h"

#if defined(GAINPUT_PLATFORM_IOS) || defined(GAINPUT_PLATFORM_TVOS)

#include "../../include/gainput/GainputIos.h"

#include "touch/GainputInputDeviceTouchIos.h"
#include "keyboard/GainputInputDeviceKeyboardIOS.h"

@implementation KeyboardView
{
	NSMutableString *inputString_;
	gainput::InputManager * inputManager_;
}


- (id)initWithFrame:(CGRect)frame  inputManager:(gainput::InputManager&)inputManager{
	
	if ((self = [super initWithFrame:frame])) {
		// Initialization code
		inputString_ = [[NSMutableString alloc] init];
		[inputString_ setString:@""];
		inputManager_ = &inputManager;
		
		// hide undo, redo, paste button bar for current KeyboardView
		UITextInputAssistantItem* item = [self inputAssistantItem];
		item.leadingBarButtonGroups = @[];
		item.trailingBarButtonGroups = @[];
		
		
		//disable autocorrect
		[self setAutocorrectionType:UITextAutocorrectionTypeNo];
		
		//set up autoresizing masks
		self.autoresizingMask = UIViewAutoresizingFlexibleHeight | UIViewAutoresizingFlexibleWidth;
		self.translatesAutoresizingMaskIntoConstraints = YES;

	}
	return self;
}

- (BOOL)canBecomeFirstResponder { return YES; }

- (BOOL)hasText {
	if ([self->inputString_ length] > 0) {
		return YES;
	}
	return NO;
}

- (BOOL)isOpaque {
	return NO;
}

- (void)registerKeyboardNotifications
{
	//register to event triggered when keyboard will be shown
	[[NSNotificationCenter defaultCenter]
	 addObserver:self
	 selector:@selector(keyboardWillShow:)
	 name:UIKeyboardDidShowNotification
	 object:nil];

	//register to event triggered when keyboard will be hidden
	[[NSNotificationCenter defaultCenter]
	 addObserver:self
	 selector:@selector(keyboardWillHide:)
	 name:UIKeyboardWillHideNotification
	 object:nil];
}

-(void) dealloc
{
	[self unregisterKeyboardNotifications];
	[self->inputString_ dealloc];
	[super dealloc];
}

-(void) reset
{
	[self->inputString_ setString:@""];
}

- (void)unregisterKeyboardNotifications
{
	[[NSNotificationCenter defaultCenter] removeObserver:self];
}


-(void) keyboardWillHide:(NSNotification *)nsNotification
{
	//[self.superview.bottomAnchor constraintEqualToAnchor:self.topAnchor  constant: 1.0].active = false;
}

-(void) keyboardWillShow:(NSNotification *)nsNotification
{
	//[self.superview.bottomAnchor constraintEqualToAnchor:self.topAnchor  constant: 1.0].active = true;
}

- (void)deleteBackward {
	if([self->inputString_ length] > 0)
	{
		NSRange range = NSMakeRange([self->inputString_ length]-1, 1);
		[self->inputString_ deleteCharactersInRange:range];
	}
	
	gainput::DeviceId deviceId = inputManager_->FindDeviceId(gainput::InputDevice::DT_KEYBOARD, 0);
	if (deviceId == gainput::InvalidDeviceId)
	{
		return;
	}
	gainput::InputDeviceKeyboard* device = static_cast<gainput::InputDeviceKeyboard*>(inputManager_->GetDevice(deviceId));
	if (!device)
	{
		return;
	}
	gainput::InputDeviceKeyboardImplIOS* deviceImpl = static_cast<gainput::InputDeviceKeyboardImplIOS*>(device->GetPimpl());
	if (!deviceImpl)
	{
		return;
	}
	deviceImpl->TriggerBackspace();
	
	[super deleteBackward];
}

- (void)insertText:(nonnull NSString *)text {
	[self->inputString_ appendString:text];
	
	gainput::DeviceId deviceId = inputManager_->FindDeviceId(gainput::InputDevice::DT_KEYBOARD, 0);
	if (deviceId == gainput::InvalidDeviceId)
	{
		return;
	}
	gainput::InputDeviceKeyboard* device = static_cast<gainput::InputDeviceKeyboard*>(inputManager_->GetDevice(deviceId));
	if (!device)
	{
		return;
	}
	gainput::InputDeviceKeyboardImplIOS* deviceImpl = static_cast<gainput::InputDeviceKeyboardImplIOS*>(device->GetPimpl());
	if (!deviceImpl)
	{
		return;
	}
	const char * utfString = [text UTF8String];
	if(utfString)
		deviceImpl->HandleKey(utfString[0]);
}
@end

@implementation GainputView
{
	gainput::InputManager* inputManager_;
}

- (id)initWithFrame:(CGRect)frame inputManager:(gainput::InputManager&)inputManager
{
	self = [super initWithFrame:frame];
	if (self)
	{
		inputManager_ = &inputManager;
        
#if !defined(GAINPUT_PLATFORM_TVOS)
		[self setMultipleTouchEnabled:YES];
#endif
		self.pKeyboardView = [[[KeyboardView alloc] initWithFrame:frame inputManager:inputManager] autorelease];
		self.pKeyboardView.hidden = true;
		
		gainput::DeviceId deviceId = inputManager_->FindDeviceId(gainput::InputDevice::DT_TOUCH, 0);
		if (deviceId != gainput::InvalidDeviceId)
		{
			gainput::InputDeviceTouch* device = static_cast<gainput::InputDeviceTouch*>(inputManager_->GetDevice(deviceId));
			if (device)
			{
				gainput::InputDeviceTouchImplIos* deviceImpl = static_cast<gainput::InputDeviceTouchImplIos*>(device->GetPimpl());
				if (deviceImpl)
				{
					bool supports = false;
					UIWindow* window = [[[UIApplication sharedApplication] delegate] window];
					if (window)
					{
						UIViewController* rvc = [window rootViewController];
						if (rvc)
						{
							UITraitCollection* tc = [rvc traitCollection];
							if (tc && [tc respondsToSelector:@selector(forceTouchCapability)])
							{
								supports = [tc forceTouchCapability] == UIForceTouchCapabilityAvailable;
							}
						}
					}
					deviceImpl->SetSupportsPressure(supports);
				}
			}
		}
	}
	return self;
}

/*
 Function to Display/Hide virtual keyboard.
 Argument: type:
 0 --> disable Keyboard
 1 --> Digits only keyboard
 2 --> ASCII Keyboard
 Returns whether or not virtual keyboard toggle was succesfull
 Will only fail if keyboard wasn't allocated for some reason.
 */
- (BOOL)setVirtualKeyboard:(int)type
{
	//Can't set virtual keyboard if not allocated
	if(!self.pKeyboardView)
		return false;
	
	if(type > 0)
	{
		//add to current view otherwise it doesn't get displayed since it won't be in view hierarchy
		[self addSubview:self.pKeyboardView];
		if(type == 1)
		{
			//Set Keyboard type to Digits only.
			//on iPad this does not give us number pad but just makes the current keyboard
			//display the numbers tab
			//self.pKeyboardView.autocapitalizationType = UITextAutocapitalizationTypeNone;
			self.pKeyboardView.keyboardType = UIKeyboardTypeASCIICapableNumberPad;
		}
		else
		{
			//Set Keyboard type to regular keyboard and capitalize first word of sentences
			//self.pKeyboardView.autocapitalizationType = UITextAutocapitalizationTypeSentences;
			self.pKeyboardView.keyboardType = UIKeyboardTypeASCIICapable;
		}
		//add event handler for when the keyboard gets hidden or gets shown
		[self.pKeyboardView registerKeyboardNotifications];
		
		//Set background color to clear color
		//Set appearance to dark mode, looks much better and has better transparency
		self.pKeyboardView.backgroundColor = [UIColor clearColor];
		[self.pKeyboardView setKeyboardAppearance:UIKeyboardAppearanceDark];
		//this is the function that display the keyboard
		[self.pKeyboardView becomeFirstResponder];
		//update layout if required.
		//need to update contraints of metal view here.
		[self.pKeyboardView layoutIfNeeded];
	}
	else
	{
		//resigning first responders for UITextView hides the keyboard
		[self.pKeyboardView resignFirstResponder];
		//reset the current string held by keyboard
		[self.pKeyboardView reset];
		//remove all keyboard notifications
		[self.pKeyboardView unregisterKeyboardNotifications];
		//no need to stay in UI hierarchy anymore.
		[self.pKeyboardView removeFromSuperview];
		//need to update contraints of metal view here.
		[self.pKeyboardView layoutIfNeeded];
	}

	return true;
}

- (void)dealloc
{
	[super dealloc];
}

- (void)touchesBegan:(NSSet *)touches withEvent:(UIEvent *)event
{
	gainput::DeviceId deviceId = inputManager_->FindDeviceId(gainput::InputDevice::DT_TOUCH, 0);
	if (deviceId == gainput::InvalidDeviceId)
	{
		return;
	}
	gainput::InputDeviceTouch* device = static_cast<gainput::InputDeviceTouch*>(inputManager_->GetDevice(deviceId));
	if (!device)
	{
		return;
	}
	gainput::InputDeviceTouchImplIos* deviceImpl = static_cast<gainput::InputDeviceTouchImplIos*>(device->GetPimpl());
	if (!deviceImpl)
	{
		return;
	}

	for (UITouch *touch in touches)
	{
		CGPoint point = [touch locationInView:self];
       
        CGFloat force = 0.f;
        CGFloat maxForce = 1.f;
        if ([touch respondsToSelector:@selector(force)])
        {
            maxForce = fmax(0.01f,[touch maximumPossibleForce]);
            force = [touch force];
        }
        
		float x = point.x / self.bounds.size.width;
		float y = point.y / self.bounds.size.height;
        float z = force / maxForce;
        
		deviceImpl->HandleTouch(static_cast<void*>(touch), x, y, z);
	}
}


- (void)touchesMoved:(NSSet *)touches withEvent:(UIEvent *)event
{
	gainput::DeviceId deviceId = inputManager_->FindDeviceId(gainput::InputDevice::DT_TOUCH, 0);
	if (deviceId == gainput::InvalidDeviceId)
	{
		return;
	}
	gainput::InputDeviceTouch* device = static_cast<gainput::InputDeviceTouch*>(inputManager_->GetDevice(deviceId));
	if (!device)
	{
		return;
	}
	gainput::InputDeviceTouchImplIos* deviceImpl = static_cast<gainput::InputDeviceTouchImplIos*>(device->GetPimpl());
	if (!deviceImpl)
	{
		return;
	}

	for (UITouch *touch in touches)
	{
		CGPoint point = [touch locationInView:self];

        CGFloat force = 0.f;
        CGFloat maxForce = 1.f;
        if ([touch respondsToSelector:@selector(force)])
        {
            maxForce = [touch maximumPossibleForce];
            force = [touch force];
        }

		float x = point.x / self.bounds.size.width;
		float y = point.y / self.bounds.size.height;
        float z = force / maxForce;
        
		deviceImpl->HandleTouch(static_cast<void*>(touch), x, y, z);
	}
}


- (void)touchesEnded:(NSSet *)touches withEvent:(UIEvent *)event
{
	gainput::DeviceId deviceId = inputManager_->FindDeviceId(gainput::InputDevice::DT_TOUCH, 0);
	if (deviceId == gainput::InvalidDeviceId)
	{
		return;
	}
	gainput::InputDeviceTouch* device = static_cast<gainput::InputDeviceTouch*>(inputManager_->GetDevice(deviceId));
	if (!device)
	{
		return;
	}
	gainput::InputDeviceTouchImplIos* deviceImpl = static_cast<gainput::InputDeviceTouchImplIos*>(device->GetPimpl());
	if (!deviceImpl)
	{
		return;
	}

	for (UITouch *touch in touches)
	{
		CGPoint point = [touch locationInView:self];

        CGFloat force = 0.f;
        CGFloat maxForce = 1.f;
        if ([touch respondsToSelector:@selector(force)])
        {
            maxForce = [touch maximumPossibleForce];
            force = [touch force];
        }
        
		float x = point.x / self.bounds.size.width;
		float y = point.y / self.bounds.size.height;
        float z = force / maxForce;
        
		deviceImpl->HandleTouchEnd(static_cast<void*>(touch), x, y, z);
	}
}

- (void)touchesCancelled:(NSSet *)touches withEvent:(UIEvent *)event
{
	[self touchesEnded:touches withEvent:event];
}

@end

#endif
