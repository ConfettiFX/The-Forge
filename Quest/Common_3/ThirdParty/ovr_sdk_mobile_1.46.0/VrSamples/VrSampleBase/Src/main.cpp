/*******************************************************************************

Filename    :   Main.cpp
Content     :   Base project for mobile VR samples
Created     :   February 21, 2018
Authors     :   John Carmack, J.M.P. van Waveren, Jonathan Wright
Language    :   C++

Copyright:	Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*******************************************************************************/

#include <cstdint>
#include <cstdio>
#include <stdlib.h> // for exit()
#include <unistd.h> // for sleep()

#include "Platform/Android/Android.h"

#include <android/window.h>
#include <android/native_window_jni.h>
#include <android_native_app_glue.h>

#include "VrApi.h"

#include "Appl.h"

//==============================================================================
// Our custom application class

class ovrSampleAppl : public OVRFW::ovrAppl {
   public:
    enum ovrRenderState {
        RENDER_STATE_LOADING, // show the loading icon
        RENDER_STATE_RUNNING, // render frames
        RENDER_STATE_ENDING // show a black frame transition
    };

    ovrSampleAppl(
        const int32_t mainThreadTid,
        const int32_t renderThreadTid,
        const int cpuLevel,
        const int gpuLevel)
        : ovrAppl(mainThreadTid, renderThreadTid, cpuLevel, gpuLevel) {}
    virtual ~ovrSampleAppl() {}

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

   private:
    ovrRenderState RenderState = RENDER_STATE_LOADING;
};

bool ovrSampleAppl::AppInit(const OVRFW::ovrAppContext* appContext) {
    ALOGV("AppInit - enter");
    ALOGV("AppInit - exit");
    return true;
}

void ovrSampleAppl::AppShutdown(const OVRFW::ovrAppContext*) {
    RenderState = RENDER_STATE_ENDING;
    ALOGV("AppShutdown - enter");
    ALOGV("AppShutdown - exit");
}

void ovrSampleAppl::AppResumed(const OVRFW::ovrAppContext* /* context */) {
    ALOGV("ovrSampleAppl::AppResumed");
    // simulate loading
    sleep(5);
    RenderState = RENDER_STATE_RUNNING;
}

void ovrSampleAppl::AppPaused(const OVRFW::ovrAppContext* /* context */) {
    ALOGV("ovrSampleAppl::AppPaused");
}

OVRFW::ovrApplFrameOut ovrSampleAppl::AppFrame(const OVRFW::ovrApplFrameIn&) {
    return OVRFW::ovrApplFrameOut();
}

void ovrSampleAppl::AppRenderFrame(const OVRFW::ovrApplFrameIn& in, OVRFW::ovrRendererOutput& out) {
    switch (RenderState) {
        case RENDER_STATE_LOADING: {
            DefaultRenderFrame_Loading(in, out);
        } break;
        case RENDER_STATE_RUNNING: {
            DefaultRenderFrame_Running(in, out);
        } break;
        case RENDER_STATE_ENDING: {
            DefaultRenderFrame_Ending(in, out);
        } break;
    }
}

//==============================================================
// android_main
//==============================================================
void android_main(struct android_app* app) {
    std::unique_ptr<ovrSampleAppl> appl =
        std::unique_ptr<ovrSampleAppl>(new ovrSampleAppl(0, 0, 0, 0));
    appl->Run(app);
}
