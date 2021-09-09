/*******************************************************************************

Filename    :   Appl.h
Content     :   VR application base class.
Created     :   March 20, 2018
Authors     :   Jonathan Wright
Language    :   C++

Copyright:  Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*******************************************************************************/

#pragma once

#include <cstdint>
#include <vector>
#include <mutex>
#include <memory>

#include "VrApi.h"
#include "VrApi_Helpers.h"
#include "VrApi_Input.h"

#include "OVR_Math.h"

#include "System.h"
#include "FrameParams.h"
#include "OVR_FileSys.h"

#include "Platform/Android/Android.h"
#include <android_native_app_glue.h>
#include <android/keycodes.h>

#include "Render/Egl.h"
#include "Render/Framebuffer.h"
#include "Render/SurfaceRender.h"

inline xrJava JavaContextConvert(const ovrJava& j) {
    xrJava x;
    x.Vm = j.Vm;
    x.Env = j.Env;
    x.ActivityObject = j.ActivityObject;
    return x;
};

namespace OVRFW {

class ovrAppl {
   public:
    //============================
    // public interface
    enum ovrLifecycle { LIFECYCLE_UNKNOWN, LIFECYCLE_RESUMED, LIFECYCLE_PAUSED };

    enum ovrRenderState {
        RENDER_STATE_LOADING, // show the loading icon
        RENDER_STATE_RUNNING, // render frames
        RENDER_STATE_ENDING, // show a black frame transition
    };

    ovrAppl(
        const int32_t mainThreadTid,
        const int32_t renderThreadTid,
        const int cpuLevel,
        const int gpuLevel,
        bool useMultiView = true)
        : CpuLevel(cpuLevel),
          GpuLevel(gpuLevel),
          MainThreadTid(mainThreadTid),
          RenderThreadTid(renderThreadTid),
          UseMultiView(useMultiView),
          NumFramebuffers(useMultiView ? 1 : VRAPI_FRAME_LAYER_EYE_MAX) {}

    // Called from the OS callback when the app pauses or resumes
    void SetLifecycle(const ovrLifecycle lc);

    // Called from the OS lifecycle callback when the app's window is destroyed
    void SetWindow(const void* window);

    // Called one time when the application process starts.
    // Returns true if the application initialized successfully.
    bool Init(const OVRFW::ovrAppContext* context, const ovrInitParms* initParms = nullptr);

    // Called one time when the applicatoin process exits
    void Shutdown(const OVRFW::ovrAppContext* context);

    // Called to handle any lifecycle state changes. This will call
    // AppPaused() and AppResumed()
    void HandleLifecycle(const OVRFW::ovrAppContext* context);

    // Returns true if the application has a valid VR session object.
    bool HasActiveVRSession() const;

    // Called once per frame when the VR session is active.
    ovrApplFrameOut Frame(ovrApplFrameIn& in);

    // Called once per frame to allow the application to render eye buffers.
    void RenderFrame(const ovrApplFrameIn& in);

    // Called from the OS callback when a key event occurs.
    void AddKeyEvent(const int32_t keyCode, const int32_t action);

    // Called from the OS callback when a touch event occurs.
    void AddTouchEvent(const int32_t action, const int32_t x, const int32_t y);

    // Handle VrApi system events.
    void HandleVrApiEvents(ovrApplFrameIn& in);

    // Handle VRAPI input updates.
    void HandleVRInputEvents(ovrApplFrameIn& in);

    // App entry point
    void Run(struct android_app* app);

    //============================
    // public context interface

    // Returns the application's context
    const OVRFW::ovrAppContext* GetContext() const {
        return Context;
    }

    // Returns the application's session obj for VrApi calls
    ovrMobile* GetSessionObject() const {
        return SessionObject;
    }

    void SetRunWhilePaused(bool b) {
        RunWhilePaused = b;
    }

   protected:
    //============================
    // protected interface

    // Returns the application's current lifecycle state
    ovrLifecycle GetLifecycle() const;

    // Returns the application's current window pointer
    const void* GetWindow() const;

    // Set the clock levels immediately, as long as there is a valid VR session.
    void SetClockLevels(const int32_t cpuLevel, const int32_t gpuLevel);

    // Returns the CPU and GPU clock levels last set by SetClockLevels().
    void GetClockLevels(int32_t& cpuLevel, int& gpuLevel) const;

    uint64_t GetFrameIndex() const {
        return FrameIndex;
    }

    double GetDisplayTime() const {
        return DisplayTime;
    }

    int GetNumFramebuffers() const {
        return NumFramebuffers;
    }

    ovrFramebuffer* GetFrameBuffer(int eye) {
        return Framebuffer[NumFramebuffers == 1 ? 0 : eye].get();
    }

    //============================
    // App functions
    // All App* function can be overridden by the derived application class to
    // implement application-specific behaviors

    // Called when the application initializes.
    // Must return true if the application initializes successfully.
    virtual bool AppInit(const OVRFW::ovrAppContext* context);
    // Called when the application shuts down
    virtual void AppShutdown(const OVRFW::ovrAppContext* context);
    // Called when the application is resumed by the system.
    virtual void AppResumed(const OVRFW::ovrAppContext* contet);
    // Called when the application is paused by the system.
    virtual void AppPaused(const OVRFW::ovrAppContext* context);
    // Called once per frame when the VR session is active.
    virtual OVRFW::ovrApplFrameOut AppFrame(const OVRFW::ovrApplFrameIn& in);
    // Called once per frame to allow the application to render eye buffers.
    virtual void AppRenderFrame(const OVRFW::ovrApplFrameIn& in, OVRFW::ovrRendererOutput& out);
    // Called once per eye each frame for default renderer
    virtual void
    AppRenderEye(const OVRFW::ovrApplFrameIn& in, OVRFW::ovrRendererOutput& out, int eye);
    // Called once per eye each frame for default renderer
    virtual void
    AppEyeGLStateSetup(const OVRFW::ovrApplFrameIn& in, const ovrFramebuffer* fb, int eye);
    // Called when app loses focus
    virtual void AppLostFocus();
    // Called when app re-gains focus
    virtual void AppGainedFocus();
    // Called when VrApi event data was lost
    virtual void AppDataLost();
    // Called when the app gains visibility
    virtual void AppVisibilityGained();
    // Called when the app loses visibility
    virtual void AppVisibilityLost();
    // Called when any event other than those with a specific overloaded method
    // is is received via vrapi_PollEvents
    virtual void AppHandleVrApiEvent(const ovrEventHeader* event);

    //============================
    // Default helpers
    void DefaultRenderFrame_Loading(const OVRFW::ovrApplFrameIn& in, OVRFW::ovrRendererOutput& out);
    void DefaultRenderFrame_Running(const OVRFW::ovrApplFrameIn& in, OVRFW::ovrRendererOutput& out);
    void DefaultRenderFrame_Ending(const OVRFW::ovrApplFrameIn& in, OVRFW::ovrRendererOutput& out);

    void ChangeVolume(int volumeDelta);

    void GetIntentStrings(
        std::string& intentFromPackage,
        std::string& intentJSON,
        std::string& intentURI);

   protected:
    float SuggestedEyeFovDegreesX = 90.0f;
    float SuggestedEyeFovDegreesY = 90.0f;
    ovrMobile* SessionObject = nullptr;
    ovrEgl Egl;
    /// VRAPI frame submit
    ovrTracking2 Tracking;
    ovrLayer_Union2 Layers[ovrMaxLayerCount] = {};
    int NumLayers = 0;
    int FrameFlags = 0;

   private:
    const OVRFW::ovrAppContext* Context = nullptr;
    ovrLifecycle Lifecycle = LIFECYCLE_UNKNOWN;
    const void* Window = nullptr;
    uint64_t FrameIndex = 0;
    double DisplayTime = 0.0;
    int32_t CpuLevel = 0;
    int32_t GpuLevel = 0;
    int32_t MainThreadTid = 0;
    int32_t RenderThreadTid = 0;
    uint32_t LastFrameAllButtons = 0u;
    uint32_t LastFrameAllTouches = 0u;
    bool LastFrameHeadsetIsMounted = true;

    bool UseMultiView;
    std::unique_ptr<ovrFramebuffer> Framebuffer[VRAPI_FRAME_LAYER_EYE_MAX];
    int NumFramebuffers;

    std::mutex KeyEventMutex;
    std::vector<ovrKeyEvent> PendingKeyEvents;
    std::mutex TouchEventMutex;
    std::vector<ovrTouchEvent> PendingTouchEvents;
    bool IsAppFocused = false;
    bool RunWhilePaused = false;
};

} // namespace OVRFW
