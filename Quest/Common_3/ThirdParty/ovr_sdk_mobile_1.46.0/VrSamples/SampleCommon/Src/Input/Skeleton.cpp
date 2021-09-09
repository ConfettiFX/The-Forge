/************************************************************************************

Filename    :   Skeleton.cpp
Content     :   skeleton for arm model implementation
Created     :   2/20/2017
Authors     :   Jonathan E. Wright

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

************************************************************************************/

#include "Skeleton.h"
#include <cassert>

using OVR::Posef;

namespace OVRFW {

ovrSkeleton::ovrSkeleton() : WorldSpaceDirty(true) {}

const ovrJoint& ovrSkeleton::GetJoint(int const idx) const {
    assert(idx >= 0 && idx < static_cast<int>(Joints.size()));
    return Joints[idx];
}

int ovrSkeleton::GetParentIndex(int const idx) const {
    if (idx < 0 || idx >= static_cast<int>(Joints.size())) {
        return -1;
    }
    return Joints[idx].ParentIndex;
}

void ovrSkeleton::SetJoints(const std::vector<ovrJoint>& newJoints) {
    Joints = newJoints;
    LocalSpacePoses.resize(Joints.size());
    WorldSpacePoses.resize(Joints.size());

    /// Set local
    for (int i = 0; i < (int)Joints.size(); ++i) {
        LocalSpacePoses[i] = Joints[i].Pose;
    }

    /// Set World
    WorldSpaceDirty = true;
    UpdateWorldFromLocal();
}

void ovrSkeleton::UpdateWorldFromLocal() const {
    if (false == WorldSpaceDirty)
        return;

    assert(Joints.size() == LocalSpacePoses.size());
    assert(Joints.size() == WorldSpacePoses.size());
    for (int i = 0; i < (int)Joints.size(); ++i) {
        WorldSpacePoses[i] = (Joints[i].ParentIndex < 0)
            ? LocalSpacePoses[i]
            : (WorldSpacePoses[Joints[i].ParentIndex] * LocalSpacePoses[i]);
    }
    WorldSpaceDirty = false;
}

void ovrSkeleton::TransformLocal(const OVR::Posef& t, int idx) {
    assert(idx >= 0 && idx < static_cast<int>(Joints.size()));
    if (idx >= 0 && idx < static_cast<int>(Joints.size())) {
        LocalSpacePoses[idx] = Joints[idx].Pose * t;
        WorldSpaceDirty = true;
    }
}

void ovrSkeleton::UpdateLocalRotation(const OVR::Quatf& q, int idx) {
    assert(idx >= 0 && idx < static_cast<int>(Joints.size()));
    if (idx >= 0 && idx < static_cast<int>(Joints.size())) {
        LocalSpacePoses[idx].Rotation = q;
        WorldSpaceDirty = true;
    }
}

void ovrSkeleton::UpdateLocalTranslation(const OVR::Vector3f& t, int idx) {
    assert(idx >= 0 && idx < static_cast<int>(Joints.size()));
    LocalSpacePoses[idx].Translation = t;
    WorldSpaceDirty = true;
}

void ovrSkeleton::TransformWorld(const OVR::Posef& t, int idx) {
    assert(idx >= 0 && idx < static_cast<int>(Joints.size()));
    if (idx >= 0 && idx < static_cast<int>(Joints.size())) {
        WorldSpacePoses[idx] = t;
        const int paretnIndex = Joints[idx].ParentIndex;
        if (paretnIndex < 0) {
            // the root remains the same
            LocalSpacePoses[idx] = t;
        } else {
            const Posef& parentPose = WorldSpacePoses[paretnIndex];
            LocalSpacePoses[idx] = parentPose.Inverted() * WorldSpacePoses[idx];
        }
        WorldSpaceDirty = true;
    }
}

} // namespace OVRFW
