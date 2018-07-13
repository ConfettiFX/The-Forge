
#include "../../include/gainput/gainput.h"

#if defined(GAINPUT_PLATFORM_IOS) || defined(GAINPUT_PLATFORM_TVOS)

#include "../../include/gainput/GainputIos.h"

#include "touch/GainputInputDeviceTouchIos.h"

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
	}

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
	return self;
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
            maxForce = [touch maximumPossibleForce];
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
