/************************************************************************************

Filename    :   SkeletonRenderer.cpp
Content     :   A rendering component for sample skeletons
Created     :   April 2020
Authors     :   Federico Schliemann

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

************************************************************************************/
#include "SkeletonRenderer.h"
#include "Misc/Log.h"

using OVR::Matrix4f;
using OVR::Posef;
using OVR::Quatf;
using OVR::Vector3f;
using OVR::Vector4f;

namespace OVRFW {

bool ovrSkeletonRenderer::Init(OVRFW::ovrFileSys* fs) {
    if (!fs) {
        ALOG("ovrSkeletonRenderer::Init FAILED -> NULL ovrFileSys.");
        return false;
    }
    OVRFW::ovrFileSys& FileSys = *fs;

    /// Defaults
    BoneColor = OVR::Vector4f(0.0f, 0.0f, 1.0f, 1.0f);
    JointRadius = 0.008f;
    BoneWidth = 0.005f;
    DrawAxis = false;

    /// Create Beam & Particle Renderers
    SpriteAtlas = std::make_unique<OVRFW::ovrTextureAtlas>();
    if (SpriteAtlas) {
        SpriteAtlas->Init(FileSys, "apk:///assets/particles2.ktx");
        SpriteAtlas->BuildSpritesFromGrid(4, 2, 8);
        ParticleSystem = std::make_unique<OVRFW::ovrParticleSystem>();
        ParticleSystem->Init(
            2048, SpriteAtlas.get(), ovrParticleSystem::GetDefaultGpuState(), false);
    } else {
        ALOG("ovrSkeletonRenderer::Init FAILED -> could not create SpriteAtlas.");
        return false;
    }
    BeamAtlas = std::make_unique<OVRFW::ovrTextureAtlas>();
    if (BeamAtlas) {
        BeamAtlas->Init(FileSys, "apk:///assets/beams.ktx");
        BeamAtlas->BuildSpritesFromGrid(2, 1, 2);
        BeamRenderer = std::make_unique<OVRFW::ovrBeamRenderer>();
        BeamRenderer->Init(256, true);
    } else {
        ALOG("ovrSkeletonRenderer::Init FAILED -> could not create BeamAtlas.");
        return false;
    }

    /// Create Axis program
    if (AxisRenderer.Init() == false) {
        ALOG("ovrSkeletonRenderer::Init FAILED -> could not create AxisRenderer.");
        return false;
    }

    return true;
}

void ovrSkeletonRenderer::Shutdown() {
    ParticleSystem->Shutdown();
    BeamRenderer->Shutdown();
    SpriteAtlas->Shutdown();
    BeamAtlas->Shutdown();
    AxisRenderer.Shutdown();
}

void ovrSkeletonRenderer::Render(
    const OVR::Matrix4f& worldMatrix,
    const std::vector<ovrJoint>& joints,
    const ovrApplFrameIn& in,
    OVRFW::ovrRendererOutput& out) {
    if (joints.size() != JointHandles.size()) {
        ResetBones(JointHandles);
        JointHandles.resize(joints.size());
    }

    RenderBones(in, worldMatrix, joints, JointHandles, BoneColor, JointRadius, BoneWidth);

    const OVR::Matrix4f projectionMatrix = OVR::Matrix4f(); /// currently Unused
    BeamRenderer->Frame(in, out.FrameMatrices.CenterView, *BeamAtlas);
    ParticleSystem->Frame(in, SpriteAtlas.get(), out.FrameMatrices.CenterView);
    ParticleSystem->RenderEyeView(out.FrameMatrices.CenterView, projectionMatrix, out.Surfaces);
    BeamRenderer->RenderEyeView(out.FrameMatrices.CenterView, projectionMatrix, out.Surfaces);

    /// Instance the Axes ...
    if (DrawAxis) {
        std::vector<OVR::Posef> poses(joints.size());
        for (size_t j = 0; j < joints.size(); ++j) {
            poses[j] = joints[j].Pose;
        }
        AxisRenderer.Update(poses);
        AxisRenderer.Render(worldMatrix, in, out);
    }
}

void ovrSkeletonRenderer::RenderBones(
    const OVRFW::ovrApplFrameIn& frame,
    const OVR::Matrix4f& worldMatrix,
    const std::vector<ovrJoint>& joints,
    jointHandles_t& handles,
    const OVR::Vector4f& boneColor,
    const float jointRadius,
    const float boneWidth) {
    const uint16_t particleAtlasIndex = 0;
    const uint16_t beamAtlasIndex = 0;
    OVRFW::ovrParticleSystem* ps = ParticleSystem.get();
    OVRFW::ovrBeamRenderer* br = BeamRenderer.get();
    OVRFW::ovrTextureAtlas& beamAtlas = *BeamAtlas.get();

    for (int i = 0; i < static_cast<int>(joints.size()); ++i) {
        const ovrJoint& joint = joints[i];
        OVR::Vector3f jwPosition = worldMatrix.Transform(joint.Pose.Translation);

        OVR::Vector4f jointColor = joint.Color;
        if (!handles[i].first.IsValid()) {
            handles[i].first = ps->AddParticle(
                frame,
                jwPosition,
                0.0f,
                Vector3f(0.0f),
                Vector3f(0.0f),
                jointColor,
                ovrEaseFunc::NONE,
                0.0f,
                jointRadius,
                FLT_MAX,
                particleAtlasIndex);
        } else {
            ps->UpdateParticle(
                frame,
                handles[i].first,
                jwPosition,
                0.0f,
                Vector3f(0.0f),
                Vector3f(0.0f),
                jointColor,
                ovrEaseFunc::NONE,
                0.0f,
                jointRadius,
                FLT_MAX,
                particleAtlasIndex);
        }

        if (i > 0) {
            const ovrJoint& parentJoint = joints[joint.ParentIndex];
            OVR::Vector3f pwPosition = worldMatrix.Transform(parentJoint.Pose.Translation);

            if (!handles[i].second.IsValid()) {
                handles[i].second = br->AddBeam(
                    frame,
                    beamAtlas,
                    beamAtlasIndex,
                    boneWidth,
                    pwPosition,
                    jwPosition,
                    boneColor,
                    ovrBeamRenderer::LIFETIME_INFINITE);
            } else {
                br->UpdateBeam(
                    frame,
                    handles[i].second,
                    beamAtlas,
                    beamAtlasIndex,
                    boneWidth,
                    pwPosition,
                    jwPosition,
                    boneColor);
            }
        }
    }
}

void ovrSkeletonRenderer::ResetBones(jointHandles_t& handles) {
    OVRFW::ovrParticleSystem* ps = ParticleSystem.get();
    OVRFW::ovrBeamRenderer* br = BeamRenderer.get();

    for (int i = 0; i < static_cast<int>(handles.size()); ++i) {
        if (handles[i].first.IsValid()) {
            ps->RemoveParticle(handles[i].first);
            handles[i].first.Release();
        }
        if (handles[i].second.IsValid()) {
            br->RemoveBeam(handles[i].second);
            handles[i].second.Release();
        }
    }
}

} // namespace OVRFW
