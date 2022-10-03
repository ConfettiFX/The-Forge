#include "../../../include/gainput/gainput.h"

#ifdef GAINPUT_PLATFORM_IOS

#include "GainputInputDeviceBuiltInImpl.h"
#include "../../../include/gainput/GainputInputDeltaState.h"
#include "../../../include/gainput/GainputHelpers.h"
#include "../../../include/gainput/GainputLog.h"

#include "GainputInputDeviceBuiltInIos.h"

#import <CoreMotion/CoreMotion.h>

namespace gainput
{

InputDeviceBuiltInImplIos::InputDeviceBuiltInImplIos(InputManager& manager, InputDevice& device, unsigned index, InputState& state, InputState& previousState)
	: pausePressed_(false),
	manager_(manager),
	device_(device),
	index_(index),
	padFound_(false),
	state_(state),
	previousState_(previousState),
	deviceState_(InputDevice::DS_UNAVAILABLE),
	motionManager_(0)
{
	CMMotionManager* motionManager = [[CMMotionManager alloc] init];
	[motionManager startDeviceMotionUpdates];
	motionManager_ = motionManager;
	deviceState_ = InputDevice::DS_OK;
}

InputDeviceBuiltInImplIos::~InputDeviceBuiltInImplIos()
{
	CMMotionManager* motionManager = reinterpret_cast<CMMotionManager*>(motionManager_);
	[motionManager stopDeviceMotionUpdates];
	[motionManager release];
}

void InputDeviceBuiltInImplIos::Update(InputDeltaState* delta)
{
	GAINPUT_ASSERT(motionManager_);
    @autoreleasepool {
	CMMotionManager* motionManager = reinterpret_cast<CMMotionManager*>(motionManager_);
	CMDeviceMotion* motion = motionManager.deviceMotion;
	if (!motion)
	{
		return;
	}

	HandleAxis(device_, state_, delta, BuiltInButtonAccelerationX, motion.userAcceleration.x);
	HandleAxis(device_, state_, delta, BuiltInButtonAccelerationY, motion.userAcceleration.y);
	HandleAxis(device_, state_, delta, BuiltInButtonAccelerationZ, motion.userAcceleration.z);

	HandleAxis(device_, state_, delta, BuiltInButtonGravityX, motion.gravity.x);
	HandleAxis(device_, state_, delta, BuiltInButtonGravityY, motion.gravity.y);
	HandleAxis(device_, state_, delta, BuiltInButtonGravityZ, motion.gravity.z);

	const float gyroX = 2.0f * (motion.attitude.quaternion.x * motion.attitude.quaternion.z + motion.attitude.quaternion.w * motion.attitude.quaternion.y);
	const float gyroY = 2.0f * (motion.attitude.quaternion.y * motion.attitude.quaternion.z - motion.attitude.quaternion.w * motion.attitude.quaternion.x);
	const float gyroZ = 1.0f - 2.0f * (motion.attitude.quaternion.x * motion.attitude.quaternion.x + motion.attitude.quaternion.y * motion.attitude.quaternion.y);
	HandleAxis(device_, state_, delta, BuiltInButtonGyroscopeX, gyroX);
	HandleAxis(device_, state_, delta, BuiltInButtonGyroscopeY, gyroY);
	HandleAxis(device_, state_, delta, BuiltInButtonGyroscopeZ, gyroZ);

	HandleAxis(device_, state_, delta, BuiltInButtonMagneticFieldX, motion.magneticField.field.x);
	HandleAxis(device_, state_, delta, BuiltInButtonMagneticFieldY, motion.magneticField.field.y);
	HandleAxis(device_, state_, delta, BuiltInButtonMagneticFieldZ, motion.magneticField.field.z);

    }
}

bool InputDeviceBuiltInImplIos::IsValidButton(DeviceButtonId deviceButton) const
{
	return deviceButton >= BuiltInButtonAccelerationX && deviceButton <= BuiltInButtonMagneticFieldZ;
}

}

#endif

