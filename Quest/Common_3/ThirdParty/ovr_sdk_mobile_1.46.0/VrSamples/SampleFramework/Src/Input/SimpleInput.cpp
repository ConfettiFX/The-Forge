/************************************************************************************

Filename    :   SimpleInput.cpp
Content     :   Helper around VRAPI input calls
Created     :   October 2020
Authors     :   Federico Schliemann
Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

************************************************************************************/
#include "SimpleInput.h"

namespace OVRFW {

void SimpleInput::Reset() {
    frame_.controllerL_ = kInvalidDeviceID;
    frame_.controllerR_ = kInvalidDeviceID;
    frame_.handL_ = kInvalidDeviceID;
    frame_.handR_ = kInvalidDeviceID;
}

void SimpleInput::Update(ovrMobile* ovr, double displayTimeInSeconds) {
    if (isPlaying_) {
        UpdateFromRecording(ovr, displayTimeInSeconds);
    } else {
        UpdateFromSensors(ovr, displayTimeInSeconds);
        if (isRecording_) {
            recordedFrames_.push_back(frame_);
        }
    }
}

void SimpleInput::UpdateFromRecording(ovrMobile* ovr, double displayTimeInSeconds) {
    if (recordedFrames_.size() > 0) {
        frame_ = recordedFrames_[currentFrame_];
        frame_.timeStamp_ = displayTimeInSeconds;
        if (!isPaused_) {
            currentFrame_++;
            int maxFrames = static_cast<int>(recordedFrames_.size() - 1);
            if (currentFrame_ > maxFrames) {
                currentFrame_ = maxFrames;
                Pause();
            }
        }
    }
}

void SimpleInput::UpdateFromSensors(ovrMobile* ovr, double displayTimeInSeconds) {
    Reset();
    frame_.timeStamp_ = displayTimeInSeconds;

    /// Enumerate
    for (uint32_t deviceIndex = 0;; deviceIndex++) {
        ovrInputCapabilityHeader capsHeader;
        if (vrapi_EnumerateInputDevices(ovr, deviceIndex, &capsHeader) < 0) {
            break; // no more devices
        }

        if (capsHeader.Type == ovrControllerType_TrackedRemote) {
            ovrInputTrackedRemoteCapabilities remoteCaps;
            remoteCaps.Header = capsHeader;
            ovrResult result = vrapi_GetInputDeviceCapabilities(ovr, &remoteCaps.Header);
            if (result == ovrSuccess) {
                if ((remoteCaps.ControllerCapabilities & ovrControllerCaps_LeftHand) != 0) {
                    frame_.controllerL_ = capsHeader.DeviceID;
                } else {
                    frame_.controllerR_ = capsHeader.DeviceID;
                }
            }
        }
        if (capsHeader.Type == ovrControllerType_Hand) {
            ovrInputHandCapabilities handCaps;
            handCaps.Header = capsHeader;
            ovrResult result = vrapi_GetInputDeviceCapabilities(ovr, &handCaps.Header);
            if (result == ovrSuccess) {
                if ((handCaps.HandCapabilities & ovrHandCaps_LeftHand) != 0) {
                    frame_.handL_ = capsHeader.DeviceID;
                } else {
                    frame_.handR_ = capsHeader.DeviceID;
                }
            }
        }
    }
    /// update states
    if (IsLeftHandTracked()) {
        handInputStatePrevL_ = frame_.handInputStateL_;
        frame_.handInputStateL_.Header.ControllerType = ovrControllerType_Hand;
        (void)vrapi_GetCurrentInputState(ovr, frame_.handL_, &(frame_.handInputStateL_.Header));
        frame_.handPoseL_.Header.Version = ovrHandVersion_1;
        (void)vrapi_GetHandPose(
            ovr, frame_.handL_, displayTimeInSeconds, &(frame_.handPoseL_.Header));
    } else {
        handInputStatePrevL_.InputStateStatus = 0u;
    }
    if (IsRightHandTracked()) {
        handInputStatePrevR_ = frame_.handInputStateR_;
        frame_.handInputStateR_.Header.ControllerType = ovrControllerType_Hand;
        (void)vrapi_GetCurrentInputState(ovr, frame_.handR_, &(frame_.handInputStateR_.Header));
        frame_.handPoseR_.Header.Version = ovrHandVersion_1;
        (void)vrapi_GetHandPose(
            ovr, frame_.handR_, displayTimeInSeconds, &(frame_.handPoseR_.Header));
    } else {
        handInputStatePrevR_.InputStateStatus = 0u;
    }

    /// Head Pose
    frame_.head_ = vrapi_GetPredictedTracking2(ovr, displayTimeInSeconds);

    /// Controllers
    if (IsLeftControllerTracked()) {
        prevRemoteInputStateL_ = frame_.remoteInputStateL_;
        frame_.remoteInputStateL_.Header.ControllerType = ovrControllerType_TrackedRemote;
        (void)vrapi_GetCurrentInputState(
            ovr, frame_.controllerL_, &frame_.remoteInputStateL_.Header);
        (void)vrapi_GetInputTrackingState(
            ovr, frame_.controllerL_, displayTimeInSeconds, &frame_.controllerPoseL_);
    } else {
        prevRemoteInputStateL_ = {};
    }
    if (IsRightControllerTracked()) {
        prevRemoteInputStateR_ = frame_.remoteInputStateR_;
        frame_.remoteInputStateR_.Header.ControllerType = ovrControllerType_TrackedRemote;
        (void)vrapi_GetCurrentInputState(
            ovr, frame_.controllerR_, &frame_.remoteInputStateR_.Header);
        (void)vrapi_GetInputTrackingState(
            ovr, frame_.controllerR_, displayTimeInSeconds, &frame_.controllerPoseR_);
    } else {
        prevRemoteInputStateR_ = {};
    }
}

} // namespace OVRFW
