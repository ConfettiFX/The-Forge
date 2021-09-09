/************************************************************************************

Filename    :   SurfaceRenderApp.h
Content     :   Simple application framework that uses SurfaceRenderer
Created     :   April 2020
Authors     :   Federico Schliemann

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

************************************************************************************/
#pragma once

#include <vector>
#include <string>
#include <memory>
#include <unordered_map>
#include <functional>

#include "OVR_Math.h"

#include "Appl.h"
#include "Model/SceneView.h"
#include "Render/SurfaceRender.h"

namespace OVRFW {

class ovrSurfaceRenderApp : public OVRFW::ovrAppl {
   public:
    ovrSurfaceRenderApp(
        const int32_t mainThreadTid,
        const int32_t renderThreadTid,
        const int cpuLevel,
        const int gpuLevel);

    ovrSurfaceRenderApp() : ovrSurfaceRenderApp(0, 0, 0, 0) {}

    virtual ~ovrSurfaceRenderApp() = default;

    // Called when the application initializes.
    // Must return true if the application initializes successfully.
    virtual bool AppInit(const OVRFW::ovrAppContext* context) override;
    // Called when the application shuts down
    virtual void AppShutdown(const OVRFW::ovrAppContext* context) override;
    // Called when the application is resumed by the system.
    virtual void AppResumed(const OVRFW::ovrAppContext* contet) override;
    // Called when the application is paused by the system.
    virtual void AppPaused(const OVRFW::ovrAppContext* context) override;
    // Called once per frame when the VR session is active.
    virtual OVRFW::ovrApplFrameOut AppFrame(const OVRFW::ovrApplFrameIn& in) override;
    // Called once per frame to allow the application to render eye buffers.
    virtual void AppRenderFrame(const OVRFW::ovrApplFrameIn& in, OVRFW::ovrRendererOutput& out)
        override;
    // Called once per eye each frame for default renderer
    virtual void
    AppRenderEye(const OVRFW::ovrApplFrameIn& in, OVRFW::ovrRendererOutput& out, int eye) override;
    // Called once per eye each frame for default renderer
    virtual void AppEyeGLStateSetup(const ovrApplFrameIn& in, const ovrFramebuffer* fb, int eye)
        override;

    // Session management
    virtual void SessionInit(ovrMobile* ovr);
    virtual void SessionEnd();
    // Update state
    virtual void Update(const OVRFW::ovrApplFrameIn& in);
    // Render eye buffers while running
    virtual void Render(const OVRFW::ovrApplFrameIn& in, OVRFW::ovrRendererOutput& out);
    // Manage Compositor submit
    virtual void SubmitCompositorLayers(const ovrApplFrameIn& in, ovrRendererOutput& out);

    // App state share
    OVRFW::ovrFileSys* GetFileSys() {
        return FileSys.get();
    }
    OVRFW::ovrSurfaceRender& GetSurfaceRender() {
        return SurfaceRender;
    }
    OVRFW::OvrSceneView& GetScene() {
        return Scene;
    }

   public:
    OVR::Vector4f BackgroundColor;
    bool FreeMove;

   private:
    ovrRenderState RenderState;
    OVRFW::ovrSurfaceRender SurfaceRender;
    OVRFW::OvrSceneView Scene;
    std::unique_ptr<OVRFW::ovrFileSys> FileSys;
    std::unique_ptr<OVRFW::ModelFile> SceneModel;
};

} // namespace OVRFW
