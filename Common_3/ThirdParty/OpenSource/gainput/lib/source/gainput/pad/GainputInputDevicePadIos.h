
#ifndef GAINPUTINPUTDEVICEPADIOS_H_
#define GAINPUTINPUTDEVICEPADIOS_H_

#include "../../../include/gainput/GainputContainers.h"

namespace gainput
{

class InputDevicePadImplIos : public InputDevicePadImpl
{
public:
	InputDevicePadImplIos(InputManager& manager, InputDevice& device, unsigned index, InputState& state, InputState& previousState);
	~InputDevicePadImplIos();
    
	InputDevice::DeviceVariant GetVariant() const
	{
		return InputDevice::DV_STANDARD;
	}
    
    void Update(InputDeltaState* delta);
    
	InputDevice::DeviceState GetState() const
	{
		return deviceState_;
	}

	bool IsValidButton(DeviceButtonId deviceButton) const;

    typedef gainput::Array<void *> GlobalControllerList;
    static GlobalControllerList* mappedControllers_;

	bool Vibrate(float leftMotor, float rightMotor)
	{
		return false;
	}

private:
    InputManager& manager_;
    InputDevice& device_;
    unsigned index_;
    InputState& state_;
    InputState& previousState_;
    InputDevice::DeviceState deviceState_;

    bool pausePressed_;
    bool isMicro_;
    bool isNormal_;
    bool isExtended_;
    bool supportsMotion_;
    bool isRemote_;
    void* gcController_;

    void UpdateRemote_(InputDeltaState* delta);
    void UpdateGamepad_(InputDeltaState* delta);
};

}

#endif
