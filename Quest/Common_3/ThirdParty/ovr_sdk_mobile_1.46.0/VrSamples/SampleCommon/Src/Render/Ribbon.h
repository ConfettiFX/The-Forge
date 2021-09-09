/************************************************************************************

Filename    :   Ribbon.h
Content     :   Class that renders connected polygon strips from a list of points
Created     :   6/16/2017
Authors     :   Jonathan E. Wright

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

************************************************************************************/

#pragma once

#include <vector>

#include "OVR_Math.h"
#include "PointList.h"
#include "SurfaceRender.h"

namespace OVRFW {

//==============================================================
// ovrRibbon
class ovrRibbon {
   public:
    ovrRibbon(const ovrPointList& pointList, const float width, const OVR::Vector4f& color);
    ~ovrRibbon();

    void AddPoint(ovrPointList& pointList, const OVR::Vector3f& point);
    void Update(
        const ovrPointList& pointList,
        const OVR::Matrix4f& centerViewMatrix,
        const bool invertAlpha);
    void GenerateSurfaceList(std::vector<ovrDrawSurface>& surfaceList) const;

   private:
    float HalfWidth;
    OVR::Vector4f Color;
    ovrSurfaceDef Surface;
    GlTexture Texture;
};

} // namespace OVRFW
