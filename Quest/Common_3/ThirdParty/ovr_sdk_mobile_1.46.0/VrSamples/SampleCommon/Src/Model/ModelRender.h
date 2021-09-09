/************************************************************************************

Filename    :   ModelRender.h
Content     :   Optimized OpenGL rendering path
Created     :   August 9, 2013
Authors     :   John Carmack

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

************************************************************************************/
#pragma once

#include "OVR_Math.h"
#include "Render/SurfaceRender.h"
#include "ModelFile.h"

#include <vector>

namespace OVRFW {
// The model surfaces are culled and added to the sorted surface list.
// Application specific surfaces from the emit list are also added to the sorted surface list.
// The surface list is sorted such that opaque surfaces come first, sorted front-to-back,
// and transparent surfaces come last, sorted back-to-front.
void BuildModelSurfaceList(
    std::vector<ovrDrawSurface>& surfaceList,
    const std::vector<ModelNodeState*>& emitNodes,
    const std::vector<ovrDrawSurface>& emitSurfaces,
    const OVR::Matrix4f& viewMatrix,
    const OVR::Matrix4f& projectionMatrix);

} // namespace OVRFW
