
#ifndef GAINPUTIOS_H_
#define GAINPUTIOS_H_

#import <UIKit/UIKit.h>

namespace gainput
{
    class InputManager;
	struct InputGestureConfig;
}

#ifdef GAINPUT_IOS_HAPTICS
#import <CoreHaptics/CoreHaptics.h>
#import <GameController/GameController.h>
// Haptic motor interface
// Applies to both gamepad motors (if initWithController)
// Or applies to actual iphone device (if initWithIOSDevice)
// Device Vibration doesn't work with ipads
// controller vibration only available from ios 14.0+
// phone vibration only available from ios 13.0+
API_AVAILABLE(ios(13.0))
@interface IOSHapticMotor : NSObject
-(void)cleanup;
-(bool)setIntensity:(float)intensity sharpness:(float)sharpness duration:(float)duration;
-(id _Nullable) initWithController:(GCController* _Nonnull)controller locality:(GCHapticsLocality _Nonnull )locality API_AVAILABLE(ios(14.0));
-(id _Nullable) initWithIOSDevice;
@end
#endif

/// [IOS ONLY] UIKit view that captures keyboard inputs.
/**
 * In order to enable keyboard input on iOS devices (i.e. make the InputDeviceTouch work),
 * an instance of this view has to be attached as a subview to GainputView below.
 * TODO:  CUstomization, Modifier keys
 * TODO: Behave as regular keyboard with full events
 */
@interface KeyboardView : UITextView<UIKeyInput>
- (id _Nonnull) initWithFrame:(CGRect)frame inputManager:(gainput::InputManager&)inputManager;
@property(nonatomic, readonly) BOOL hasText;
@end


/// [IOS ONLY] UIKit view that captures touch inputs.
/**
 * In order to enable touch input on iOS devices (i.e. make the InputDeviceTouch work),
 * an instance of this view has to be attached as a subview to the view of your application.
 * Note that, for touches to work in portrait and landscape mode, the subview has to be
 * rotated correctly with its parent.
 */
@interface GainputView : UIView
@property (nullable, nonatomic, strong) KeyboardView * pKeyboardView;
- (id _Nonnull)initWithFrame:(CGRect)frame inputManager:(gainput::InputManager&)inputManager;
- (BOOL)setVirtualKeyboard:(int)inputType;
-(void) addGestureMapping:
(unsigned)gestureId
withConfig:(gainput::GestureConfig&)gestureConfig;
+ (bool)setPhoneHaptics:(float)intensity sharpness:(float)sharpness seconds:(float)duration API_AVAILABLE(ios(13.0));
@end

#endif

