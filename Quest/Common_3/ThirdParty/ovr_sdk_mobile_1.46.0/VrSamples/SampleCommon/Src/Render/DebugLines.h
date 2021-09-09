/************************************************************************************

Filename    :   DebugLines.h
Content     :   Class that manages and renders debug lines.
Created     :   April 22, 2014
Authors     :   Jonathan E. Wright

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/

#pragma once

#include <vector>
#include "OVR_Math.h"
#include "SurfaceRender.h"

namespace OVRFW {

//==============================================================
// OvrDebugLines
class OvrDebugLines {
   public:
    virtual ~OvrDebugLines() {}

    static OvrDebugLines* Create();
    static void Free(OvrDebugLines*& debugLines);

    virtual void Init() = 0;
    virtual void Shutdown() = 0;

    virtual void BeginFrame(const long long frameNum) = 0;
    virtual void AppendSurfaceList(std::vector<ovrDrawSurface>& surfaceList) = 0;

    virtual void AddLine(
        const OVR::Vector3f& start,
        const OVR::Vector3f& end,
        const OVR::Vector4f& startColor,
        const OVR::Vector4f& endColor,
        const long long endFrame,
        const bool depthTest) = 0;

    virtual void AddPoint(
        const OVR::Vector3f& pos,
        const float size,
        const OVR::Vector4f& color,
        const long long endFrame,
        const bool depthTest) = 0;

    // Add a debug point without a specified color. The axis lines will use default
    // colors: X = red, Y = green, Z = blue (same as Maya).
    virtual void AddPoint(
        const OVR::Vector3f& pos,
        const float size,
        const long long endFrame,
        const bool depthTest) = 0;

    virtual void AddAxes(
        const OVR::Vector3f& origin,
        const OVR::Matrix4f& axes,
        const float size,
        const OVR::Vector4f& color,
        const long long endFrame,
        const bool depthTest) = 0;

    virtual void
    AddBounds(OVR::Posef const& pose, OVR::Bounds3f const& bounds, OVR::Vector4f const& color) = 0;
};

} // namespace OVRFW
