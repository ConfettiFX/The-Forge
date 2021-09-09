/************************************************************************************

Filename    :   HandMaskRenderer.h
Content     :   A one stop for rendering hand masks
Created     :   03/24/2020
Authors     :   Federico Schliemann

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

************************************************************************************/
#pragma once

#include <vector>
#include <string>
#include <memory>
#include <unordered_map>
#include <functional>

#include "Model/SceneView.h"
#include "Render/GlProgram.h"
#include "Render/SurfaceRender.h"

#include "OVR_Math.h"

#include "VrApi_Types.h"
#include "VrApi_Input.h"

namespace OVRFW {

class HandMaskRenderer {
   public:
    HandMaskRenderer() = default;
    ~HandMaskRenderer() = default;

    void Init(bool leftHand);
    void Shutdown();
    void Update(
        const OVR::Posef& headPose,
        const OVR::Posef& handPose,
        const std::vector<OVR::Matrix4f>& jointTransforms,
        const double displayTimeInSeconds,
        const float handSize = 1.0f);
    void Render(std::vector<ovrDrawSurface>& surfaceList);

   public:
    float LayerBlend;
    float Falloff;
    float Intensity;
    float FadeIntensity;
    bool UseBorderFade;
    float BorderFadeSize;
    float AlphaMaskSize;
    bool RenderInverseSubtract;
    ovrSurfaceDef HandMaskSurfaceDef;

   private:
    GlProgram ProgHandMaskAlphaGradient;
    GlProgram ProgHandMaskBorderFade;
    ovrDrawSurface HandMaskSurface;
    std::vector<OVR::Matrix4f> HandMaskMatrices;
    GlBuffer HandMaskUniformBuffer;
    bool isLeftHand;
};

} // namespace OVRFW
