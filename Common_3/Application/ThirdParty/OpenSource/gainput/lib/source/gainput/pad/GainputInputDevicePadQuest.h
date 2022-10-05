/*
 * Copyright (c) 2017-2022 The Forge Interactive Inc.
*/


#ifndef GAINPUTINPUTDEVICEPADQUEST_H_
#define GAINPUTINPUTDEVICEPADQUEST_H_

#include "../include/gainput/gainput.h"
#include "../include/gainput/GainputHelpers.h"
#include "../include/gainput/GainputInputManager.h"

#define GAINPUT_PLATFORM_QUEST
#if defined(GAINPUT_PLATFORM_QUEST)

#define QUEST_REMOTE_MAX 2

#include "../../../../../../../../../Common_3/OS/Quest/VrApi.h"
#include "../../../../../../../../OS/ThirdParty/OpenSource/ovr_sdk_mobile/VrApi/Include/VrApi.h"
#include "../../../../../../../../OS/ThirdParty/OpenSource/ovr_sdk_mobile/VrApi/Include/VrApi_Input.h"

#include "../../../../../../../../../Common_3/Utilities/Interfaces/ILog.h"

extern QuestVR* pQuest; 

namespace gainput {

    struct ovrRemote
    {
        ovrInputTrackedRemoteCapabilities capabilities;
        ovrHandedness handedness;
    };

    class InputDevicePadImplQuest : public InputDevicePadImpl
    {
    public:
        InputDevicePadImplQuest(InputManager& manager, InputDevice& device, unsigned index, InputState& state, InputState& previousState) :
            device_(device),
            state_(state),
            ovr_(NULL)
        {
			ovr_ = pQuest->pOvr;
            for (int i = 0; i < QUEST_REMOTE_MAX; ++i)
                inputDevices_[i].capabilities.Header.DeviceID = ovrDeviceIdType_Invalid;
            EnumerateInputDevices();
        }

        ~InputDevicePadImplQuest()
        {
        }

        InputDevice::DeviceVariant GetVariant() const { return InputDevice::DV_STANDARD; }

        void Update(InputDeltaState* delta)
        {
			ovr_ = pQuest->pOvr; 
            EnumerateInputDevices();
            
            for (int i = 0; i < QUEST_REMOTE_MAX; ++i)
            {
                if (inputDevices_[i].capabilities.Header.DeviceID != ovrDeviceIdType_Invalid)
                {
                    ovrInputStateTrackedRemote remoteInputState;
                    remoteInputState.Header.ControllerType = inputDevices_[i].capabilities.Header.Type;

                    if (vrapi_GetCurrentInputState(ovr_, inputDevices_[i].capabilities.Header.DeviceID, &remoteInputState.Header) != ovrSuccess)
                    {
                        LOGF(eERROR, "Failed to get input state from device ID(%u)", inputDevices_[i].capabilities.Header.DeviceID);
                        continue;
                    }

                    ApplyStateToDelta(&remoteInputState, inputDevices_[i].handedness, delta);
                }
            }
        }

        InputDevice::DeviceState GetState() const { return InputDevice::DeviceState::DS_OK; }

        bool IsValidButton(DeviceButtonId deviceButton) const
        {
            return deviceButton < PadButtonAxis6 || (deviceButton >= PadButtonStart && deviceButton <= PadButtonHome);
        }

        bool Vibrate(float leftMotor, float rightMotor) { return false; }

    private:
        InputDevice&             device_;
        InputState&              state_;
        ovrMobile*               ovr_;

        ovrRemote inputDevices_[QUEST_REMOTE_MAX];

        void EnumerateInputDevices()
        {
            if (!ovr_)
                return;

            for (int i = 0;; ++i)
            {
                ovrInputCapabilityHeader capHeader = {};
                if (vrapi_EnumerateInputDevices(ovr_, i, &capHeader) != ovrSuccess)
                    break;

                if (!IsDeviceTracked(capHeader.DeviceID))
                {
                    OnDeviceConnected(capHeader);
                }
            }
        }

        bool IsDeviceTracked(ovrDeviceID deviceID)
        {
            for (int i = 0; i < QUEST_REMOTE_MAX; ++i)
            {
                if (inputDevices_[i].capabilities.Header.DeviceID == deviceID)
                    return true;
            }

            return false;
        }

        void OnDeviceConnected(ovrInputCapabilityHeader capHeader)
        {
            if (capHeader.Type == ovrControllerType_TrackedRemote)
            {
                LOGF(eINFO, "Controller connected ID(%u)", capHeader.DeviceID);

                ovrRemote remote = {};
                remote.capabilities.Header = capHeader;
                if (vrapi_GetInputDeviceCapabilities(ovr_, &remote.capabilities.Header) == ovrSuccess)
                {
                    if (remote.capabilities.ControllerCapabilities & ovrControllerCaps_LeftHand)
                        remote.handedness = ovrHandedness::VRAPI_HAND_LEFT;
                    else if (remote.capabilities.ControllerCapabilities & ovrControllerCaps_RightHand)
                        remote.handedness = ovrHandedness::VRAPI_HAND_RIGHT;
                    else
                        return;

                    for (int i = 0; i < QUEST_REMOTE_MAX; ++i)
                    {
                        if (inputDevices_[i].capabilities.Header.DeviceID == ovrDeviceIdType_Invalid)
                        {
                            inputDevices_[i] = remote;
                            return;
                        }
                    }
                }

                LOGF(eERROR, "Failed to connect controller ID(%u)", capHeader.DeviceID);
            }
        }

        static float NormalizeThumbstickValue(const float thumbstickValue, const float deadZone)
        {
            if (thumbstickValue > +deadZone)
            {
                return (thumbstickValue - deadZone) / (1.0f - deadZone);
            }

            if (thumbstickValue < -deadZone)
            {
                return (thumbstickValue + deadZone) / (1.0f - deadZone);
            }

            return 0.0f;
        }

        void ApplyStateToDelta(const ovrInputStateTrackedRemote* pState, ovrHandedness handedness, InputDeltaState* pDelta)
        {
            if (handedness == VRAPI_HAND_LEFT)
            {

                static const uint32_t               numButtons = 5;
                static const ovrButton questButtons[numButtons] = {
                    ovrButton_X,
                    ovrButton_Y,
                    ovrButton_LThumb,
                    ovrButton_LShoulder,
                    ovrButton_Back
                };

                static const gainput::DeviceButtonId gaintputButtons[numButtons] = {
                    PadButtonX,
                    PadButtonY,
                    PadButtonL3,
                    PadButtonL2,
                    PadButtonSelect
                };

                // buttons
                for (uint32_t i = 0; i < numButtons; i++)
                {
                    HandleButton(device_, state_, pDelta, gaintputButtons[i], pState->Buttons & questButtons[i]);
                }

                // left trigger
                HandleAxis(device_, state_, pDelta, PadButtonAxis4, pState->IndexTrigger);

                //left joystick
                HandleAxis(device_, state_, pDelta, PadButtonLeftStickX, NormalizeThumbstickValue(pState->Joystick.x, 0.25f));
                HandleAxis(device_, state_, pDelta, PadButtonLeftStickY, NormalizeThumbstickValue(pState->Joystick.y, 0.25f));
            }
            else if (handedness == VRAPI_HAND_RIGHT)
            {
                static const uint32_t               numButtons = 5;
                static const ovrButton questButtons[numButtons] = {
                    ovrButton_A,
                    ovrButton_B,
                    ovrButton_RThumb,
                    ovrButton_RShoulder,
                    ovrButton_Enter
                };

                static const gainput::DeviceButtonId gaintputButtons[numButtons] = {
                    PadButtonA,
                    PadButtonB,
                    PadButtonR3,
                    PadButtonR2,
                    PadButtonStart
                };

                // buttons
                for (uint32_t i = 0; i < numButtons; i++)
                {
                    HandleButton(device_, state_, pDelta, gaintputButtons[i], pState->Buttons & questButtons[i]);
                }

                // right trigger
                HandleAxis(device_, state_, pDelta, PadButtonAxis5, pState->IndexTrigger);

                // right joystick
                HandleAxis(device_, state_, pDelta, PadButtonRightStickX, NormalizeThumbstickValue(pState->Joystick.x, 0.25f));
                HandleAxis(device_, state_, pDelta, PadButtonRightStickY, NormalizeThumbstickValue(pState->Joystick.y, 0.25f));
            }
        }
    };

}    // namespace gainput
#endif
#endif