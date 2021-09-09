/************************************************************************************

Filename    :   BeamRenderer.h
Content     :   Class that manages and renders view-oriented beams.
Created     :   October 23, 2015
Authors     :   Jonathan E. Wright

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/
#pragma once

#include <vector>

#include "OVR_Math.h"
#include "OVR_TypesafeNumber.h"

#include "FrameParams.h"
#include "Render/SurfaceRender.h"
#include "Render/GlProgram.h"

#include "TextureAtlas.h"
#include "EaseFunctions.h"

namespace OVRFW {

//==============================================================
// ovrBeamRenderer
class ovrBeamRenderer {
   public:
    static const int MAX_BEAMS = (1ULL << (sizeof(uint16_t) * 8)) - 1;
    enum ovrBeamHandle { INVALID_BEAM_HANDLE = MAX_BEAMS };
    typedef OVR::TypesafeNumberT<uint16_t, ovrBeamHandle, INVALID_BEAM_HANDLE> handle_t;

    static float LIFETIME_INFINITE;

    ovrBeamRenderer();
    ~ovrBeamRenderer();

    void Init(const int maxBeams, const bool depthTest);
    void Shutdown();

    void Frame(
        const OVRFW::ovrApplFrameIn& frame,
        const OVR::Matrix4f& centerViewMatrix,
        const class ovrTextureAtlas& atlas);
    void Frame(const OVRFW::ovrApplFrameIn& frame, const OVR::Matrix4f& centerViewMatrix);

    void SetPose(const OVR::Posef& pose);

    void RenderEyeView(
        const OVR::Matrix4f& viewMatrix,
        const OVR::Matrix4f& projMatrix,
        std::vector<ovrDrawSurface>& surfaceList);
    void Render(std::vector<ovrDrawSurface>& surfaceList);

    // If lifeTime == LIFETIME_INFINITE, then the beam will never be automatically removed and
    // it can be referenced by handle. The handle will be returned from this function.
    // If the lifeTime != LIFETIME_INFINITE, then this function will still add the beam (if the
    // max beams has not been reached) but return a handle == MAX_BEAMS
    handle_t AddBeam(
        const OVRFW::ovrApplFrameIn& frame,
        const ovrTextureAtlas& atlas,
        const int atlasIndex,
        const float width,
        const OVR::Vector3f& start,
        const OVR::Vector3f& end,
        const OVR::Vector4f& initialColor,
        const float lifeTime);

    // Updates the properties of the beam with the specified handle
    void UpdateBeam(
        const OVRFW::ovrApplFrameIn& frame,
        const handle_t handle,
        const ovrTextureAtlas& atlas,
        const int atlasIndex,
        const float width,
        const OVR::Vector3f& start,
        const OVR::Vector3f& end,
        const OVR::Vector4f& initialColor);

    handle_t AddBeam(
        const OVRFW::ovrApplFrameIn& frame,
        const float width,
        const OVR::Vector3f& start,
        const OVR::Vector3f& end,
        const OVR::Vector4f& initialColor);
    void UpdateBeam(
        const OVRFW::ovrApplFrameIn& frame,
        const handle_t handle,
        const float width,
        const OVR::Vector3f& start,
        const OVR::Vector3f& end,
        const OVR::Vector4f& initialColor);

    // removes the beam with the specified handle
    void RemoveBeam(handle_t const handle);

   private:
    void FrameInternal(
        const OVRFW::ovrApplFrameIn& frame,
        const OVR::Matrix4f& centerViewMatrix,
        const class ovrTextureAtlas* atlas);
    void UpdateBeamInternal(
        const OVRFW::ovrApplFrameIn& frame,
        const handle_t handle,
        const ovrTextureAtlas* atlas,
        const int atlasIndex,
        const float width,
        const OVR::Vector3f& start,
        const OVR::Vector3f& end,
        const OVR::Vector4f& initialColor,
        const float lifeTime);

    struct ovrBeamInfo {
        ovrBeamInfo()
            : StartTime(0.0),
              LifeTime(0.0f),
              Width(0.0f),
              StartPos(0.0f),
              EndPos(0.0f),
              InitialColor(0.0f),
              TexCoords(),
              AtlasIndex(0),
              Handle(MAX_BEAMS),
              EaseFunc(ovrEaseFunc::NONE) {}

        double StartTime;
        float LifeTime;
        float Width;
        OVR::Vector3f StartPos;
        OVR::Vector3f EndPos;
        OVR::Vector4f InitialColor;
        OVR::Vector2f TexCoords[2]; // tex coords are in the space of the atlas entry
        uint16_t AtlasIndex; // index in the texture atlas
        handle_t Handle;
        ovrEaseFunc EaseFunc;
    };

    ovrSurfaceDef Surf;

    std::vector<ovrBeamInfo> BeamInfos;
    std::vector<handle_t> ActiveBeams;
    std::vector<handle_t> FreeBeams;

    int MaxBeams;

    GlProgram TextureProgram;
    GlProgram ParametricProgram;
    OVR::Matrix4f ModelMatrix;
};

} // namespace OVRFW
