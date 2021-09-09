/************************************************************************************

Filename    :   HandRenderer.h
Content     :   A one stop for rendering hands
Created     :   April 2020
Authors     :   Federico Schliemann

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

************************************************************************************/

#pragma once

#include <vector>
#include <string>
#include <memory>

/// Sample Framework
#include "Misc/Log.h"
#include "Model/SceneView.h"
#include "Render/GlProgram.h"
#include "Render/SurfaceRender.h"
#include "Input/HandModel.h"
#include "Input/Skeleton.h"

#include "OVR_Math.h"

#include "VrApi_Types.h"
#include "VrApi_Input.h"

namespace OVRFW {

class HandRenderer {
   public:
    HandRenderer() = default;
    ~HandRenderer() = default;

    bool Init(ovrMobile* ovr, bool leftHand);
    void Shutdown();
    void Update(const ovrHandPose& pose);
    void Render(std::vector<ovrDrawSurface>& surfaceList);

    bool IsLeftHand() const {
        return isLeftHand;
    }
    const ovrHandModel& Model() const {
        return HandModel;
    }
    const std::vector<OVR::Matrix4f>& Transforms() const {
        return TransformMatrices;
    }

   public:
    OVR::Vector3f SpecularLightDirection;
    OVR::Vector3f SpecularLightColor;
    OVR::Vector3f AmbientLightColor;
    OVR::Vector3f GlowColor;
    float Confidence;

   private:
    bool isLeftHand;
    ovrHandModel HandModel;
    GlProgram ProgHand;
    ovrSurfaceDef HandSurfaceDef;
    ovrDrawSurface HandSurface;
    std::vector<OVR::Matrix4f> TransformMatrices;
    std::vector<OVR::Matrix4f> BindMatrices;
    std::vector<OVR::Matrix4f> SkinMatrices;
    GlBuffer SkinUniformBuffer;
};

} // namespace OVRFW
