/************************************************************************************

Filename    :   SkeletonRenderer.h
Content     :   A rendering component for sample skeletons
Created     :   April 2020
Authors     :   Federico Schliemann

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

************************************************************************************/
#pragma once

#include <vector>

#include "Input/Skeleton.h"
#include "OVR_FileSys.h"
#include "Render/TextureAtlas.h"
#include "Render/BeamRenderer.h"
#include "Render/ParticleSystem.h"
#include "Render/GlProgram.h"

#include "AxisRenderer.h"

namespace OVRFW {

typedef std::vector<std::pair<ovrParticleSystem::handle_t, ovrBeamRenderer::handle_t>>
    jointHandles_t;

class ovrSkeletonRenderer {
   public:
    ovrSkeletonRenderer() = default;
    ~ovrSkeletonRenderer() = default;

    bool Init(OVRFW::ovrFileSys* FileSys);
    void Shutdown();
    void Render(
        const OVR::Matrix4f& worldMatrix,
        const std::vector<ovrJoint>& joints,
        const ovrApplFrameIn& in,
        OVRFW::ovrRendererOutput& out);

   public:
    OVR::Vector4f BoneColor;
    float JointRadius;
    float BoneWidth;
    bool DrawAxis;

   protected:
    void ResetBones(jointHandles_t& handles);
    void RenderBones(
        const ovrApplFrameIn& frame,
        const OVR::Matrix4f& worldMatrix,
        const std::vector<ovrJoint>& joints,
        jointHandles_t& handles,
        const OVR::Vector4f& boneColor,
        const float jointRadius,
        const float boneWidth);

   private:
    std::unique_ptr<OVRFW::ovrParticleSystem> ParticleSystem;
    std::unique_ptr<OVRFW::ovrBeamRenderer> BeamRenderer;
    std::unique_ptr<OVRFW::ovrTextureAtlas> SpriteAtlas;
    std::unique_ptr<OVRFW::ovrTextureAtlas> BeamAtlas;
    jointHandles_t JointHandles;
    OVRFW::ovrAxisRenderer AxisRenderer;
};

} // namespace OVRFW
