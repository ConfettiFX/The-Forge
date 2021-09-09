/*******************************************************************************

Filename    :   Main.cpp
Content     :   Simple test app to test filter settings
Created     :
Authors     :   Federico Schliemann
Language    :   C++
Copyright:  Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*******************************************************************************/

#include "SurfaceRenderApp.h"
#include "Input/HandRenderer.h"
#include "Input/ControllerRenderer.h"
#include "Input/SimpleInput.h"
#include "Input/TinyUI.h"
#include "Render/SimpleBeamRenderer.h"

namespace OVRFW {

class BaseSampleApp : public OVRFW::ovrSurfaceRenderApp {
   public:
    BaseSampleApp() : OVRFW::ovrSurfaceRenderApp() {
        BackgroundColor = OVR::Vector4f(1.0f, 0.65f, 0.1f, 1.0f);
    }
    virtual ~BaseSampleApp() = default;

    // Must return true if the application initializes successfully.
    virtual bool AppInit(const OVRFW::ovrAppContext* context) override {
        /// Init Rendering
        if (false == OVRFW::ovrSurfaceRenderApp::AppInit(context)) {
            ALOG("base AppInit::Init FAILED.");
            return false;
        }

        if (false == InitUi(context)) {
            return false;
        }

        return true;
    }

    virtual bool InitUi(const OVRFW::ovrAppContext* context) {
        const ovrJava& jj = *(reinterpret_cast<const ovrJava*>(context->ContextForVrApi()));
        const xrJava ctx = JavaContextConvert(jj);
        if (false == ui_.Init(&ctx, GetFileSys())) {
            ALOG("TinyUI::Init FAILED.");
            return false;
        }

        return true;
    }

    virtual void AppShutdown(const OVRFW::ovrAppContext* context) override {
        OVRFW::ovrSurfaceRenderApp::AppShutdown(context);
        beamRenderer_.Shutdown();
        ui_.Shutdown();
    }

    virtual void SessionInit(ovrMobile* ovr) override {
        /// Disable scene navitgation
        GetScene().SetFootPos({0.0f, 0.0f, 0.0f});
        this->FreeMove = false;
        /// Init session bound objects
        if (false == handRendererL_.Init(ovr, true)) {
            ALOG("AppInit::Init L hand renderer FAILED.");
        }
        if (false == handRendererR_.Init(ovr, false)) {
            ALOG("AppInit::Init R hand renderer FAILED.");
        }

        // Passing nullptr to fileSys and controllerModelFile parameters will create controllers
        // from geometry. Passing the actual ovrFileSys instance and a controller model will render
        // the controller with the textured model
        if (false == controllerRenderL_.Init(true, nullptr, nullptr)) {
            ALOG("AppInit::Init L controller renderer FAILED.");
        }
        if (false == controllerRenderR_.Init(false, nullptr, nullptr)) {
            ALOG("AppInit::Init R controller renderer FAILED.");
        }

        // Initialize pointer pose beam and particle
        beamRenderer_.Init(GetFileSys(), nullptr, OVR::Vector4f(0.0f), 1.0f);
    }

    virtual void SessionEnd() override {
        handRendererL_.Shutdown();
        handRendererR_.Shutdown();
        controllerRenderL_.Shutdown();
        controllerRenderR_.Shutdown();
    }

    // Update state
    virtual void Update(const OVRFW::ovrApplFrameIn& in) override {
        ui_.HitTestDevices().clear();

        auto ovr = GetSessionObject();
        if (ovr != nullptr) {
            simpleInput_.Update(ovr, in.PredictedDisplayTime);
            handRendererL_.Confidence = 0.0f;
            handRendererR_.Confidence = 0.0f;
            if (simpleInput_.IsLeftHandTracked()) {
                handRendererL_.Update(simpleInput_.LeftHandPose());
                handRendererL_.Confidence = 1.0f;
                const bool didPinch =
                    !simpleInput_.WasLeftHandPinching() && simpleInput_.IsLeftHandPinching();
                const OVR::Posef& ray = simpleInput_.LeftHandInputState().PointerPose;
                ui_.AddHitTestRay(ray, didPinch);
            }
            if (simpleInput_.IsRightHandTracked()) {
                handRendererR_.Update(simpleInput_.RightHandPose());
                handRendererR_.Confidence = 1.0f;
                const bool didPinch =
                    !simpleInput_.WasRightHandPinching() && simpleInput_.IsRightHandPinching();
                const OVR::Posef& ray = simpleInput_.RightHandInputState().PointerPose;
                ui_.AddHitTestRay(ray, didPinch);
            }
            if (simpleInput_.IsLeftControllerTracked()) {
                controllerRenderL_.Update(simpleInput_.LeftControllerPose());
                const OVR::Posef& ray = simpleInput_.LeftControllerPose();
                ui_.AddHitTestRay(ray, simpleInput_.IsLeftHandTriggerPressed());
            }
            if (simpleInput_.IsRightControllerTracked()) {
                controllerRenderR_.Update(simpleInput_.RightControllerPose());
                const OVR::Posef& ray = simpleInput_.RightControllerPose();
                ui_.AddHitTestRay(ray, simpleInput_.IsRightHandTriggerPressed());
            }
        }
        ui_.Update(in);
        beamRenderer_.Update(in, ui_.HitTestDevices());
    }

    // Render eye buffers while running
    virtual void Render(const OVRFW::ovrApplFrameIn& in, OVRFW::ovrRendererOutput& out) override {
        /// Render UI
        ui_.Render(in, out);

        /// Render beams
        beamRenderer_.Render(in, out);

        /// Render hands
        handRendererL_.Render(out.Surfaces);
        handRendererR_.Render(out.Surfaces);

        /// Render controllers
        controllerRenderL_.Render(out.Surfaces);
        controllerRenderR_.Render(out.Surfaces);
    }

   protected:
    OVRFW::SimpleInput simpleInput_;
    OVRFW::HandRenderer handRendererL_;
    OVRFW::HandRenderer handRendererR_;
    OVRFW::ControllerRenderer controllerRenderL_;
    OVRFW::ControllerRenderer controllerRenderR_;
    OVRFW::TinyUI ui_;
    OVRFW::SimpleBeamRenderer beamRenderer_;
};

} // namespace OVRFW
