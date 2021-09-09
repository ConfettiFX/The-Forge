
#include "../../include/gainput/gainput.h"

#if defined(GAINPUT_PLATFORM_IOS) || defined(GAINPUT_PLATFORM_TVOS)

#include "../../include/gainput/GainputIos.h"
#include "../../../../../../OS/Interfaces/IMemory.h"

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
	const char* utfString = [text UTF8String];
	if(utfString)
		deviceImpl->HandleKey((wchar_t)utfString[0]);
}
@end

gainput::GesturePhase GetGestureState(UIGestureRecognizerState state)
{
	switch (state) {
		case UIGestureRecognizerStateBegan:
			return gainput::GesturePhase::GesturePhaseStarted;
		case UIGestureRecognizerStateChanged:
			return gainput::GesturePhase::GesturePhaseUpdated;
		case UIGestureRecognizerStateEnded:
		case UIGestureRecognizerStateCancelled:
			return gainput::GesturePhase::GesturePhaseEnded;
		case UIGestureRecognizerStateFailed:
		default:
			return gainput::GesturePhase::GesturePhaseCanceled;
	}
}
gainput::GesturePhase GetGesturePhase(UITouchPhase phase)
{
	switch (phase) {
		case UITouchPhaseBegan:
			return gainput::GesturePhase::GesturePhaseStarted;
		case UITouchPhaseMoved:
		case UITouchPhaseStationary:
			return gainput::GesturePhase::GesturePhaseUpdated;
		case UITouchPhaseEnded:
			return gainput::GesturePhase::GesturePhaseEnded;
		case UITouchPhaseCancelled:
			return gainput::GesturePhase::GesturePhaseCanceled;
		default:
			return gainput::GesturePhase::GesturePhaseCanceled;
	}
}

@interface GainputGestureRecognizerDelegate : NSObject<UIGestureRecognizerDelegate> {}
@property(nonnull) gainput::InputDeviceTouchImplIos* inputManager;
@property unsigned gestureId;
@property gainput::GestureConfig gestureConfig;
@end

@implementation GainputGestureRecognizerDelegate

-(void) handlePinch:(UIPinchGestureRecognizer *)gesture
{
	float velocity = 0.0f;
	if (!isnan(gesture.velocity))
		velocity = gesture.velocity;
	float contentScale = [gesture.view.layer contentsScale];
	CGPoint touch0 = [gesture locationOfTouch:0 inView:[gesture.view superview]];
	touch0.x *= contentScale;
	touch0.y *= contentScale;
	CGPoint touch1 = CGPointMake(0.0f,0.0f);
	if([gesture numberOfTouches] > 1)
	{
		touch1 = [gesture locationOfTouch:1 inView:[gesture.view superview]];
		touch1.x *= contentScale;
		touch1.y *= contentScale;
	}
	CGPoint location = [gesture locationInView:[gesture.view superview]];
	location.x *= contentScale;
	location.y *= contentScale;
	
	gainput::GestureChange gestureData = {};
	gestureData.phase = GetGestureState(gesture.state);
	gestureData.type = gainput::GesturePinch;
	gestureData.position[0] = location.x;
	gestureData.position[1] = location.y;
	gestureData.scale = gesture.scale;
	gestureData.velocity = velocity;
	gestureData.distance[0] = (touch1.x - touch0.x);
	gestureData.distance[1] = (touch1.y - touch0.y);
	self.inputManager->HandleGesture(self.gestureId, gestureData);
	
	gesture.scale = 1.0f;
}

-(void) handleLongPress:(UILongPressGestureRecognizer *)gesture
{
	CGPoint location = [gesture locationInView:[gesture.view superview]];
	float contentScale = [gesture.view.layer contentsScale];
	location.x *= contentScale;
	location.y *= contentScale;
	
	gainput::GestureChange gestureData = {};
	gestureData.phase = GetGestureState(gesture.state);
	gestureData.type = gainput::GestureLongPress;
	gestureData.position[0] = location.x;
	gestureData.position[1] = location.y;
	self.inputManager->HandleGesture(self.gestureId, gestureData);
}

- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer shouldRecognizeSimultaneouslyWithGestureRecognizer:(UIGestureRecognizer *)otherGestureRecognizer
{
	return YES;
}
@end

@interface GainputGestureRecognizerImpl : NSObject {}
@property(nonnull) gainput::InputDeviceTouchImplIos* inputManager;
@property(nonnull, retain) GainputView* gainputView;

- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event;
- (void)touchesMoved:(NSSet *)touches withEvent:(UIEvent *)event;
- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event;
- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event;
@end

@implementation GainputGestureRecognizerImpl

- (void)touchesBegan:(NSSet *)touches withEvent:(UIEvent *)event
{
	gainput::InputDeviceTouchImplIos* deviceImpl = self.inputManager;
	if (!deviceImpl)
	{
		return;
	}
	
	for (UITouch *touch in touches)
	{
		CGPoint point = [touch locationInView:self.gainputView.superview];
		float contentScale = [self.gainputView.superview.layer contentsScale];
		point.x *= contentScale;
		point.y *= contentScale;
		
		CGFloat force = 0.f;
		CGFloat maxForce = 1.f;
		if ([touch respondsToSelector:@selector(force)])
		{
			maxForce = fmax(0.01f,[touch maximumPossibleForce]);
			force = [touch force];
		}
		
		float x = point.x;
		float y = point.y;
		float z = force / maxForce;
		
		deviceImpl->HandleTouch(static_cast<void*>(touch), x, y, z);
	}
}

- (void)touchesMoved:(NSSet *)touches withEvent:(UIEvent *)event
{
	gainput::InputDeviceTouchImplIos* deviceImpl = self.inputManager;
	if (!deviceImpl)
	{
		return;
	}
	
	for (UITouch *touch in touches)
	{
		CGPoint point = [touch locationInView:self.gainputView.superview];
		float contentScale = [self.gainputView.superview.layer contentsScale];
		point.x *= contentScale;
		point.y *= contentScale;
		
		CGFloat force = 0.f;
		CGFloat maxForce = 1.f;
		if ([touch respondsToSelector:@selector(force)])
		{
			maxForce = [touch maximumPossibleForce];
			force = [touch force];
		}
		
		float x = point.x;
		float y = point.y;
		float z = force / maxForce;
		
		deviceImpl->HandleTouch(static_cast<void*>(touch), x, y, z);
	}
}

- (void)touchesEnded:(NSSet *)touches withEvent:(UIEvent *)event
{
	gainput::InputDeviceTouchImplIos* deviceImpl = self.inputManager;
	if (!deviceImpl)
	{
		return;
	}
	
	for (UITouch *touch in touches)
	{
		CGPoint point = [touch locationInView:self.gainputView.superview];
		float contentScale = [self.gainputView.superview.layer contentsScale];
		point.x *= contentScale;
		point.y *= contentScale;
		
		CGFloat force = 0.f;
		CGFloat maxForce = 1.f;
		if ([touch respondsToSelector:@selector(force)])
		{
			maxForce = [touch maximumPossibleForce];
			force = [touch force];
		}
		
		float x = point.x;
		float y = point.y;
		float z = force / maxForce;
		
		deviceImpl->HandleTouchEnd(static_cast<void*>(touch), x, y, z);
	}
}

- (void)touchesCancelled:(NSSet *)touches withEvent:(UIEvent *)event
{
	[self touchesEnded:touches withEvent:event];
}
@end

@interface GainputPanGestureRecognizer : UIPanGestureRecognizer {}
@property(nonnull, retain) GainputGestureRecognizerImpl* inputManager;
- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event;
- (void)touchesMoved:(NSSet *)touches withEvent:(UIEvent *)event;
- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event;
- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event;
@end

@implementation GainputPanGestureRecognizer


-(BOOL) cancelsTouchesInView
{
	return false;
}
-(BOOL) delaysTouchesEnded
{
	return false;
}


- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer shouldRecognizeSimultaneouslyWithGestureRecognizer:(UIGestureRecognizer *)otherGestureRecognizer
{
	return YES;
}

- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer shouldBeRequiredToFailByGestureRecognizer:(UIGestureRecognizer *)otherGestureRecognizer
{
	return NO;
}

- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer shouldRequireFailureOfGestureRecognizer:(UIGestureRecognizer *)otherGestureRecognizer
{
	return NO;
}

- (void)touchesBegan:(NSSet *)touches withEvent:(UIEvent *)event
{
	[self.inputManager touchesBegan:touches withEvent:event];
}

- (void)touchesMoved:(NSSet *)touches withEvent:(UIEvent *)event
{
	[self.inputManager touchesMoved:touches withEvent:event];
}

- (void)touchesEnded:(NSSet *)touches withEvent:(UIEvent *)event
{
	[self.inputManager touchesEnded:touches withEvent:event];
}

- (void)touchesCancelled:(NSSet *)touches withEvent:(UIEvent *)event
{
	[self.inputManager touchesCancelled:touches withEvent:event];
}

@end

@interface GainputPinchGestureRecognizer : UIPinchGestureRecognizer {}
@property(nonnull, retain) GainputGestureRecognizerImpl* inputManager;

- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event;
- (void)touchesMoved:(NSSet *)touches withEvent:(UIEvent *)event;
- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event;
- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event;
@end

@implementation GainputPinchGestureRecognizer
- (void)touchesBegan:(NSSet *)touches withEvent:(UIEvent *)event
{
	[super touchesBegan:touches withEvent:event];
	[self.inputManager touchesBegan:touches withEvent:event];
}

- (void)touchesMoved:(NSSet *)touches withEvent:(UIEvent *)event
{
	[super touchesMoved:touches withEvent:event];
	[self.inputManager touchesMoved:touches withEvent:event];
}

- (void)touchesEnded:(NSSet *)touches withEvent:(UIEvent *)event
{
	[super touchesEnded:touches withEvent:event];
	[self.inputManager touchesEnded:touches withEvent:event];
}

- (void)touchesCancelled:(NSSet *)touches withEvent:(UIEvent *)event
{
	[super touchesCancelled:touches withEvent:event];
	[self.inputManager touchesCancelled:touches withEvent:event];
}

- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer shouldRecognizeSimultaneouslyWithGestureRecognizer:(UIGestureRecognizer *)otherGestureRecognizer
{
	return YES;
}
@end

struct ActiveFingers
{
	CGPoint downLocations[gainput::TouchPointCount];
	void* downTouches[gainput::TouchPointCount];
};

@interface GainputTapGestureRecognizer : UITapGestureRecognizer {}
@property(nonnull, retain) GainputGestureRecognizerImpl* inputManager;
@property unsigned gestureID;
@property(nonnull) ActiveFingers * activeGestures;
- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event;
- (void)touchesMoved:(NSSet *)touches withEvent:(UIEvent *)event;
- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event;
- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event;
@end

@implementation GainputTapGestureRecognizer

-(BOOL) cancelsTouchesInView
{
	return false;
}
-(BOOL) delaysTouchesEnded
{
	return false;
}


- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer shouldRecognizeSimultaneouslyWithGestureRecognizer:(UIGestureRecognizer *)otherGestureRecognizer
{
	return YES;
}

- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer shouldBeRequiredToFailByGestureRecognizer:(UIGestureRecognizer *)otherGestureRecognizer
{
	return NO;
}

- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer shouldRequireFailureOfGestureRecognizer:(UIGestureRecognizer *)otherGestureRecognizer
{
	return NO;
}

-(void) dealloc
{
	if(_activeGestures)
		tf_free(_activeGestures);
	
	[super dealloc];
}

-(int) AddAndGetTapIndex:(UITouch *) touch Location:(CGPoint) loc
{
	for(int i = 0 ; i < gainput::TouchPointCount; i++)
	{
		if(_activeGestures->downTouches[i] == NULL)
		{
			_activeGestures->downTouches[i] = static_cast<void*>(touch);
			_activeGestures->downLocations[i] = loc;
			return i;
		}
	}
	
	return -1;
}
-(int) GetTapIndex:(UITouch *) touch
{
	for(unsigned i = 0 ; i < gainput::TouchPointCount; i++)
	{
		if(_activeGestures->downTouches[i] == static_cast<void*>(touch))
		{
			return i;
		}
	}
	
	return -1;
}

-(void) RemoveTap:(int) tapIndex
{
	_activeGestures->downTouches[tapIndex] = NULL;
}

-(void) handleCustomTap:(NSSet *)touches
{
	gainput::InputDeviceTouchImplIos* deviceImpl = [self.inputManager inputManager];
	UIView * view = [[self.inputManager gainputView] superview];
	
	for (UITouch *touch in touches)
	{
		UITouchPhase currentPhase = [touch phase];
		if(currentPhase == UITouchPhaseMoved)
			continue;
		
		CGPoint point = [touch locationInView:view];
		float contentScale = [view.layer contentsScale];
		point.x *= contentScale;
		point.y *= contentScale;

		int tapIndex = -1;
		if(currentPhase == UITouchPhaseBegan)
		{
			tapIndex = [self AddAndGetTapIndex:(touch) Location:point];
		}
		else
			tapIndex = [self GetTapIndex:touch];
		
		if(tapIndex != -1)
		{
			gainput::GestureChange gestureData = {};
			gestureData.phase = GetGesturePhase(currentPhase);
			gestureData.fingerIndex = tapIndex;
			gestureData.type = gainput::GestureTap;
			gestureData.position[0] = point.x;
			gestureData.position[1] = point.y;
			
			if(gestureData.phase == gainput::GesturePhaseEnded || gestureData.phase== gainput::GesturePhaseCanceled)
			{
				CGPoint prevPoint = _activeGestures->downLocations[tapIndex];
				
				if(prevPoint.x != point.x || prevPoint.y != point.y)
				{
					gestureData.phase = gainput::GesturePhaseCanceled;
				}
				
				gestureData.position[0] = prevPoint.x;
				gestureData.position[1] = prevPoint.y;
				[self RemoveTap:tapIndex];
			}
			
			deviceImpl->HandleGesture([self gestureID], gestureData);
		}
	}
}

- (void)touchesBegan:(NSSet *)touches withEvent:(UIEvent *)event
{
	[self handleCustomTap:touches];
}

- (void)touchesMoved:(NSSet *)touches withEvent:(UIEvent *)event
{
}

- (void)touchesEnded:(NSSet *)touches withEvent:(UIEvent *)event
{
	[self handleCustomTap:touches];
}

- (void)touchesCancelled:(NSSet *)touches withEvent:(UIEvent *)event
{
	[self handleCustomTap:touches];
}

@end

@interface GainputCustomPanGestureRecognizer : UIPanGestureRecognizer {}
@property(nonnull, retain) GainputGestureRecognizerImpl* inputManager;
@property unsigned gestureID;
@property(nonnull) ActiveFingers * activeGestures;
- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event;
- (void)touchesMoved:(NSSet *)touches withEvent:(UIEvent *)event;
- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event;
- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event;
@end

@implementation GainputCustomPanGestureRecognizer

-(BOOL) cancelsTouchesInView
{
	return false;
}
-(BOOL) delaysTouchesEnded
{
	return false;
}

- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer shouldRecognizeSimultaneouslyWithGestureRecognizer:(UIGestureRecognizer *)otherGestureRecognizer
{
	return YES;
}

- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer shouldBeRequiredToFailByGestureRecognizer:(UIGestureRecognizer *)otherGestureRecognizer
{
	return NO;
}

- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer shouldRequireFailureOfGestureRecognizer:(UIGestureRecognizer *)otherGestureRecognizer
{
	return NO;
}

-(void) dealloc
{
	if(_activeGestures)
		tf_free(_activeGestures);
	
	[super dealloc];
}

-(int) AddAndGetPanIndex:(UITouch *) touch Location:(CGPoint) loc
{
	for(int i = 0 ; i < gainput::TouchPointCount; i++)
	{
		if(_activeGestures->downTouches[i] == NULL)
		{
			_activeGestures->downTouches[i] = static_cast<void*>(touch);
			_activeGestures->downLocations[i] = loc;
			return i;
		}
	}
	
	return -1;
}
-(int) GetPanIndex:(UITouch *) touch
{
	for(unsigned i = 0 ; i < gainput::TouchPointCount; i++)
	{
		if(_activeGestures->downTouches[i] == static_cast<void*>(touch))
		{
			return i;
		}
	}
	
	return -1;
}

-(void) RemovePan:(int) panIndex
{
	_activeGestures->downTouches[panIndex] = NULL;
}

-(void) handleCustomPan:(NSSet *)touches
{
	gainput::InputDeviceTouchImplIos* deviceImpl = [self.inputManager inputManager];
	UIView * view = [[self.inputManager gainputView] superview];
	
	for (UITouch *touch in touches)
	{
		UITouchPhase currentPhase = [touch phase];
		
		CGPoint point = [touch locationInView:view];
		float contentScale = [view.layer contentsScale];
		point.x *= contentScale;
		point.y *= contentScale;

		int panIndex = -1;
		if(currentPhase == UITouchPhaseBegan)
		{
			panIndex = [self AddAndGetPanIndex:(touch) Location:point];
		}
		else
			panIndex = [self GetPanIndex:touch];
		
		if(panIndex != -1)
		{
			gainput::GestureChange gestureData = {};
			gestureData.phase = GetGesturePhase(currentPhase);
			gestureData.fingerIndex = panIndex;
			gestureData.type = gainput::GesturePan;

			gestureData.position[0] = point.x;
			gestureData.position[1] = point.y;
			
			gestureData.translation[0] = point.x - _activeGestures->downLocations[panIndex].x;
			gestureData.translation[1] = point.y - _activeGestures->downLocations[panIndex].y;

			if(gestureData.phase == gainput::GesturePhaseEnded || gestureData.phase == gainput::GesturePhaseCanceled)
			{
				[self RemovePan:panIndex];
			}

			deviceImpl->HandleGesture([self gestureID], gestureData);
		}
	}
}

- (void)touchesBegan:(NSSet *)touches withEvent:(UIEvent *)event
{
	[self handleCustomPan:touches];
}

- (void)touchesMoved:(NSSet *)touches withEvent:(UIEvent *)event
{
	[self handleCustomPan:touches];
}

- (void)touchesEnded:(NSSet *)touches withEvent:(UIEvent *)event
{
	[self handleCustomPan:touches];
}

- (void)touchesCancelled:(NSSet *)touches withEvent:(UIEvent *)event
{
	[self handleCustomPan:touches];
}

@end

@interface GainputLongPressGestureRecognizer : UILongPressGestureRecognizer {}
@property(nonnull, retain) GainputGestureRecognizerImpl* inputManager;

- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event;
- (void)touchesMoved:(NSSet *)touches withEvent:(UIEvent *)event;
- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event;
- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event;
@end

@implementation GainputLongPressGestureRecognizer

- (void)touchesBegan:(NSSet *)touches withEvent:(UIEvent *)event
{
	[super touchesBegan:touches withEvent:event];
	[self.inputManager touchesBegan:touches withEvent:event];
}

- (void)touchesMoved:(NSSet *)touches withEvent:(UIEvent *)event
{
	[super touchesMoved:touches withEvent:event];
	[self.inputManager touchesMoved:touches withEvent:event];
}

- (void)touchesEnded:(NSSet *)touches withEvent:(UIEvent *)event
{
	[super touchesEnded:touches withEvent:event];
	[self.inputManager touchesEnded:touches withEvent:event];
}

- (void)touchesCancelled:(NSSet *)touches withEvent:(UIEvent *)event
{
	[super touchesCancelled:touches withEvent:event];
	[self.inputManager touchesCancelled:touches withEvent:event];
}

@end

@implementation GainputView
{
	gainput::InputManager* inputManager_;
	GainputGestureRecognizerImpl* gestureRecognizerImpl;
	NSMutableArray<GainputGestureRecognizerDelegate*>* gestureRecognizerDelegates;
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
			
			gainput::InputDeviceTouchImplIos* deviceImpl = static_cast<gainput::InputDeviceTouchImplIos*>(device->GetPimpl());
			
			gestureRecognizerImpl = [GainputGestureRecognizerImpl alloc];
			gestureRecognizerImpl.inputManager = deviceImpl;
			gestureRecognizerImpl.gainputView = self;
			
			gestureRecognizerDelegates = [[NSMutableArray<GainputGestureRecognizerDelegate*> alloc] init];
			
			GainputPanGestureRecognizer* uiPan = [[GainputPanGestureRecognizer alloc] initWithTarget:self action:NULL];
			uiPan.minimumNumberOfTouches = 1;
			uiPan.maximumNumberOfTouches = 8;
			uiPan.inputManager = gestureRecognizerImpl;
			
			[self addGestureRecognizer:uiPan];
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
	for (GainputGestureRecognizerDelegate* delegate in gestureRecognizerDelegates)
	{
		[delegate dealloc];
	}
	[super dealloc];
}

- (void)addGestureMapping:
(unsigned)gestureId
withConfig:(gainput::GestureConfig&)gestureConfig
{
	const gainput::GestureType gestureType = gestureConfig.mType;
	gainput::DeviceId deviceId = inputManager_->FindDeviceId(gainput::InputDevice::DT_TOUCH, 0);
	gainput::InputDeviceTouch* device = static_cast<gainput::InputDeviceTouch*>(inputManager_->GetDevice(deviceId));
	gainput::InputDeviceTouchImplIos* deviceImpl = static_cast<gainput::InputDeviceTouchImplIos*>(device->GetPimpl());
	
	GainputGestureRecognizerDelegate* gestureRecognizerDelegate = [GainputGestureRecognizerDelegate alloc];
	gestureRecognizerDelegate.gestureId = gestureId;
	gestureRecognizerDelegate.gestureConfig = gestureConfig;
	gestureRecognizerDelegate.inputManager = deviceImpl;
	[gestureRecognizerDelegates addObject:gestureRecognizerDelegate];
	
	if (gestureType == gainput::GestureTap)
	{
		GainputTapGestureRecognizer* uiTap = [[GainputTapGestureRecognizer alloc] initWithTarget:gestureRecognizerDelegate action:NULL];
		uiTap.delegate = gestureRecognizerDelegate;
		uiTap.inputManager = gestureRecognizerImpl;
		uiTap.gestureID = gestureId;
		uiTap.activeGestures = (ActiveFingers*)tf_calloc(1, sizeof(ActiveFingers));
		if (gestureConfig.mNumberOfTapsRequired)
			uiTap.numberOfTapsRequired = gestureConfig.mNumberOfTapsRequired;
		[self addGestureRecognizer:uiTap];
	}
	if (gestureType == gainput::GesturePan)
	{
		GainputCustomPanGestureRecognizer* uiPan = [[GainputCustomPanGestureRecognizer alloc] initWithTarget:gestureRecognizerDelegate action:NULL];
		if (gestureConfig.mMinNumberOfTouches)
			uiPan.minimumNumberOfTouches = gestureConfig.mMinNumberOfTouches;
		if (gestureConfig.mMaxNumberOfTouches)
			uiPan.maximumNumberOfTouches = gestureConfig.mMaxNumberOfTouches;
		uiPan.delegate = gestureRecognizerDelegate;
		uiPan.inputManager = gestureRecognizerImpl;
		uiPan.gestureID = gestureId;
		uiPan.activeGestures = (ActiveFingers*)tf_calloc(1, sizeof(ActiveFingers));
		[self addGestureRecognizer:uiPan];
	}
	if (gestureType == gainput::GesturePinch)
	{
		GainputPinchGestureRecognizer* uiPinch = [[GainputPinchGestureRecognizer alloc] initWithTarget:gestureRecognizerDelegate action:@selector(handlePinch:)];
		uiPinch.delegate = gestureRecognizerDelegate;
		uiPinch.inputManager = gestureRecognizerImpl;
		[self addGestureRecognizer:uiPinch];
	}
	if (gestureType == gainput::GestureLongPress)
	{
		GainputLongPressGestureRecognizer* uiLongPress = [[GainputLongPressGestureRecognizer alloc] initWithTarget:gestureRecognizerDelegate action:@selector(handleLongPress:)];
		uiLongPress.delegate = gestureRecognizerDelegate;
		uiLongPress.minimumPressDuration = gestureConfig.mMinimumPressDuration;
		uiLongPress.inputManager = gestureRecognizerImpl;
		[self addGestureRecognizer:uiLongPress];
	}
}

@end

#endif
