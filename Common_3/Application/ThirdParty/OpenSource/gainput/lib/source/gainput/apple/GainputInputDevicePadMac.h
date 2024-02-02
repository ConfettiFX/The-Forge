
#ifndef GAINPUTINPUTDEVICEPADMAC_H_
#define GAINPUTINPUTDEVICEPADMAC_H_


namespace gainput
{
typedef void(*DeviceChangeCB)(const char*, bool, int controllerID);

class InputDevicePadImplMac : public InputDevicePadImpl
{
public:
	InputDevicePadImplMac(InputManager& manager, InputDevice& device, unsigned index, InputState& state, InputState& previousState);
	~InputDevicePadImplMac();

    virtual InputDevice::DeviceVariant GetVariant() const override
	{
		return InputDevice::DV_STANDARD;
	}

    virtual void Update(InputDeltaState* delta) override;

    virtual InputDevice::DeviceState GetState() const override
	{
		return deviceState_;
	}

    virtual bool IsValidButton(DeviceButtonId deviceButton) const override;

    virtual bool Vibrate(float leftMotor, float rightMotor) override
	{
		return false;
	}

	virtual bool SetRumbleEffect(float leftMotor, float rightMotor, uint32_t duration_ms, bool targetOwningDevice) override;

	virtual const char* GetDeviceName() override;

	virtual void SetOnDeviceChangeCallBack(void(*onDeviceChange)(const char*, bool added, int controllerID)) override;

	HashMap<unsigned, DeviceButtonId> buttonDialect_;
	HashMap<unsigned, DeviceButtonId> axisDialect_;
	bool  alternativeDPadScheme;
	float minAxis_;
	float maxAxis_;
	float minTriggerAxis_;
	float maxTriggerAxis_;
	InputManager& manager_;
	InputDevice& device_;
	unsigned index_;
	InputState& state_;
	InputState& previousState_;
	InputState nextState_;
	InputDeltaState* delta_;
	InputDevice::DeviceState deviceState_;
	DeviceChangeCB deviceChangeCb;
	
	struct RumbleData
	{
		float fLeftMotor;
		float fRightMotor;
		uint32_t durationMs;
		void* pIOHIDRef_;
		InputDevicePadImplMac* pPadRef;
	};

	static const int MAX_NAME_SIZE = 128;
	char deviceName[MAX_NAME_SIZE];
	char serialNumber[MAX_NAME_SIZE];
	void* ioManager_;
	void* pIOHIDRef_;
private:
};

}

#endif

