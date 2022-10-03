
#ifndef GAINPUTINPUTDEVICEPADIOS_H_
#define GAINPUTINPUTDEVICEPADIOS_H_

#include "../../../include/gainput/GainputContainers.h"

namespace gainput
{
typedef void(*DeviceChangeCB)(const char*, bool, int controllerID);

class InputDevicePadImplIos : public InputDevicePadImpl
{
public:
	InputDevicePadImplIos(InputManager& manager, InputDevice& device, unsigned index, InputState& state, InputState& previousState);
	~InputDevicePadImplIos();
    
	InputDevice::DeviceVariant GetVariant() const override
	{
		return InputDevice::DV_STANDARD;
	}
    
    void Update(InputDeltaState* delta) override;
    
	InputDevice::DeviceState GetState() const override
	{
		return deviceState_;
	}
    
    virtual const char* GetDeviceName() override;

	bool IsValidButton(DeviceButtonId deviceButton) const override;

    typedef gainput::Array<void *> GlobalControllerList;
    static GlobalControllerList* mappedControllers_;

	virtual bool Vibrate(float leftMotor, float rightMotor) override {return false;}
	virtual bool SetRumbleEffect(float leftMotor, float rightMotor, uint32_t duration_ms, bool targetOwningDevice) override;
    virtual void SetOnDeviceChangeCallBack(void(*onDeviceChange)(const char*, bool added, int controllerID)) override;


private:
    InputManager& manager_;
    InputDevice& device_;
    unsigned index_;
    InputState& state_;
    InputState& previousState_;
	InputDevice::DeviceState deviceState_;
	InputDeltaState* deltaState_;
	void* deviceConnectedObserver;
	void* deviceDisconnectedObserver;
    DeviceChangeCB deviceChangeCb;

    bool isMicro_;
    bool isNormal_;
    bool isExtended_;
    bool supportsMotion_;
    bool isRemote_;
    void* gcController_;
#ifdef GAINPUT_IOS_HAPTICS
	void* hapticMotorLeft;
	void* hapticMotorRight;
#endif
    void UpdateRemote_(InputDeltaState* delta);
    void UpdateGamepad_();
	void SetupController();
};

}

#endif
