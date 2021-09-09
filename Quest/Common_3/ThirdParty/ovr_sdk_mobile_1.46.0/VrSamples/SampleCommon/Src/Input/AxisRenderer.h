/************************************************************************************

Filename    :   AxisRenderer.h
Content     :   A rendering component for axis
Created     :   September 2020
Authors     :   Federico Schliemann
Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

************************************************************************************/
#pragma once

#include <vector>

#include "OVR_Math.h"
#include "FrameParams.h"
#include "Render/SurfaceRender.h"
#include "Render/GlProgram.h"

namespace OVRFW {

class ovrAxisRenderer {
   public:
    ovrAxisRenderer() = default;
    ~ovrAxisRenderer() = default;

    bool Init(size_t count = 64);
    void Shutdown();
    void Update(const std::vector<OVR::Posef>& points);
    void Update(const OVR::Posef* points, size_t count);
    void Render(
        const OVR::Matrix4f& worldMatrix,
        const OVRFW::ovrApplFrameIn& in,
        OVRFW::ovrRendererOutput& out);

   public:
    float AxisSize;

   private:
    GlProgram ProgAxis;
    ovrSurfaceDef AxisSurfaceDef;
    ovrDrawSurface AxisSurface;
    std::vector<OVR::Matrix4f> TransformMatrices;
    GlBuffer InstancedBoneUniformBuffer;
    size_t Count;
};

} // namespace OVRFW
