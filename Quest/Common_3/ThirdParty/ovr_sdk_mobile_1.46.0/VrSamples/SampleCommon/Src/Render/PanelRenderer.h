/************************************************************************************

Filename    :   PanelRenderer.h
Content     :   Class that manages and renders quad-based panels with custom shaders.
Created     :   September 19, 2019
Authors     :   Federico Schliemann
Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/
#pragma once

#include <vector>

#include "OVR_Math.h"

#include "SurfaceRender.h"
#include "GlProgram.h"

#include "FrameParams.h"

namespace OVRFW {

//==============================================================
// PanelRenderer
class ovrPanelRenderer {
   public:
    static const int NUM_DATA_POINTS = 256;

    ovrPanelRenderer()
        : ModelScale(1.0f),
          UniformChannelEnable(1.0f, 1.0f, 1.0f, 1.0f),
          UniformGraphOffset(1.0f, 0.0f),
          WritePosition(0) {
        UniformChannelData.resize(NUM_DATA_POINTS, OVR::Vector4f(0.0f));
        UniformChannelColor[0] = OVR::Vector4f(0.75f, 0.0f, 0.0f, 0.5f);
        UniformChannelColor[1] = OVR::Vector4f(0.0f, 0.75f, 0.0f, 0.5f);
        UniformChannelColor[2] = OVR::Vector4f(0.0f, 0.0f, 0.75f, 0.5f);
        UniformChannelColor[3] = OVR::Vector4f(0.5f, 0.0f, 0.5f, 0.5f);
    }
    ~ovrPanelRenderer() = default;

    virtual void Init();
    virtual void Shutdown();

    virtual void Update(const OVR::Vector4f& dataPoint);
    virtual void Render(std::vector<ovrDrawSurface>& surfaceList);

    void SetPose(const OVR::Posef& pose) {
        ModelMatrix = OVR::Matrix4f(pose) * OVR::Matrix4f::Scaling(ModelScale);
    }
    void SetScale(OVR::Vector3f v) {
        ModelScale = v;
    }
    void SetChannelColor(int channel, OVR::Vector4f c) {
        UniformChannelColor[channel % 4] = c;
    }

   private:
    ovrSurfaceDef SurfaceDef;
    GlProgram Program;
    OVR::Matrix4f ModelMatrix;
    OVR::Vector3f ModelScale;

    OVR::Vector4f UniformChannelEnable;
    OVR::Vector2f UniformGraphOffset;
    std::vector<OVR::Vector4f> UniformChannelData;
    OVR::Vector4f UniformChannelColor[4];

    GlBuffer ChannelDataBuffer;
    int WritePosition;
};

} // namespace OVRFW
