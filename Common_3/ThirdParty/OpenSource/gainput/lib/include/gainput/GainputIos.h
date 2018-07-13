
#ifndef GAINPUTIOS_H_
#define GAINPUTIOS_H_

#import <UIKit/UIKit.h>

namespace gainput
{
    class InputManager;
}

/// [IOS ONLY] UIKit view that captures touch inputs.
/**
 * In order to enable touch input on iOS devices (i.e. make the InputDeviceTouch work),
 * an instance of this view has to be attached as a subview to the view of your application.
 * Note that, for touches to work in portrait and landscape mode, the subview has to be
 * rotated correctly with its parent.
 */
@interface GainputView : UIView

- (id)initWithFrame:(CGRect)frame inputManager:(gainput::InputManager&)inputManager;

@end

#endif

