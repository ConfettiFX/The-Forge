/*
 * Copyright (c) 2017-2024 The Forge Interactive Inc.
 *
 * This file is part of The-Forge
 * (see https://github.com/ConfettiFX/The-Forge).
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
*/
#ifndef GAINPUTINPUTDEVICEPADGCKIT_H_
#define GAINPUTINPUTDEVICEPADGCKIT_H_

#include "../../../include/gainput/GainputContainers.h"

namespace gainput
{
typedef void(*DeviceChangeCB)(const char*, bool, int controllerID);

class InputDevicePadImplGCKit : public InputDevicePadImpl
{
public:
	InputDevicePadImplGCKit(InputManager& manager, InputDevice& device, unsigned index, InputState& state, InputState& previousState);
	~InputDevicePadImplGCKit();
    
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
	const char* controllerName_;
#ifdef GAINPUT_GC_HAPTICS
	void* hapticMotorLeft;
	void* hapticMotorRight;
#endif
    void UpdateGamepad_();
	void SetupController();
};

}

#endif
