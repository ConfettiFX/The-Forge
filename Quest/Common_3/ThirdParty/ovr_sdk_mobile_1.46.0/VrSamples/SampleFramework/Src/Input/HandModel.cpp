/************************************************************************************

Filename    :   HandModel.cpp
Content     :   A hand model for the tracked remote
Created     :   8/20/2019
Authors     :   Federico Schliemann

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

************************************************************************************/

#include "HandModel.h"

using OVR::Matrix4f;
using OVR::Posef;
using OVR::Quatf;
using OVR::Vector3f;
using OVR::Vector4f;

namespace OVRFW {

static const char* ovrHandBoneNames[] = {
    "WristRoot", "ForearmStub", "Thumb0",   "Thumb1",    "Thumb2",  "Thumb3",
    "Index1",    "Index2",      "Index3",   "Middle1",   "Middle2", "Middle3",
    "Ring1",     "Ring2",       "Ring3",    "Pinky0",    "Pinky1",  "Pinky2",
    "Pinky3",    "ThumbTip",    "IndexTip", "MiddleTip", "RingTip", "PinkyTip"};
OVR_VERIFY_ARRAY_SIZE(ovrHandBoneNames, ovrHandBone_Max);

void ovrHandModel::Init(const ovrHandSkeleton& skeleton) {
    std::vector<ovrJoint> HandJoints;
    const Vector4f color{1.0, 1.0, 1.0f, 1.0f};

    for (uint32_t i = 0; i < skeleton.NumBones; ++i) {
        HandJoints.push_back(ovrJoint(
            ovrHandBoneNames[i], color, skeleton.BonePoses[i], (int)skeleton.BoneParentIndices[i]));
    }
    Skeleton.SetJoints(HandJoints);
    TransformedJoints = Skeleton.GetJoints();
}

void ovrHandModel::Update(const ovrHandPose& pose) {
    /// Move the skeleton to the hand joint
    for (int b = 0; b < ovrHandBone_Max; ++b) {
        const Quatf& rot = pose.BoneRotations[b];
        Skeleton.UpdateLocalRotation(rot, b);
    }

    /// Update the joints for beam rendering
    for (int i = 0; i < (int)TransformedJoints.size(); ++i) {
        TransformedJoints[i].Pose = Skeleton.GetWorldSpacePoses()[i];
    }
}

} // namespace OVRFW
