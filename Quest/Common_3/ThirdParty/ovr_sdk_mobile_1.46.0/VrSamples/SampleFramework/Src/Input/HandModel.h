/************************************************************************************

Filename    :   HandModel.h
Content     :   A hand model for the tracked remote
Created     :   8/20/2019
Authors     :   Federico Schliemann

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

************************************************************************************/
#pragma once

#include <vector>

#include "OVR_Types.h"
#include "OVR_LogUtils.h"

#include "VrApi_Types.h"
#include "VrApi_Input.h"

#include "Input/Skeleton.h"

namespace OVRFW {

class ovrHandModel {
   public:
    ovrHandModel() = default;
    ~ovrHandModel() = default;

    void Init(const ovrHandSkeleton& skeleton);
    void Update(const ovrHandPose& pose);

    const ovrSkeleton& GetSkeleton() const {
        return Skeleton;
    }
    const std::vector<ovrJoint>& GetTransformedJoints() const {
        return TransformedJoints;
    }

   private:
    ovrSkeleton Skeleton;
    std::vector<ovrJoint> TransformedJoints;
};

} // namespace OVRFW
