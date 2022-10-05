
#include <gainput/gainput.h>

#if defined(GAINPUT_PLATFORM_MAC)
#include <iostream>

#import <Cocoa/Cocoa.h>

// Define your user buttons
enum Button
{
	ButtonMenu,
	ButtonConfirm,
	MouseX,
	MouseY
};


const char* windowName = "Gainput basic sample";
const int width = 800;
const int height = 600;

gainput::InputManager* manager;
gainput::InputMap* map;
gainput::DeviceId mouseId;
gainput::DeviceId keyboardId;
gainput::DeviceId padId;

@interface Updater : NSObject
@end

@implementation Updater
- (void)update:(NSTimer *)theTimer
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

int main(int argc, char** argv)
{
    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    id window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, width, height)
        styleMask:NSTitledWindowMask|NSClosableWindowMask backing:NSBackingStoreBuffered defer:NO];
    [window cascadeTopLeftFromPoint:NSMakePoint(20,20)];
    [window setTitle: @"Gainput Basic Sample"];
    [window makeKeyAndOrderFront:nil];

	manager = new gainput::InputManager;
	manager->SetDisplaySize(width, height);
	mouseId = manager->CreateDevice<gainput::InputDeviceMouse>();
	keyboardId = manager->CreateDevice<gainput::InputDeviceKeyboard>();
	padId = manager->CreateDevice<gainput::InputDevicePad>();

    map = new gainput::InputMap(*manager);
	map->MapBool(ButtonMenu, keyboardId, gainput::KeyEscape);
	map->MapBool(ButtonConfirm, mouseId, gainput::MouseButtonLeft);
	map->MapFloat(MouseX, mouseId, gainput::MouseAxisX);
	map->MapFloat(MouseY, mouseId, gainput::MouseAxisY);
	map->MapBool(ButtonConfirm, padId, gainput::PadButtonA);

    Updater* updater = [[Updater alloc] init];

    [NSTimer scheduledTimerWithTimeInterval:0.016
        target:updater
        selector:@selector(update:)
        userInfo:nil
        repeats:YES];

//    [NSApp activateIgnoringOtherApps:YES];
    [NSApp run];

	return 0;
}
#endif

