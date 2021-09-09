/************************************************************************************

Filename    :   Skeleton.h
Content     :   Skeleton for arm and hand model implementation
Created     :   2/20/2017
Authors     :   Jonathan E. Wright, Federico Schliemann

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

************************************************************************************/
#pragma once

#include <vector>

#include "OVR_Math.h"

namespace OVRFW {

class ovrJoint {
   public:
    ovrJoint() : Name(nullptr), Color(1.0f), ParentIndex(-1) {}

    ovrJoint(
        const char* name,
        const OVR::Vector4f& color,
        const OVR::Posef& pose,
        const int parentIndex)
        : Name(name), Color(color), Pose(pose), ParentIndex(parentIndex) {}

    char const* Name; // name of the joint
    OVR::Vector4f Color;
    OVR::Posef Pose; // pose of this joint
    int ParentIndex; // index of this joint's parent
};

class ovrSkeleton {
   public:
    ovrSkeleton();

    const ovrJoint& GetJoint(int const idx) const;
    int GetParentIndex(int const idx) const;
    const std::vector<ovrJoint>& GetJoints() const {
        return Joints;
    }
    const std::vector<OVR::Posef>& GetLocalSpacePoses() const {
        return LocalSpacePoses;
    }
    const std::vector<OVR::Posef>& GetWorldSpacePoses(bool updateFromLocal = true) const {
        if (updateFromLocal) {
            UpdateWorldFromLocal();
        }
        return WorldSpacePoses;
    }
    void SetJoints(const std::vector<ovrJoint>& newJoints);
    void TransformLocal(const OVR::Posef& t, int idx);
    void TransformWorld(const OVR::Posef& t, int idx);
    void UpdateLocalRotation(const OVR::Quatf& q, int idx);
    void UpdateLocalTranslation(const OVR::Vector3f& t, int idx);

   private:
    void UpdateWorldFromLocal() const;

    /// Essentially a BIND pose for the skeleton
    std::vector<ovrJoint> Joints;

    /// Current skeleton state for each joint
    std::vector<OVR::Posef> LocalSpacePoses;

    /// These two are semantically `const` but in practice lazily updated whenever
    /// UpdateWorldFromLocal is called on the const GetWorldSpacePoses method.
    /// The expectation is that calling any of the Transform methods will update LocalSpacePoses
    /// instantly and invalidate WorldSpaceDirty so that they can be lazily updated on demand.
    /// Declaring them mutable lets them change inside a `const` method deliberately.
    mutable std::vector<OVR::Posef> WorldSpacePoses;
    mutable bool WorldSpaceDirty;
};

} // namespace OVRFW
