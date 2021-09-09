/************************************************************************************

Filename    :   SimpleInput.h
Content     :   Helper around VRAPI input calls
Created     :   July 2020
Authors     :   Federico Schliemann
Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

************************************************************************************/
#pragma once

#include "VrApi.h"
#include "VrApi_Input.h"
#include "OVR_Math.h"

#include <vector>

namespace OVRFW {

class SimpleInput {
   public:
    struct Frame {
        /// timing
        double timeStamp_;
        /// head
        ovrTracking2 head_;
        /// controllers
        ovrTracking controllerPoseL_;
        ovrTracking controllerPoseR_;
        ovrInputStateTrackedRemote remoteInputStateL_;
        ovrInputStateTrackedRemote remoteInputStateR_;
        /// hands
        ovrHandPose handPoseL_;
        ovrHandPose handPoseR_;
        ovrInputStateHand handInputStateL_;
        ovrInputStateHand handInputStateR_;
        /// ids
        ovrDeviceID controllerL_;
        ovrDeviceID controllerR_;
        ovrDeviceID handL_;
        ovrDeviceID handR_;
    };

   public:
    SimpleInput()
        : isRecording_(false),
          isPlaying_(false),
          isPaused_(false),
          currentFrame_(-1),
          recordStartTime_(0.0),
          playStartTime_(0.0) {
        Reset();
    }
    ~SimpleInput() = default;

    static const ovrDeviceID kInvalidDeviceID = 0;

    /// API
    void Reset();
    void Update(ovrMobile* ovr, double displayTimeInSeconds);

    /// Tracking State
    bool IsLeftHandTracked() const {
        return frame_.handL_ != kInvalidDeviceID;
    }
    bool IsRightHandTracked() const {
        return frame_.handR_ != kInvalidDeviceID;
    }
    bool IsLeftControllerTracked() const {
        return frame_.controllerL_ != kInvalidDeviceID;
    }
    bool IsRightControllerTracked() const {
        return frame_.controllerR_ != kInvalidDeviceID;
    }

    /// Hand Pinch state
    bool IsLeftHandPinching() const {
        return (frame_.handInputStateL_.InputStateStatus & ovrInputStateHandStatus_IndexPinching) !=
            0;
    }
    bool IsRightHandPinching() const {
        return (frame_.handInputStateR_.InputStateStatus & ovrInputStateHandStatus_IndexPinching) !=
            0;
    }
    bool WasLeftHandPinching() const {
        return (handInputStatePrevL_.InputStateStatus & ovrInputStateHandStatus_IndexPinching) != 0;
    }
    bool WasRightHandPinching() const {
        return (handInputStatePrevR_.InputStateStatus & ovrInputStateHandStatus_IndexPinching) != 0;
    }

    // Controller Button State
    bool IsLeftHandTriggerDown() const {
        return IsLeftControllerTracked() && frame_.remoteInputStateL_.IndexTrigger > 0.1f;
    }
    bool IsLeftHandTriggerPressed() const {
        return IsLeftHandTriggerDown() && prevRemoteInputStateL_.IndexTrigger <= 0.1f;
    }
    bool IsRightHandTriggerDown() const {
        return IsRightControllerTracked() && frame_.remoteInputStateR_.IndexTrigger > 0.1f;
    }
    bool IsRightHandTriggerPressed() const {
        return IsRightHandTriggerDown() && prevRemoteInputStateR_.IndexTrigger <= 0.1f;
    }

    /// Raw tracking
    const ovrTracking2& HeadTracking() const {
        return frame_.head_;
    }
    const ovrInputStateTrackedRemote& LeftControllerInputState() const {
        return frame_.remoteInputStateL_;
    }
    const ovrInputStateTrackedRemote& RightControllerInputState() const {
        return frame_.remoteInputStateR_;
    }
    /// Raw hand input
    const ovrInputStateHand& LeftHandInputState() const {
        return frame_.handInputStateL_;
    }
    const ovrInputStateHand& RightHandInputState() const {
        return frame_.handInputStateR_;
    }

    /// frame delta for pinch detection
    const ovrInputStateHand& PreviousLeftHandInputState() const {
        return handInputStatePrevL_;
    }
    const ovrInputStateHand& PreviousRightHandInputState() const {
        return handInputStatePrevR_;
    }

    /// Raw hand pose, including fingers
    const ovrHandPose& LeftHandPose() const {
        return frame_.handPoseL_;
    }
    const ovrHandPose& RightHandPose() const {
        return frame_.handPoseR_;
    }

    /// Head and controller
    OVR::Posef HeadPose() const {
        // Note: this does a cast conversion
        return frame_.head_.HeadPose.Pose;
    }
    OVR::Posef LeftControllerPose() const {
        // Note: this does a cast conversion
        return frame_.controllerPoseL_.HeadPose.Pose;
    }
    OVR::Posef RightControllerPose() const {
        // Note: this does a cast conversion
        return frame_.controllerPoseR_.HeadPose.Pose;
    }

    /// Recording and Playback
    bool IsRecording() const {
        return isRecording_;
    }
    bool IsPlaying() const {
        return isPlaying_;
    }
    bool IsPaused() const {
        return isPaused_;
    }
    bool IsAtEnd() const {
        return currentFrame_ == static_cast<int>(recordedFrames_.size() - 1);
    }
    int CurrentFrame() const {
        return currentFrame_;
    }
    size_t FrameCount() const {
        return recordedFrames_.size();
    }
    void Record() {
        isPlaying_ = false;
        recordedFrames_.clear();
        currentFrame_ = 0;
        isRecording_ = true;
        recordStartTime_ = frame_.timeStamp_;
    }
    void Play() {
        isRecording_ = false;
        currentFrame_ = isPlaying_ ? currentFrame_ : 0;
        playStartTime_ = isPlaying_ ? playStartTime_ : frame_.timeStamp_;
        isPlaying_ = true;
        isPaused_ = false;
    }
    void Stop() {
        isPaused_ = false;
        currentFrame_ = 0;
        isRecording_ = false;
        isPlaying_ = false;
    }
    void Pause() {
        isPaused_ = true;
    }
    void Step(int delta) {
        currentFrame_ += delta;
        int maxFrames = static_cast<int>(recordedFrames_.size() - 1);
        currentFrame_ = (currentFrame_ < maxFrames) ? currentFrame_ : maxFrames;
        currentFrame_ = (currentFrame_ >= 0) ? currentFrame_ : 0;
    }
    const std::vector<Frame>& GetRecording() const {
        return recordedFrames_;
    }
    void SetRecording(const std::vector<Frame>& recording) {
        recordedFrames_ = recording;
    }

   protected:
    void UpdateFromSensors(ovrMobile* ovr, double displayTimeInSeconds);
    void UpdateFromRecording(ovrMobile* ovr, double displayTimeInSeconds);

   private:
    /// current frame
    Frame frame_;
    /// helpers
    ovrInputStateHand handInputStatePrevL_;
    ovrInputStateHand handInputStatePrevR_;

    ovrInputStateTrackedRemote prevRemoteInputStateL_;
    ovrInputStateTrackedRemote prevRemoteInputStateR_;

    /// serialization
    bool isRecording_;
    bool isPlaying_;
    bool isPaused_;
    int currentFrame_;
    double recordStartTime_;
    double playStartTime_;
    std::vector<Frame> recordedFrames_;
};

} // namespace OVRFW
