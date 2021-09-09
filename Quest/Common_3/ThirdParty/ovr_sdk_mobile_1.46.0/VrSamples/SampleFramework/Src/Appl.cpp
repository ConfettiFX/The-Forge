/*******************************************************************************

Filename    :   Appl.cpp
Content     :   VR application base class.
Created     :   March 20, 2018
Authors     :   Jonathan Wright
Language    :   C++

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*******************************************************************************/

#include "Appl.h"
#include "CompilerUtils.h"

#include "OVR_Math.h"
#include "JniUtils.h"

#include "Platform/Android/Android.h"

#include <android/window.h>
#include <android/native_window_jni.h>

#include <memory>

#include "VrApi_SystemUtils.h"

// android_handle_cmd
//==============================================================
void android_handle_cmd(struct android_app* app, int32_t cmd) {
    OVRFW::ovrAppl* appl = (OVRFW::ovrAppl*)app->userData;
    if (appl == nullptr) {
        ALOGV("android_handle_cmd with NULL user data!");
        return;
    }

    switch (cmd) {
        case APP_CMD_START:
            ALOGV("APP_CMD_START");
            break;
        case APP_CMD_RESUME:
            ALOGV("APP_CMD_RESUME");
            appl->SetLifecycle(OVRFW::ovrAppl::LIFECYCLE_RESUMED);
            break;
        case APP_CMD_PAUSE:
            ALOGV("APP_CMD_PAUSE");
            appl->SetLifecycle(OVRFW::ovrAppl::LIFECYCLE_PAUSED);
            break;
        case APP_CMD_STOP:
            ALOGV("APP_CMD_STOP");
            break;
        case APP_CMD_DESTROY:
            ALOGV("APP_CMD_DESTROY");
            appl->SetWindow(nullptr);
            break;
        case APP_CMD_INIT_WINDOW:
            ALOGV("APP_CMD_INIT_WINDOW");
            appl->SetWindow(app->window);
            break;
        case APP_CMD_TERM_WINDOW:
            ALOGV("APP_TERM_WINDOW");
            appl->SetWindow(nullptr);
            break;
    }
}

//==============================================================
// android_handle_input
//
// TODO: This handles events that come through normal Android input
// routing, such as keyboard keys. Headset and controller already
// come through VrApi input APIs, so we should not be encouraging
// this route. This is left in for reference in the initial
// check in, but will be stubbed in later change sets.
//==============================================================
int32_t android_handle_input(struct android_app* app, AInputEvent* event) {
    OVRFW::ovrAppl* appl = (OVRFW::ovrAppl*)app->userData;
    if (appl == nullptr) {
        ALOGV("android_handle_input with NULL user data!");
        return 0;
    }

    const int type = AInputEvent_getType(event);
    if (type == AINPUT_EVENT_TYPE_KEY) {
        const int keyCode = AKeyEvent_getKeyCode(event);
        const int action = AKeyEvent_getAction(event);
        appl->AddKeyEvent(keyCode, action);
        return 1; // we eat all other key events
    } else if (type == AINPUT_EVENT_TYPE_MOTION) {
        const int action = AKeyEvent_getAction(event) & AMOTION_EVENT_ACTION_MASK;
        const float x = AMotionEvent_getRawX(event, 0);
        const float y = AMotionEvent_getRawY(event, 0);
        appl->AddTouchEvent(action, x, y);
        return 1; // we eat all touch events
    }
    return 0;
}

namespace OVRFW {
void ovrAppl::SetLifecycle(const ovrLifecycle lc) {
    Lifecycle = lc;
}

ovrAppl::ovrLifecycle ovrAppl::GetLifecycle() const {
    return Lifecycle;
}

void ovrAppl::SetWindow(const void* window) {
    Window = window;
}

const void* ovrAppl::GetWindow() const {
    return Window;
}

// set the clock levels immediately, as long as there is a valid VR session.
void ovrAppl::SetClockLevels(const int32_t cpuLevel, const int32_t gpuLevel) {
    CpuLevel = cpuLevel;
    GpuLevel = gpuLevel;
    vrapi_SetClockLevels(SessionObject, cpuLevel, gpuLevel);
}

void ovrAppl::GetClockLevels(int32_t& cpuLevel, int& gpuLevel) const {
    cpuLevel = CpuLevel;
    gpuLevel = GpuLevel;
}

bool ovrAppl::Init(const ovrAppContext* context, const ovrInitParms* initParms) {
    ALOGV("ovrAppl::Init");
    Context = context;
    const ovrJava* java = reinterpret_cast<const ovrJava*>(context->ContextForVrApi());

    int32_t result = VRAPI_INITIALIZE_SUCCESS;
    if (initParms == nullptr) {
        ovrInitParms localInitParms = vrapi_DefaultInitParms(java);
        result = vrapi_Initialize(&localInitParms);
    } else {
        result = vrapi_Initialize(initParms);
    }

    if (result != VRAPI_INITIALIZE_SUCCESS) {
        ALOGV("ovrAppl::Init - failed to Initialize initParms=%p", initParms);
        return false;
    }

    ovrEgl_Clear(&Egl);
    ovrEgl_CreateContext(&Egl, NULL);
    EglInitExtensions();

    SuggestedEyeFovDegreesX =
        vrapi_GetSystemPropertyFloat(java, VRAPI_SYS_PROP_SUGGESTED_EYE_FOV_DEGREES_X);
    SuggestedEyeFovDegreesY =
        vrapi_GetSystemPropertyFloat(java, VRAPI_SYS_PROP_SUGGESTED_EYE_FOV_DEGREES_Y);

    ALOGV("ovrAppl::Init - Use Multiview: %s", UseMultiView ? "true" : "false");
    GlProgram::SetUseMultiview(UseMultiView);

    // Init renderer
    static const int NUM_MULTI_SAMPLES = 4;
    for (int eye = 0; eye < NumFramebuffers; eye++) {
        Framebuffer[eye] = std::unique_ptr<ovrFramebuffer>(new ovrFramebuffer());
        ovrFramebuffer_Clear(Framebuffer[eye].get());
        ovrFramebuffer_Create(
            Framebuffer[eye].get(),
            UseMultiView,
            GL_SRGB8_ALPHA8,
            vrapi_GetSystemPropertyInt(java, VRAPI_SYS_PROP_SUGGESTED_EYE_TEXTURE_WIDTH),
            vrapi_GetSystemPropertyInt(java, VRAPI_SYS_PROP_SUGGESTED_EYE_TEXTURE_HEIGHT),
            NUM_MULTI_SAMPLES);
    }

    bool appResult = AppInit(context);
    if (!appResult) {
        AppShutdown(context);
    }

    ALOGV("ovrAppl::Init - ALL DONE");
    return appResult;
}

bool ovrAppl::AppInit(const ovrAppContext* /* context */) {
    ALOGV("Default AppInit called!");
    return true;
}

void ovrAppl::Shutdown(const ovrAppContext* context) {
    Context = nullptr;
    AppShutdown(context);

    for (int eye = 0; eye < NumFramebuffers; eye++) {
        ovrFramebuffer_Destroy(Framebuffer[eye].get());
    }

    ovrEgl_DestroyContext(&Egl);

    vrapi_Shutdown();
}

void ovrAppl::AppShutdown(const ovrAppContext* /* context */) {
    ALOGV("Default AppShutdown called!");
}

void ovrAppl::AppResumed(const ovrAppContext* /* context */) {}

void ovrAppl::AppPaused(const ovrAppContext* /* context */) {}

static void
SubmitLoadingIcon(ovrMobile* sessionObject, const uint64_t frameIndex, const double displayTime) {
    ovrLayer_Union2 layers[ovrMaxLayerCount] = {};

    // black layer
    ovrLayerProjection2 blackLayer = vrapi_DefaultLayerBlackProjection2();
    layers[0].Projection = blackLayer;
    // loading icon layer
    ovrLayerLoadingIcon2 iconLayer = vrapi_DefaultLayerLoadingIcon2();
    layers[1].LoadingIcon = iconLayer;

    const ovrLayerHeader2* layerPtrs[ovrMaxLayerCount] = {&layers[0].Header, &layers[1].Header};

    ovrSubmitFrameDescription2 frameDesc = {};
    frameDesc.Flags = VRAPI_FRAME_FLAG_FLUSH;
    frameDesc.SwapInterval = 1;
    frameDesc.FrameIndex = frameIndex;
    frameDesc.DisplayTime = displayTime;
    frameDesc.LayerCount = 2;
    frameDesc.Layers = layerPtrs;

    vrapi_SubmitFrame2(sessionObject, &frameDesc);
}

void ovrAppl::HandleLifecycle(const ovrAppContext* context) {
    if (Lifecycle == LIFECYCLE_UNKNOWN) {
        ALOGV("LIFECYCLE_UNKNOWN");
        return;
    }

    if (Lifecycle == LIFECYCLE_RESUMED && Window != nullptr) {
        if (SessionObject == nullptr) {
            ovrModeParms modeParms = vrapi_DefaultModeParms(
                reinterpret_cast<const ovrJava*>(context->ContextForVrApi()));

            modeParms.Flags &= ~VRAPI_MODE_FLAG_RESET_WINDOW_FULLSCREEN;
            modeParms.Flags |= VRAPI_MODE_FLAG_NATIVE_WINDOW;
            modeParms.Flags |= VRAPI_MODE_FLAG_FRONT_BUFFER_SRGB;
            modeParms.Display = (size_t)Egl.Display;
            modeParms.WindowSurface = (size_t)Window;
            modeParms.ShareContext = (size_t)Egl.Context;

            ALOGV("eglGetCurrentSurface( EGL_DRAW ) = %p", eglGetCurrentSurface(EGL_DRAW));
            ALOGV("vrapi_EnterVrMode()");

            SessionObject = vrapi_EnterVrMode(&modeParms);

            ALOGV("eglGetCurrentSurface( EGL_DRAW ) = %p", eglGetCurrentSurface(EGL_DRAW));

            if (SessionObject == nullptr) {
                ALOGE("Invalid ANativeWindow!");
                Window = nullptr;
            } else {
                vrapi_SetClockLevels(SessionObject, CpuLevel, GpuLevel);
                if (MainThreadTid > 0) {
                    vrapi_SetPerfThread(SessionObject, VRAPI_PERF_THREAD_TYPE_MAIN, MainThreadTid);
                }
                if (RenderThreadTid > 0) {
                    vrapi_SetPerfThread(
                        SessionObject, VRAPI_PERF_THREAD_TYPE_RENDERER, RenderThreadTid);
                }
                vrapi_SetTrackingSpace(SessionObject, VRAPI_TRACKING_SPACE_LOCAL_FLOOR);
            }

            {
                ovrHmdColorDesc colorDesc{};
                colorDesc.ColorSpace = VRAPI_COLORSPACE_QUEST;
                vrapi_SetClientColorDesc(SessionObject, &colorDesc);
            }

            SubmitLoadingIcon(SessionObject, GetFrameIndex(), GetDisplayTime());
            AppResumed(context);
        }
    } else {
        if (SessionObject != nullptr) {
            // TODO: if separate render thread, wait on render thread before leaving VR mode
            AppPaused(context);

            ALOGV("eglGetCurrentSurface( EGL_DRAW ) = %p", eglGetCurrentSurface(EGL_DRAW));
            ALOGV("vrapi_LeaveVrMode()");

            vrapi_LeaveVrMode(SessionObject);
            SessionObject = nullptr;

            ALOGV("eglGetCurrentSurface( EGL_DRAW ) = %p", eglGetCurrentSurface(EGL_DRAW));
        }
    }
}

bool ovrAppl::HasActiveVRSession() const {
    return SessionObject != nullptr;
}

void ovrAppl::ChangeVolume(int volumeDelta) {
    ALOGV("ovrAppl::ChangeVolume %d", volumeDelta);

    /// Call the Volume API like the Java method below throw JNI
    /*
        private void adjustVolume(int volumeDelta) {
            String audioManagerName = Context.AUDIO_SERVICE;
            AudioManager audio = (AudioManager) getSystemService(audioManagerName);
            audio.adjustStreamVolume(AudioManager.STREAM_MUSIC, volumeDelta, 0);
        }
    */

    /// Get JNI
    const ovrJava& ctx = *(reinterpret_cast<const ovrJava*>(GetContext()->ContextForVrApi()));
    TempJniEnv env(ctx.Vm);

    /// Find all classes
    JavaClass contextClass(env, env->FindClass("android/content/Context"));
    if (0 == contextClass.GetJClass()) {
        ALOGV("contextClass = 0");
        return;
    }
    JavaClass audioManagerClass(env, env->FindClass("android/media/AudioManager"));
    if (0 == audioManagerClass.GetJClass()) {
        ALOGV("audioManagerClass = 0");
        return;
    }
    /// Find all fields/methods
    jfieldID asField =
        env->GetStaticFieldID(contextClass.GetJClass(), "AUDIO_SERVICE", "Ljava/lang/String;");
    if (0 == asField) {
        ALOGV("asField = %p ", (void*)asField);
        return;
    }
    jmethodID getSystemServiceID = env->GetMethodID(
        contextClass.GetJClass(), "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;");
    if (0 == getSystemServiceID) {
        ALOGV("getSystemServiceID = %p ", (void*)getSystemServiceID);
        return;
    }
    jmethodID adjustStreamVolumeID =
        env->GetMethodID(audioManagerClass.GetJClass(), "adjustStreamVolume", "(III)V");
    if (0 == adjustStreamVolumeID) {
        ALOGV("adjustStreamVolumeID = %p ", (void*)adjustStreamVolumeID);
        return;
    }

    /// String audioManagerName = Context.AUDIO_SERVICE;
    JavaObject audioManagerName(env, env->GetStaticObjectField(contextClass.GetJClass(), asField));
    if (0 == audioManagerName.GetJObject()) {
        ALOGV("audioManagerName = 0");
        return;
    }

    /// AudioManager audio = (AudioManager) getSystemService(audioManagerName);
    JavaObject audioManagerObject(
        env,
        env->CallObjectMethod(
            ctx.ActivityObject, getSystemServiceID, audioManagerName.GetJObject()));
    if (0 == audioManagerObject.GetJObject()) {
        ALOGV("audioManagerObject = 0");
        return;
    }

    /// audio.adjustStreamVolume(AudioManager.STREAM_MUSIC, volumeDelta, 0);
    env->CallVoidMethod(
        audioManagerObject.GetJObject(),
        adjustStreamVolumeID,
        3 /* STREAM_MUSIC */,
        volumeDelta,
        0);
}

void ovrAppl::GetIntentStrings(
    std::string& intentFromPackage,
    std::string& intentJSON,
    std::string& intentURI) {
    /// Get JNI
    const ovrJava& ctx = *(reinterpret_cast<const ovrJava*>(GetContext()->ContextForVrApi()));
    TempJniEnv env(ctx.Vm);

    const char* INTENT_KEY_CMD = "intent_cmd";
    const char* INTENT_KEY_FROM_PKG = "intent_pkg";

    jobject me = ctx.ActivityObject;

    intentFromPackage = "";
    intentJSON = "";
    intentURI = "";

    /// Find all classes
    JavaClass acl(env, env->GetObjectClass(me));
    if (0 == acl.GetJClass()) {
        ALOGV("acl = 0");
        return;
    }

    const jmethodID giid =
        env->GetMethodID(acl.GetJClass(), "getIntent", "()Landroid/content/Intent;");
    if (0 == giid) {
        ALOGV("giid = 0");
        return;
    }

    jobject jintent = env->CallObjectMethod(me, giid);
    if (0 == jintent) {
        ALOGV("intent = 0");
        return;
    }
    JavaObject intent(env, jintent); // Got our intent

    JavaClass icl(env, env->GetObjectClass(intent.GetJObject())); // class pointer of Intent
    if (0 == icl.GetJClass()) {
        ALOGV("icl = 0");
        return;
    }

    const jmethodID gseid = env->GetMethodID(
        icl.GetJClass(), "getStringExtra", "(Ljava/lang/String;)Ljava/lang/String;");
    if (0 == gseid) {
        ALOGV("gseid = 0");
        return;
    }

    JavaString intent_key_cmd(env, INTENT_KEY_CMD);
    if (0 == intent_key_cmd.GetJObject()) {
        ALOGV("intent_key_cmd = 0");
        return;
    }

    jstring jintent_cmd =
        (jstring)env->CallObjectMethod(intent.GetJObject(), gseid, intent_key_cmd.GetJObject());
    if (jintent_cmd != 0) {
        JavaString intent_cmd(env, jintent_cmd);
        const char* intent_cmd_ch = env->GetStringUTFChars(intent_cmd.GetJString(), 0);
        ALOGV("GetIntentStrings - intent_cmd_ch = `%s`", intent_cmd_ch);
        intentJSON = intent_cmd_ch;
        env->ReleaseStringUTFChars(intent_cmd.GetJString(), intent_cmd_ch);
    }

    JavaString intent_key_from_pkg(env, INTENT_KEY_FROM_PKG);
    if (0 == intent_key_from_pkg.GetJObject()) {
        ALOGV("intent_key_from_pkg = 0");
        return;
    }
    jstring jintent_pkg = (jstring)env->CallObjectMethod(
        intent.GetJObject(), gseid, intent_key_from_pkg.GetJObject());
    if (0 == jintent_pkg) {
        ALOGV("intent_key_from_pkg = 0");
        return;
    }

    JavaString intent_pkg(env, jintent_pkg);
    const char* intent_pkg_ch = env->GetStringUTFChars(intent_pkg.GetJString(), 0);
    ALOGV("GetIntentStrings - intent_pkg_ch = `%s`", intent_pkg_ch);
    intentFromPackage = intent_pkg_ch;
    env->ReleaseStringUTFChars(intent_pkg.GetJString(), intent_pkg_ch);

    // Get Uri
    const jmethodID igd = env->GetMethodID(icl.GetJClass(), "getData", "()Landroid/net/Uri;");
    if (0 == igd) {
        ALOGV("igd = 0");
        return;
    }

    jobject juri = env->CallObjectMethod(intent.GetJObject(), igd);
    if (0 == juri) {
        ALOGV("juri = 0");
        return;
    }

    JavaObject uri(env, juri);
    jclass uriclass = (jclass)env->FindClass("android/net/Uri");
    if (0 == uriclass) {
        ALOGV("uriclass = 0");
        return;
    }

    JavaClass ucl(env, uriclass); // class pointer of Uri
    const jmethodID uts = env->GetMethodID(uriclass, "toString", "()Ljava/lang/String;");
    if (0 == uts) {
        ALOGV("uts = 0");
        return;
    }

    jstring juristr = (jstring)env->CallObjectMethod(uri.GetJObject(), uts);
    if (juristr != 0) {
        JavaString uristr(env, juristr);
        const char* uristr_ch = env->GetStringUTFChars(juristr, 0);
        ALOGV("AppInit - uristr_ch = `%s`", uristr_ch);
        intentURI = uristr_ch;
        env->ReleaseStringUTFChars(juristr, uristr_ch);
    }
}

ovrApplFrameOut ovrAppl::Frame(ovrApplFrameIn& frameIn) {
    assert(SessionObject != nullptr);

    // This is the only place frame index is incremented, right before getting the
    // predicted display time
    FrameIndex++;

    const ovrPosef trackingPose =
        vrapi_LocateTrackingSpace(SessionObject, vrapi_GetTrackingSpace(SessionObject));
    const ovrPosef eyeLevelTrackingPose =
        vrapi_LocateTrackingSpace(SessionObject, VRAPI_TRACKING_SPACE_LOCAL);

    frameIn.FrameIndex = FrameIndex;
    frameIn.PredictedDisplayTime = vrapi_GetPredictedDisplayTime(SessionObject, FrameIndex);
    frameIn.RealTimeInSeconds = vrapi_GetTimeInSeconds();

    Tracking = vrapi_GetPredictedTracking2(SessionObject, frameIn.PredictedDisplayTime);
    frameIn.HeadPose = Tracking.HeadPose.Pose;
    frameIn.Eye[0].ViewMatrix = Tracking.Eye[0].ViewMatrix;
    frameIn.Eye[1].ViewMatrix = Tracking.Eye[1].ViewMatrix;
    frameIn.Eye[0].ProjectionMatrix = Tracking.Eye[0].ProjectionMatrix;
    frameIn.Eye[1].ProjectionMatrix = Tracking.Eye[1].ProjectionMatrix;
    frameIn.EyeHeight = vrapi_GetEyeHeight(&eyeLevelTrackingPose, &trackingPose);
    frameIn.IPD = vrapi_GetInterpupillaryDistance(&Tracking);
    const ovrJava* java = reinterpret_cast<const ovrJava*>(Context->ContextForVrApi());
    frameIn.RecenterCount = vrapi_GetSystemStatusInt(java, VRAPI_SYS_STATUS_RECENTER_COUNT);

    static double LastPredictedDisplayTime = frameIn.PredictedDisplayTime;
    frameIn.DeltaSeconds = float(frameIn.PredictedDisplayTime - LastPredictedDisplayTime);
    LastPredictedDisplayTime = frameIn.PredictedDisplayTime;

    // save the display time for the renderer
    DisplayTime = frameIn.PredictedDisplayTime;

    // VrApi Events event queue must be processed with regular frequency.
    HandleVrApiEvents(frameIn);

    // copy input from the pending lists into the frame input
    {
        std::lock_guard<std::mutex> lock(KeyEventMutex);
        frameIn.KeyEvents = PendingKeyEvents;
        PendingKeyEvents.clear();
    }
    {
        std::lock_guard<std::mutex> lock(TouchEventMutex);
        frameIn.TouchEvents = PendingTouchEvents;
        PendingTouchEvents.clear();
    }

    // VR input
    HandleVRInputEvents(frameIn);

    ///  From https://developer.android.com/reference/android/view/KeyEvent.html#KEYCODE_VOLUME_UP
    static const int32_t KEYCODE_VOLUME_DOWN = 0x00000019;
    static const int32_t KEYCODE_VOLUME_UP = 0x00000018;
    static const int32_t ACTION_DOWN = 0x00000000;

    // Handle Volume events
    for (const ovrKeyEvent& event : frameIn.KeyEvents) {
        if (event.Action == ACTION_DOWN) {
            if (event.KeyCode == KEYCODE_VOLUME_UP) {
                ChangeVolume(+1);
            }
            if (event.KeyCode == KEYCODE_VOLUME_DOWN) {
                ChangeVolume(-1);
            }
        }
    }

#if 0
    // Hack to mono
    frameIn.Tracking.Eye[1] = frameIn.Tracking.Eye[0]; // All L
    // frameIn.Tracking.Eye[0] = frameIn.Tracking.Eye[1]; // All R
#endif

#if 0
    // Hack to flip
    auto eye1 = frameIn.Tracking.Eye[1];
    frameIn.Tracking.Eye[1] = frameIn.Tracking.Eye[0];
    frameIn.Tracking.Eye[0] = eye1;
#endif

    /// only update when focused or called out explicitly
    bool updateApp = IsAppFocused || RunWhilePaused;

    return updateApp ? AppFrame(frameIn) : ovrApplFrameOut();
}

void ovrAppl::RenderFrame(const ovrApplFrameIn& in) {
    ovrRendererOutput out = {};
    // default the Projection for each eye to whatever the input frame has,
    // but let the application override this
    out.FrameMatrices.EyeProjection[0] = in.Eye[0].ProjectionMatrix;
    out.FrameMatrices.EyeProjection[1] = in.Eye[1].ProjectionMatrix;

    AppRenderFrame(in, out);

    const ovrLayerHeader2* layerPtrs[ovrMaxLayerCount] = {};
    for (int i = 0; i < NumLayers; ++i) {
        layerPtrs[i] = &Layers[i].Header;
    }

    ovrSubmitFrameDescription2 frameDesc = {};
    frameDesc.Flags = FrameFlags;
    frameDesc.SwapInterval = 1;
    frameDesc.FrameIndex = GetFrameIndex();
    frameDesc.DisplayTime = GetDisplayTime();
    frameDesc.LayerCount = NumLayers;
    frameDesc.Layers = layerPtrs;

    vrapi_SubmitFrame2(GetSessionObject(), &frameDesc);
}

ovrApplFrameOut ovrAppl::AppFrame(const ovrApplFrameIn& /* in */) {
    ALOGV("Default AppFrame called!");
    return ovrApplFrameOut(false);
}

void ovrAppl::AppRenderFrame(const ovrApplFrameIn& in, ovrRendererOutput& out) {
    ALOGV("Default AppRenderFrame called!");
}

void ovrAppl::AppRenderEye(
    const ovrApplFrameIn& /* in */,
    ovrRendererOutput& /* out */,
    int /* eye */) {}

void ovrAppl::AppEyeGLStateSetup(
    const ovrApplFrameIn& /* in */,
    const ovrFramebuffer* fb,
    int /* eye */) {
    GL(glEnable(GL_SCISSOR_TEST));
    GL(glDepthMask(GL_TRUE));
    GL(glEnable(GL_DEPTH_TEST));
    GL(glDepthFunc(GL_LEQUAL));
    GL(glEnable(GL_CULL_FACE));
    GL(glViewport(0, 0, fb->Width, fb->Height));
    GL(glScissor(0, 0, fb->Width, fb->Height));
    GL(glClearColor(0.125f, 0.0f, 0.125f, 1.0f));
    GL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));

    // SampleFramework apps were originally written with the presumption that
    // its swapchains and compositor front buffer were RGB.
    // In order to have the colors the same now that its compositing
    // to an sRGB front buffer, we have to write to an sRGB swapchain
    // but with the linear->sRGB conversion disabled on write.
    GL(glDisable(GL_FRAMEBUFFER_SRGB_EXT));
}

void ovrAppl::DefaultRenderFrame_Loading(const ovrApplFrameIn& in, ovrRendererOutput& out) {
    NumLayers = 0;
    FrameFlags = 0u;
    // black layer
    ovrLayerProjection2 blackLayer = vrapi_DefaultLayerBlackProjection2();
    Layers[NumLayers++].Projection = blackLayer;
    // loading icon layer
    ovrLayerLoadingIcon2 iconLayer = vrapi_DefaultLayerLoadingIcon2();
    Layers[NumLayers++].LoadingIcon = iconLayer;
    FrameFlags |= VRAPI_FRAME_FLAG_FLUSH;
}

void ovrAppl::DefaultRenderFrame_Running(const ovrApplFrameIn& in, ovrRendererOutput& out) {
    NumLayers = 0;
    FrameFlags = 0u;
    // set up layers
    int& layerCount = NumLayers;
    layerCount = 0;
    ovrLayerProjection2& layer = Layers[layerCount].Projection;
    layer = vrapi_DefaultLayerProjection2();
    layer.HeadPose = Tracking.HeadPose;
    for (int eye = 0; eye < VRAPI_FRAME_LAYER_EYE_MAX; ++eye) {
        ovrFramebuffer* framebuffer = Framebuffer[NumFramebuffers == 1 ? 0 : eye].get();
        layer.Textures[eye].ColorSwapChain = framebuffer->ColorTextureSwapChain;
        layer.Textures[eye].SwapChainIndex = framebuffer->TextureSwapChainIndex;
        layer.Textures[eye].TexCoordsFromTanAngles = ovrMatrix4f_TanAngleMatrixFromProjection(
            (ovrMatrix4f*)&out.FrameMatrices.EyeProjection[eye]);
    }
    layer.Header.Flags |= VRAPI_FRAME_LAYER_FLAG_CHROMATIC_ABERRATION_CORRECTION;
    layerCount++;

    // render images for each eye
    for (int eye = 0; eye < NumFramebuffers; ++eye) {
        ovrFramebuffer* framebuffer = Framebuffer[eye].get();
        ovrFramebuffer_SetCurrent(framebuffer);

        AppEyeGLStateSetup(in, framebuffer, eye);
        AppRenderEye(in, out, eye);

        ovrFramebuffer_Resolve(framebuffer);
        ovrFramebuffer_Advance(framebuffer);
    }

    ovrFramebuffer_SetNone();
}

void ovrAppl::DefaultRenderFrame_Ending(const ovrApplFrameIn& in, ovrRendererOutput& out) {
    NumLayers = 0;
    FrameFlags = 0u;
    ovrLayerProjection2 layer = vrapi_DefaultLayerBlackProjection2();
    Layers[NumLayers++].Projection = layer;
    FrameFlags |= VRAPI_FRAME_FLAG_FLUSH | VRAPI_FRAME_FLAG_FINAL;
}

void ovrAppl::HandleVrApiEvents(ovrApplFrameIn& in) {
    ovrEventDataBuffer eventDataBuffer = {};

    // Poll for VrApi events
    for (;;) {
        ovrEventHeader* eventHeader = (ovrEventHeader*)(&eventDataBuffer);
        ovrResult res = vrapi_PollEvent(eventHeader);
        if (res != ovrSuccess) {
            break;
        }

        switch (eventHeader->EventType) {
            case VRAPI_EVENT_DATA_LOST:
                AppDataLost();
                break;
            case VRAPI_EVENT_VISIBILITY_GAINED:
                AppVisibilityGained();
                break;
            case VRAPI_EVENT_VISIBILITY_LOST:
                AppVisibilityLost();
                break;
            case VRAPI_EVENT_FOCUS_GAINED:
                // FOCUS_GAINED is sent when the application is in the foreground and has
                // input focus. This may be due to a system overlay relinquishing focus
                // back to the application.
                IsAppFocused = true;
                AppGainedFocus();
                break;
            case VRAPI_EVENT_FOCUS_LOST:
                // FOCUS_LOST is sent when the application is no longer in the foreground and
                // therefore does not have input focus. This may be due to a system overlay taking
                // focus from the application. The application should take appropriate action when
                // this occurs.
                IsAppFocused = false;
                AppLostFocus();
                break;
            default:
                // allow the app to handle the event
                AppHandleVrApiEvent(eventHeader);
                break;
        }
    }
}

void ovrAppl::AppLostFocus() {
    ALOGV("AppLostFocus: Received VRAPI_EVENT_FOCUS_LOST");
}

void ovrAppl::AppGainedFocus() {
    ALOGV("AppGainedFocus: Received VRAPI_EVENT_FOCUS_GAINED");
}

void ovrAppl::AppDataLost() {
    ALOGV("AppDataLost: Received VRAPI_EVENT_DATA_LOST");
}

void ovrAppl::AppVisibilityGained() {
    ALOGV("AppVisibilityGained: Received VRAPI_EVENT_VISIBILITY_GAINED");
}

void ovrAppl::AppVisibilityLost() {
    ALOGV("AppVisibilityLost: Received VRAPI_EVENT_VISIBILITY_LOST");
}

void ovrAppl::AppHandleVrApiEvent(const ovrEventHeader* event) {
    ALOGV("AppHandleVrApiEvent");
}

void ovrAppl::AddKeyEvent(const int32_t keyCode, const int32_t action) {
    // lock while we push back the event because we could be copying events into
    // the event buffer when the OS calls back the input handler
    std::lock_guard<std::mutex> lock(KeyEventMutex);
    PendingKeyEvents.push_back(ovrKeyEvent(keyCode, action, GetTimeInSeconds()));
}

void ovrAppl::AddTouchEvent(const int32_t action, const int32_t x, const int32_t y) {
    // lock while we push back the event because we could be copying events into
    // the event buffer when the OS calls back the input handler
    std::lock_guard<std::mutex> lock(TouchEventMutex);
    PendingTouchEvents.push_back(ovrTouchEvent(action, x, y, GetTimeInSeconds()));
}

void ovrAppl::HandleVRInputEvents(ovrApplFrameIn& in) {
    in.LeftRemoteTracked = false;
    in.RightRemoteTracked = false;
    in.AllButtons = 0u;
    in.AllTouches = 0u;

    const ovrJava* java = reinterpret_cast<const ovrJava*>(Context->ContextForVrApi());

    // Track mount status
    in.HeadsetIsMounted = (vrapi_GetSystemStatusInt(java, VRAPI_SYS_STATUS_MOUNTED) != VRAPI_FALSE);

    // Enumerate input devices
    for (ovrDeviceID deviceIndex = 0;; deviceIndex++) {
        ovrInputCapabilityHeader capsHeader;

        if (vrapi_EnumerateInputDevices(GetSessionObject(), deviceIndex, &capsHeader) < 0) {
            break; // no more devices
        }

        /// deviceID is not the same as deviceIndex!
        ovrDeviceID deviceID = capsHeader.DeviceID;
        if (deviceID == ovrDeviceIdType_Invalid) {
            ALOGV("HandleVRInputEvents - Invalid deviceID for deviceIndex=%u", deviceIndex);
            assert(deviceID != ovrDeviceIdType_Invalid);
            continue;
        }

        // Focus on remotes for now
        if (capsHeader.Type == ovrControllerType_TrackedRemote) {
            ovrInputTrackedRemoteCapabilities remoteCaps;
            remoteCaps.Header = capsHeader;
            if (ovrSuccess ==
                vrapi_GetInputDeviceCapabilities(GetSessionObject(), &remoteCaps.Header)) {
                bool isLeft = (remoteCaps.ControllerCapabilities & ovrControllerCaps_LeftHand) != 0;
                bool isRight =
                    (remoteCaps.ControllerCapabilities & ovrControllerCaps_RightHand) != 0;

                ovrInputStateTrackedRemote state;
                state.Header.ControllerType = ovrControllerType_TrackedRemote;

                if (ovrSuccess ==
                    vrapi_GetCurrentInputState(GetSessionObject(), deviceID, &state.Header)) {
                    if (isLeft) {
                        in.LeftRemoteTracked = true;
                        in.LeftRemoteJoystick = state.Joystick;
                        in.LeftRemoteIndexTrigger = state.IndexTrigger;
                        in.LeftRemoteGripTrigger = state.GripTrigger;
                        in.AllButtons |= state.Buttons;
                        in.AllTouches |= state.Touches;
                    }
                    if (isRight) {
                        in.RightRemoteTracked = true;
                        in.RightRemoteJoystick = state.Joystick;
                        in.RightRemoteIndexTrigger = state.IndexTrigger;
                        in.RightRemoteGripTrigger = state.GripTrigger;
                        in.AllButtons |= state.Buttons;
                        in.AllTouches |= state.Touches;
                    }
                }
            }
        }
    }

    // Delta from last frame
    in.LastFrameAllButtons = LastFrameAllButtons;
    in.LastFrameAllTouches = LastFrameAllTouches;
    in.LastFrameHeadsetIsMounted = LastFrameHeadsetIsMounted;
    LastFrameAllButtons = in.AllButtons;
    LastFrameAllTouches = in.AllTouches;
    LastFrameHeadsetIsMounted = in.HeadsetIsMounted;
}

void ovrAppl::Run(struct android_app* app) {
    ALOGV("----------------------------------------------------------------");
    ALOGV("android_main()");
    ALOGV("----------------------------------------------------------------");

    std::unique_ptr<OVRFW::ovrAndroidContext> ctx =
        std::unique_ptr<OVRFW::ovrAndroidContext>(new OVRFW::ovrAndroidContext());
    ctx->Init(app->activity->vm, app->activity->clazz, "main");

    if (!Init(ctx.get())) {
        exit(EXIT_FAILURE);
    }

    app->userData = this;
    app->onAppCmd = android_handle_cmd;
    app->onInputEvent = android_handle_input;

    bool exitApp = false;

    // main loop
    while (app->destroyRequested == 0) {
        for (;;) {
            int events;
            struct android_poll_source* source = nullptr;
            // if we are not in VR mode and not waiting on the app to end, wait indefinitely in the
            // poll (-1), otherwise, return immediately
            const int timeoutMilliseconds =
                (!HasActiveVRSession() && app->destroyRequested == 0) ? -1 : 0;
            if (ALooper_pollAll(timeoutMilliseconds, nullptr, &events, (void**)&source) < 0) {
                break;
            }
            if (source != nullptr) {
                source->process(app, source);
            }

            HandleLifecycle(ctx.get());
        }

        if (!HasActiveVRSession()) {
            continue;
        }

        OVRFW::ovrApplFrameIn frameIn;
        OVRFW::ovrApplFrameOut frameOut = Frame(frameIn);
        RenderFrame(frameIn);

        exitApp = frameOut.ExitApp;
    }

    Shutdown(ctx.get());
    ctx->Shutdown();

    ALOGV("----------------------------------------------------------------");
    ALOGV("android_main() DONE");
    ALOGV("----------------------------------------------------------------");
}

} // namespace OVRFW
