
#include <gainput/gainput.h>

#if defined(GAINPUT_PLATFORM_IOS)
#include <iostream>

#import <UIKit/UIKit.h>

#include <gainput/GainputIos.h>


// Define your user buttons
enum Button
{
	ButtonMenu,
	ButtonConfirm,
	MouseX,
	MouseY
};

gainput::InputManager* manager;
gainput::InputMap* map;
gainput::DeviceId mouseId;
gainput::DeviceId keyboardId;
gainput::DeviceId padId;
gainput::DeviceId touchId;

@interface AppDelegate : UIResponder <UIApplicationDelegate>
@property (strong, nonatomic) UIWindow *window;
@end

@implementation AppDelegate
CADisplayLink* displayLink;
- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
  	manager = new gainput::InputManager;
	mouseId = manager->CreateDevice<gainput::InputDeviceMouse>();
	keyboardId = manager->CreateDevice<gainput::InputDeviceKeyboard>();
	padId = manager->CreateDevice<gainput::InputDevicePad>();
    touchId = manager->CreateDevice<gainput::InputDeviceTouch>();

    map = new gainput::InputMap(*manager);
	map->MapBool(ButtonMenu, keyboardId, gainput::KeyEscape);
	map->MapBool(ButtonConfirm, mouseId, gainput::MouseButtonLeft);
	map->MapBool(ButtonConfirm, padId, gainput::PadButtonA);
    map->MapBool(ButtonConfirm, touchId, gainput::Touch0Down);

	map->MapFloat(MouseX, mouseId, gainput::MouseAxisX);
	map->MapFloat(MouseY, mouseId, gainput::MouseAxisY);
	map->MapFloat(MouseX, touchId, gainput::Touch0X);
	map->MapFloat(MouseY, touchId, gainput::Touch0Y);


    CGRect bounds = [[UIScreen mainScreen] bounds];
    self.window = [[[UIWindow alloc] init] autorelease];
    self.window.rootViewController =  [[[UIViewController alloc] init] autorelease];
    self.window.backgroundColor = [UIColor blueColor];
    [self.window makeKeyAndVisible];
    [application setIdleTimerDisabled:TRUE];

    UIView* gainputView = [[[GainputView alloc] initWithFrame:bounds inputManager:*manager] autorelease];
    self.window.rootViewController.view = gainputView;

    displayLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(update)];
    NSAssert(displayLink, @"Failed to create display link.");
    [displayLink setFrameInterval: 1];
    [displayLink addToRunLoop:[NSRunLoop currentRunLoop] forMode:NSDefaultRunLoopMode];
	return YES;
}

- (void)update
{
    manager->Update();

    // Check button states
    if (map->GetBoolWasDown(ButtonConfirm))
    {
        gainput::InputDevicePad* pad = static_cast<gainput::InputDevicePad*>(manager->GetDevice(padId));
        pad->Vibrate(1.0f, 0.0f);
    }
    if (map->GetBoolWasDown(ButtonMenu))
    {
        gainput::InputDevicePad* pad = static_cast<gainput::InputDevicePad*>(manager->GetDevice(padId));
        pad->Vibrate(0.0f, 0.0f);
    }

    if (map->GetBoolWasDown(ButtonMenu))
    {
        std::cout << "Open Menu!!" << std::endl;
    }
    if (map->GetBoolWasDown(ButtonConfirm))
    {
        std::cout << "Confirmed!!" << std::endl;
    }
    if (map->GetBool(ButtonConfirm))
    {
        std::cout << "LM down" << std::endl;
    }

    if (map->GetFloatDelta(MouseX) != 0.0f || map->GetFloatDelta(MouseY) != 0.0f)
    {
        std::cout << "Mouse:" << map->GetFloat(MouseX) << ", " << map->GetFloat(MouseY) << std::endl;
    }
}
@end

int main(int argc, char * argv[])
{
  @autoreleasepool
  {
      return UIApplicationMain(argc, argv, nil, NSStringFromClass([AppDelegate class]));
  }
}
#endif

