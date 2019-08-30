
#ifndef GAINPUTIOS_H_
#define GAINPUTIOS_H_

#import <UIKit/UIKit.h>

namespace gainput
{
    class InputManager;
	struct InputGestureConfig;
}


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
@end

#endif

