/************************************************************************************

Filename    :   ArmModel.h
Content     :   An arm model for the tracked remote
Created     :   2/20/2017
Authors     :   Jonathan E. Wright

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

************************************************************************************/
#pragma once

#include <vector>

#include "OVR_Types.h"
#include "OVR_LogUtils.h"

#include "Skeleton.h"

namespace OVRFW {

class ovrArmModel {
   public:
    enum ovrHandedness { HAND_UNKNOWN = -1, HAND_LEFT, HAND_RIGHT, HAND_MAX };

    ovrArmModel();

    void InitSkeleton(bool isLeft);

    void Update(
        const OVR::Posef& headPose,
        const OVR::Posef& remotePose,
        const ovrHandedness handedness,
        const bool recenteredController,
        OVR::Posef& outPose);

    const ovrSkeleton& GetSkeleton() const {
        return Skeleton;
    }
    ovrSkeleton& GetSkeleton() {
        return Skeleton;
    }
    const std::vector<ovrJoint>& GetTransformedJoints() const {
        return TransformedJoints;
    }

   private:
    ovrSkeleton Skeleton;
    OVR::Posef FootPose;
    std::vector<ovrJoint> TransformedJoints;

    float LastUnclampedRoll;

    float TorsoYaw; // current yaw of the torso
    bool TorsoTracksHead; // true to make the torso track the head
    bool ForceRecenter; // true to force the torso to the head yaw

    int ClavicleJointIdx;
    int ShoulderJointIdx;
    int ElbowJointIdx;
    int WristJointIdx;
};

} // namespace OVRFW
