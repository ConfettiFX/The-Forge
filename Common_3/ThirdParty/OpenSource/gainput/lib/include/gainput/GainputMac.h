//
//  GainputMac.h
//  gainputstatic
//
//  Created by Toto on 9/7/18.
//

#ifndef GainputMac_h
#define GainputMac_h

#import <Cocoa/Cocoa.h>

/// [MACOS ONLY] NSView that captures mouse/keyboard inputs.
@interface GainputMacInputView : NSView

- (id)initWithFrame:(NSRect)frame window:(NSWindow *)window retinaScale:(float)retinaScale inputManager:(gainput::InputManager&)inputManager;

-(void)SetMouseCapture:(BOOL)captured;

@end


namespace gainput
{
	class InputManager;
}



#endif /* GainputMac_h */
