/************************************************************************************

Filename    :   SurfaceRenderApp.cpp
Content     :   Simple application framework that uses SurfaceRenderer
Created     :   April 2020
Authors     :   Federico Schliemann

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

************************************************************************************/
#include "SurfaceRenderApp.h"

using OVR::Bounds3f;
using OVR::Matrix4f;
using OVR::Posef;
using OVR::Quatf;
using OVR::Vector2f;
using OVR::Vector3f;
using OVR::Vector4f;

namespace OVRFW {

ovrSurfaceRenderApp::ovrSurfaceRenderApp(
    const int32_t mainThreadTid,
    const int32_t renderThreadTid,
    const int cpuLevel,
    const int gpuLevel)
    : ovrAppl(mainThreadTid, renderThreadTid, cpuLevel, gpuLevel, true /* useMultiView */),
      BackgroundColor(Vector4f(0.0f, 0.0f, 0.0f, 0.0f)),
      FreeMove(true),
      RenderState(RENDER_STATE_LOADING) {}

bool ovrSurfaceRenderApp::AppInit(const OVRFW::ovrAppContext* context) {
    /// Create Beam & Particle Renderers
    const ovrJava& jj = *(reinterpret_cast<const ovrJava*>(context->ContextForVrApi()));
    const xrJava ctx = JavaContextConvert(jj);
    FileSys = std::unique_ptr<OVRFW::ovrFileSys>(ovrFileSys::Create(ctx));

    if (FileSys) {
        OVRFW::ovrFileSys& fs = *FileSys;
        MaterialParms materialParms;
        materialParms.UseSrgbTextureFormats = false;
        SceneModel = std::unique_ptr<OVRFW::ModelFile>(LoadModelFile(
            fs, "apk:///assets/box.ovrscene", Scene.GetDefaultGLPrograms(), materialParms));
        if (SceneModel != nullptr) {
            Scene.SetWorldModel(*SceneModel);
            Vector3f modelOffset;
            modelOffset.x = 0.5f;
            modelOffset.y = 0.0f;
            modelOffset.z = -2.25f;
            Scene.GetWorldModel()->State.SetMatrix(
                Matrix4f::Scaling(2.5f, 2.5f, 2.5f) * Matrix4f::Translation(modelOffset));
        }
    }

    SurfaceRender.Init();
    return true;
}

void ovrSurfaceRenderApp::AppShutdown(const OVRFW::ovrAppContext* context) {
    SurfaceRender.Shutdown();
}

void ovrSurfaceRenderApp::AppResumed(const OVRFW::ovrAppContext* contet) {
    RenderState = RENDER_STATE_RUNNING;

    /// At this point the context is valid
    SessionInit(GetSessionObject());
}

void ovrSurfaceRenderApp::AppPaused(const OVRFW::ovrAppContext* context) {
    /// At this point the context is no longer valid
    SessionEnd();
}

OVRFW::ovrApplFrameOut ovrSurfaceRenderApp::AppFrame(const OVRFW::ovrApplFrameIn& in) {
    Update(in);
    return OVRFW::ovrApplFrameOut();
}

void ovrSurfaceRenderApp::AppRenderFrame(
    const OVRFW::ovrApplFrameIn& in,
    OVRFW::ovrRendererOutput& out) {
    switch (RenderState) {
        case RENDER_STATE_LOADING: {
            DefaultRenderFrame_Loading(in, out);
        } break;
        case RENDER_STATE_RUNNING: {
            Scene.SetFreeMove(FreeMove);
            /// create a local copy
            OVRFW::ovrApplFrameIn localIn = in;
            if (false == FreeMove) {
                localIn.LeftRemoteJoystick.x = 0.0f;
                localIn.LeftRemoteJoystick.y = 0.0f;
                localIn.RightRemoteJoystick.x = 0.0f;
                localIn.RightRemoteJoystick.y = 0.0f;
            }
            Scene.Frame(localIn);
            Scene.GetFrameMatrices(
                SuggestedEyeFovDegreesX, SuggestedEyeFovDegreesY, out.FrameMatrices);
            Scene.GenerateFrameSurfaceList(out.FrameMatrices, out.Surfaces);
            Render(in, out);
            // submit compositor layers
            SubmitCompositorLayers(in, out);
        } break;
        case RENDER_STATE_ENDING: {
            DefaultRenderFrame_Ending(in, out);
        } break;
    }
}

void ovrSurfaceRenderApp::AppRenderEye(
    const OVRFW::ovrApplFrameIn& in,
    OVRFW::ovrRendererOutput& out,
    int eye) {
    // Render the surfaces returned by Frame.
    SurfaceRender.RenderSurfaceList(
        out.Surfaces,
        out.FrameMatrices.EyeView[0], // always use 0 as it assumes an array
        out.FrameMatrices.EyeProjection[0], // always use 0 as it assumes an array
        eye);
}

void ovrSurfaceRenderApp::AppEyeGLStateSetup(
    const ovrApplFrameIn& in,
    const ovrFramebuffer* fb,
    int eye) {
    GL(glEnable(GL_SCISSOR_TEST));
    GL(glDepthMask(GL_TRUE));
    GL(glEnable(GL_DEPTH_TEST));
    GL(glDepthFunc(GL_LEQUAL));
    GL(glEnable(GL_CULL_FACE));
    GL(glViewport(0, 0, fb->Width, fb->Height));
    GL(glScissor(0, 0, fb->Width, fb->Height));

    GL(glClearColor(BackgroundColor.x, BackgroundColor.y, BackgroundColor.z, BackgroundColor.w));
    GL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));

    // SampleFramework apps were originally written with the presumption that
    // its swapchains and compositor front buffer were RGB.
    // In order to have the colors the same now that its compositing
    // to an sRGB front buffer, we have to write to an sRGB swapchain
    // but with the linear->sRGB conversion disabled on write.
    GL(glDisable(GL_FRAMEBUFFER_SRGB_EXT));
}

void ovrSurfaceRenderApp::SessionInit(ovrMobile* ovr) {}

void ovrSurfaceRenderApp::SessionEnd() {}

void ovrSurfaceRenderApp::Render(const OVRFW::ovrApplFrameIn& in, OVRFW::ovrRendererOutput& out) {}

void ovrSurfaceRenderApp::Update(const OVRFW::ovrApplFrameIn& in) {}

void ovrSurfaceRenderApp::SubmitCompositorLayers(const ovrApplFrameIn& in, ovrRendererOutput& out) {
    DefaultRenderFrame_Running(in, out);
}

} // namespace OVRFW
