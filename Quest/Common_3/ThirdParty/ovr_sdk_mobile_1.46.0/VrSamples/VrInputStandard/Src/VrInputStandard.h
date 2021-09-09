/************************************************************************************

Filename    :   VrInputStandard.h
Content     :   Trivial use of standard input
Created     :   2/9/2017
Authors     :   Lewis Weaver

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

************************************************************************************/
#pragma once

#include <vector>
#include <string>
#include <memory>
#include <unordered_map>
#include <functional>

#include "Appl.h"
#include "OVR_FileSys.h"

#include "Model/SceneView.h"
#include "Render/SurfaceRender.h"
#include "Render/DebugLines.h"
#include "Render/TextureAtlas.h"
#include "Render/BeamRenderer.h"
#include "Render/ParticleSystem.h"
#include "Render/PanelRenderer.h"
#include "GUI/GuiSys.h"

#include "VrApi_Input.h"

#include "Input/HandModel.h"
#include "Input/Skeleton.h"

namespace OVRFW {

class ovrLocale;
class ovrTextureAtlas;
class ovrParticleSystem;
class ovrBeamRenderer;
class ovrVrInputStandard;

enum HapticStates {
    HAPTICS_NONE = 0,
    HAPTICS_BUFFERED = 1,
    HAPTICS_SIMPLE = 2,
    HAPTICS_SIMPLE_CLICKED = 3
};

typedef std::vector<std::pair<ovrParticleSystem::handle_t, ovrBeamRenderer::handle_t>>
    jointHandles_t;

struct HandSampleConfigurationParameters {
   public:
    HandSampleConfigurationParameters() : RenderAxis(false), EnableStandardDevices(false) {}

    bool RenderAxis; // render 3D axis on hands, etc
    bool EnableStandardDevices; // Render and handle input events from ovrInputDeviceStandardPointer
                                // instead of per device type.
    HapticStates OnTriggerHapticsState;
};

struct DeviceHapticState {
    HapticStates HapticState = HapticStates::HAPTICS_NONE;
    float HapticSimpleValue = 0.0f;
};

//==============================================================
// ovrInputDeviceBase
// Abstract base class for generically tracking controllers of different types.
class ovrInputDeviceBase {
   public:
    ovrInputDeviceBase() = default;
    virtual ~ovrInputDeviceBase() = default;
    virtual const ovrInputCapabilityHeader* GetCaps() const = 0;
    virtual ovrControllerType GetType() const = 0;
    virtual ovrDeviceID GetDeviceID() const = 0;
    virtual const char* GetName() const = 0;
};

//==============================================================
// ovrInputDeviceHandBase
class ovrInputDeviceHandBase : public ovrInputDeviceBase {
   public:
    ovrInputDeviceHandBase(ovrHandedness hand)
        : ovrInputDeviceBase(),
          Hand(hand),
          TransformMatrices(MAX_JOINTS, OVR::Matrix4f::Identity()),
          BindMatrices(MAX_JOINTS, OVR::Matrix4f::Identity()),
          SkinMatrices(MAX_JOINTS, OVR::Matrix4f::Identity()),
          GlowColor(1.0f, 1.0f, 1.0f),
          PreviousFramePinch(false),
          IsActiveInputDevice(false) {}

    virtual ~ovrInputDeviceHandBase() {}

    ovrHandedness GetHand() const {
        return Hand;
    }
    virtual OVR::Matrix4f GetModelMatrix(const OVR::Posef& handPose) const {
        return OVR::Matrix4f(handPose);
    }
    virtual OVR::Matrix4f GetPointerMatrix() const {
        return OVR::Matrix4f(PointerPose);
    }
    inline bool IsLeftHand() const {
        return Hand == VRAPI_HAND_LEFT;
    }

    const menuHandle_t& GetLastHitHandle() const {
        return HitHandle;
    }
    void SetLastHitHandle(const menuHandle_t& lastHitHandle) {
        HitHandle = lastHitHandle;
    }

    ovrHandModel& GetHandModel() {
        return HandModel;
    }
    jointHandles_t& GetFingerJointHandles() {
        return FingerJointHandles;
    }
    ovrBeamRenderer::handle_t& GetLaserPointerBeamHandle() {
        return LaserPointerBeamHandle;
    }
    ovrParticleSystem::handle_t& GetLaserPointerParticleHandle() {
        return LaserPointerParticleHandle;
    }

    bool GetIsActiveInputDevice() const {
        return IsActiveInputDevice;
    }
    void SetIsActiveInputDevice(bool isActive) {
        IsActiveInputDevice = isActive;
    }

    virtual void
    InitFromSkeletonAndMesh(ovrVrInputStandard& app, ovrHandSkeleton* skeleton, ovrHandMesh* mesh);
    void UpdateSkeleton(const OVR::Posef& handPose);
    virtual bool Update(ovrMobile* ovr, const double displayTimeInSeconds, const float dt);
    virtual void Render(std::vector<ovrDrawSurface>& surfaceList) {
        for (auto& surface : Surfaces) {
            if (surface.surface != nullptr) {
                surfaceList.push_back(surface);
            }
        }
    }
    virtual void ResetHaptics(ovrMobile* ovr, float displayTimeInSeconds){};
    void UpdateHaptics(ovrMobile* ovr, float dt);
    virtual bool HasCapSimpleHaptics() const {
        return false;
    }
    virtual bool HasCapBufferedHaptics() const {
        return false;
    }
    virtual float GetHapticSampleDurationMS() const {
        return 0.0f;
    }
    virtual uint32_t GetHapticSamplesMax() const {
        return 0;
    }

    virtual OVR::Posef GetHandPose() const {
        return HandPose;
    }
    virtual OVR::Posef GetPointerPose() const {
        return PointerPose;
    }
    virtual bool IsPinching() const {
        return ((InputStateHand.InputStateStatus & ovrInputStateHandStatus_IndexPinching) != 0);
    }
    virtual bool IsPointerValid() const {
        return ((InputStateHand.InputStateStatus & ovrInputStateHandStatus_PointerValid) != 0);
    }
    virtual bool WasPinching() const {
        return PreviousFramePinch;
    }
    virtual bool Clicked() const {
        return WasPinching() && !IsPinching();
    }
    virtual bool IsInSystemGesture() const {
        return false;
    }

    virtual OVR::Vector3f GetRayOrigin() const {
        return IsLeftHand() ? OVR::Vector3f(0.04f, -0.05f, -0.1f)
                            : OVR::Vector3f(-0.04f, -0.05f, -0.1f);
    }

    virtual OVR::Vector3f GetRayEnd() const {
        return PointerPose.Transform(OVR::Vector3f(0.0f, 0.0f, -1.5f));
    }

    virtual ovrConfidence GetHandPoseConfidence() const {
        return ovrConfidence_HIGH;
    }
    virtual bool IsMenuPressed() const {
        return false;
    }
    virtual bool WasMenuPressed() const {
        return PreviousFrameMenu;
    }
    virtual bool MenuClicked() const {
        return WasMenuPressed() && !IsMenuPressed();
    }
    virtual const DeviceHapticState& GetRequestedHapticsState() {
        return PreviousHapticState;
    }

   protected:
    ovrHandedness Hand;
    std::vector<ovrDrawSurface> Surfaces;
    ovrHandModel HandModel;
    ovrInputStateHand InputStateHand;
    jointHandles_t FingerJointHandles;
    ovrBeamRenderer::handle_t LaserPointerBeamHandle;
    ovrParticleSystem::handle_t LaserPointerParticleHandle;
    ovrTracking Tracking;
    std::vector<OVR::Matrix4f> TransformMatrices;
    std::vector<OVR::Matrix4f> BindMatrices;
    std::vector<OVR::Matrix4f> SkinMatrices;
    GlBuffer SkinUniformBuffer;
    menuHandle_t HitHandle;
    ovrSurfaceDef SurfaceDef;
    OVR::Vector3f GlowColor;
    OVR::Posef HandPose;
    OVR::Posef PointerPose;
    OVR::Posef HeadPose;
    bool PreviousFramePinch;
    bool PreviousFrameMenu;
    bool IsActiveInputDevice;

    // State of Haptics after a call to UpdateHaptics
    DeviceHapticState PreviousHapticState;
};

//==============================================================
// ovrInputDeviceTrackedRemoteHand
class ovrInputDeviceTrackedRemoteHand : public ovrInputDeviceHandBase {
   public:
    ovrInputDeviceTrackedRemoteHand(
        const ovrInputTrackedRemoteCapabilities& caps,
        ovrHandedness hand)
        : ovrInputDeviceHandBase(hand),
          Caps(caps),
          ControllerModel(nullptr),
          IsPinchingInternal(false) {
        GlowColor = OVR::Vector3f(0.0f, 0.0f, 1.0f);
    }

    virtual ~ovrInputDeviceTrackedRemoteHand() {}

    static ovrInputDeviceTrackedRemoteHand* Create(
        OVRFW::ovrAppl& app,
        const ovrInputTrackedRemoteCapabilities& capsHeader);

    virtual const ovrInputCapabilityHeader* GetCaps() const override {
        return &Caps.Header;
    }
    virtual ovrControllerType GetType() const override {
        return Caps.Header.Type;
    }
    virtual ovrDeviceID GetDeviceID() const override {
        return Caps.Header.DeviceID;
    }
    virtual const char* GetName() const override {
        return "TrackedRemoteHand";
    }
    const ovrInputTrackedRemoteCapabilities& GetTrackedRemoteCaps() const {
        return Caps;
    }

    virtual OVR::Matrix4f GetModelMatrix(const OVR::Posef& handPose) const override;

    virtual bool Update(ovrMobile* ovr, const double displayTimeInSeconds, const float dt) override;
    virtual void Render(std::vector<ovrDrawSurface>& surfaceList) override;

    virtual bool HasCapSimpleHaptics() const override {
        return GetTrackedRemoteCaps().ControllerCapabilities &
            ovrControllerCaps_HasSimpleHapticVibration;
    }
    virtual bool HasCapBufferedHaptics() const override {
        return GetTrackedRemoteCaps().ControllerCapabilities &
            ovrControllerCaps_HasBufferedHapticVibration;
    }
    virtual float GetHapticSampleDurationMS() const override {
        return GetTrackedRemoteCaps().HapticSampleDurationMS;
    }
    virtual uint32_t GetHapticSamplesMax() const override {
        return GetTrackedRemoteCaps().HapticSamplesMax;
    }

    virtual OVR::Vector3f GetRayOrigin() const override {
        return PointerPose.Transform(OVR::Vector3(0.0f, 0.0f, -0.055f));
    }
    virtual OVR::Matrix4f GetPointerMatrix() const override;

    virtual bool IsPinching() const override {
        return IsPinchingInternal;
    }
    virtual bool IsPointerValid() const override {
        return true;
    }
    virtual bool IsMenuPressed() const override {
        return !PreviousFrameMenuPressed && IsMenuPressedInternal;
    }
    virtual const DeviceHapticState& GetRequestedHapticsState() override {
        return RequestedHapticState;
    }

    void SetControllerModel(ModelFile* m);
    void ResetHaptics(ovrMobile* ovr, float displayTimeInSeconds) override;

   private:
    void UpdateHapticRequestedState(const ovrInputStateTrackedRemote& remoteInputState);

    DeviceHapticState RequestedHapticState;
    ovrInputTrackedRemoteCapabilities Caps;
    ModelFile* ControllerModel;
    bool IsPinchingInternal;
    bool PreviousFrameMenuPressed;
    bool IsMenuPressedInternal;
};

//==============================================================
// ovrInputDeviceTrackedHand
class ovrInputDeviceTrackedHand : public ovrInputDeviceHandBase {
   public:
    ovrInputDeviceTrackedHand(const ovrInputHandCapabilities& caps, ovrHandedness hand)
        : ovrInputDeviceHandBase(hand), Caps(caps) {
        GlowColor = OVR::Vector3f(1.0f, 0.0f, 0.0f);
    }

    virtual ~ovrInputDeviceTrackedHand() {}

    static ovrInputDeviceTrackedHand* Create(
        OVRFW::ovrAppl& app,
        const ovrInputHandCapabilities& capsHeader);

    virtual const ovrInputCapabilityHeader* GetCaps() const override {
        return &Caps.Header;
    }
    virtual ovrControllerType GetType() const override {
        return Caps.Header.Type;
    }
    virtual ovrDeviceID GetDeviceID() const override {
        return Caps.Header.DeviceID;
    }
    virtual const char* GetName() const override {
        return "TrackedHand";
    }
    const ovrInputHandCapabilities& GetHandCaps() const {
        return Caps;
    }

    virtual bool Update(ovrMobile* ovr, const double displayTimeInSeconds, const float dt) override;
    virtual void Render(std::vector<ovrDrawSurface>& surfaceList) override;

    virtual OVR::Vector3f GetRayOrigin() const override {
        return PointerPose.Transform(OVR::Vector3f(0.0f));
    }

    virtual void InitFromSkeletonAndMesh(
        ovrVrInputStandard& app,
        ovrHandSkeleton* skeleton,
        ovrHandMesh* mesh) override;

    virtual ovrConfidence GetHandPoseConfidence() const override {
        return RealHandPose.HandConfidence;
    }
    virtual bool IsInSystemGesture() const override {
        return (
            (InputStateHand.InputStateStatus & ovrInputStateHandStatus_SystemGestureProcessing) !=
            0);
    }
    virtual bool IsMenuPressed() const override {
        return (InputStateHand.InputStateStatus & ovrInputStateHandStatus_MenuPressed) != 0;
    }

   private:
    OVR::Vector3f GetBonePosition(ovrHandBone bone) const;
    ovrInputHandCapabilities Caps;
    ovrHandPose RealHandPose;
    OVR::Vector3f PinchOffset;
};

//==============================================================
// ovrInputDeviceStandardPointer
class ovrInputDeviceStandardPointer : public ovrInputDeviceHandBase {
   public:
    ovrInputDeviceStandardPointer(
        const ovrInputStandardPointerCapabilities& caps,
        ovrHandedness hand)
        : ovrInputDeviceHandBase(hand), Caps(caps) {}

    virtual ~ovrInputDeviceStandardPointer() {}

    static ovrInputDeviceStandardPointer* Create(
        OVRFW::ovrAppl& app,
        const ovrInputStandardPointerCapabilities& capsHeader);

    virtual const ovrInputCapabilityHeader* GetCaps() const override {
        return &Caps.Header;
    }
    virtual ovrControllerType GetType() const override {
        return Caps.Header.Type;
    }
    virtual ovrDeviceID GetDeviceID() const override {
        return Caps.Header.DeviceID;
    }
    virtual const char* GetName() const override {
        return "StandardPointerDevice";
    }
    const ovrInputStandardPointerCapabilities& GetStandardDeviceCaps() const {
        return Caps;
    }

    bool Update(ovrMobile* ovr, const double displayTimeInSeconds, const float dt) override;

    virtual bool HasCapSimpleHaptics() const override {
        return GetStandardDeviceCaps().ControllerCapabilities &
            ovrControllerCaps_HasSimpleHapticVibration;
    }
    virtual bool HasCapBufferedHaptics() const override {
        return GetStandardDeviceCaps().ControllerCapabilities &
            ovrControllerCaps_HasBufferedHapticVibration;
    }
    virtual float GetHapticSampleDurationMS() const override {
        return GetStandardDeviceCaps().HapticSampleDurationMS;
    }
    virtual uint32_t GetHapticSamplesMax() const override {
        return GetStandardDeviceCaps().HapticSamplesMax;
    }

    bool IsPinching() const override {
        return InputStateStandardPointer.PointerStrength > 0.99f;
    }
    bool IsPointerValid() const override {
        return (InputStateStandardPointer.InputStateStatus &
                ovrInputStateStandardPointerStatus_PointerValid) > 0;
    }
    bool IsMenuPressed() const override {
        return (InputStateStandardPointer.InputStateStatus &
                ovrInputStateStandardPointerStatus_MenuPressed) > 0;
    }
    OVR::Vector3f GetRayOrigin() const override {
        return PointerPose.Transform(OVR::Vector3f(0.0f));
    }
    const DeviceHapticState& GetRequestedHapticsState() override {
        return RequestedHapticState;
    }
    void ResetHaptics(ovrMobile* ovr, float displayTimeInSeconds) override;

   protected:
    void UpdateHapticRequestedState(const ovrInputStateStandardPointer& inputState);

    DeviceHapticState RequestedHapticState;
    ovrInputStandardPointerCapabilities Caps;
    ovrInputStateStandardPointer InputStateStandardPointer;
};

//==============================================================
// ovrVrInputStandard
class ovrVrInputStandard : public OVRFW::ovrAppl {
   public:
    ovrVrInputStandard(
        const int32_t mainThreadTid,
        const int32_t renderThreadTid,
        const int cpuLevel,
        const int gpuLevel);

    virtual ~ovrVrInputStandard();

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

    void SubmitCompositorLayers(const ovrApplFrameIn& in, ovrRendererOutput& out);

    class OvrGuiSys& GetGuiSys() {
        return *GuiSys;
    }
    class ovrLocale& GetLocale() {
        return *Locale;
    }

   public:
    GlProgram ProgOculusTouch;
    GlProgram ProgHandSkinned;
    GlProgram ProgHandCapsules;
    OVR::Vector3f SpecularLightDirection;
    OVR::Vector3f SpecularLightColor;
    OVR::Vector3f AmbientLightColor;

   private:
    ovrRenderState RenderState;
    ovrFileSys* FileSys;
    OvrDebugLines* DebugLines;
    OvrGuiSys::SoundEffectPlayer* SoundEffectPlayer;
    OvrGuiSys* GuiSys;
    ovrLocale* Locale;

    ModelFile* SceneModel;
    OvrSceneView Scene;

    ovrTextureAtlas* SpriteAtlas;
    ovrParticleSystem* ParticleSystem;
    ovrTextureAtlas* BeamAtlas;
    ovrBeamRenderer* BeamRenderer;

    ModelFile* ControllerModelOculusTouchLeft;
    ModelFile* ControllerModelOculusTouchRight;

    /// Axis rendering
    ovrSurfaceDef AxisSurfaceDef;
    ovrDrawSurface AxisSurface;
    std::vector<OVR::Matrix4f> TransformMatrices;
    std::vector<ovrInputDeviceHandBase*> EnabledInputDevices;
    GlBuffer AxisUniformBuffer;
    GlProgram ProgAxis;

    // because a single Gear VR controller can be a left or right controller dependent on the
    // user's handedness (dominant hand) setting, we can't simply track controllers using a left
    // or right hand slot look up, because on any frame a Gear VR controller could change from
    // left handed to right handed and switch slots.
    std::vector<ovrInputDeviceBase*> InputDevices;

    OVRFW::ovrSurfaceRender SurfaceRender;

    std::unordered_map<VRMenuObject*, std::function<void(void)>> ButtonHandlers;
    int MenuPressCount = 0;

   private:
    void ResetLaserPointer(ovrInputDeviceHandBase& trDevice);

    void EnumerateInputDevices();
    int FindInputDevice(const ovrDeviceID deviceID) const;
    void RemoveDevice(const ovrDeviceID deviceID);
    bool IsDeviceTracked(const ovrDeviceID deviceID) const;
    void OnDeviceConnected(const ovrInputCapabilityHeader& capsHeader);
    void OnDeviceDisconnected(ovrDeviceID const disconnectedDeviceID);

    bool IsDeviceTypeEnabled(const ovrInputDeviceBase& device) const;
    void RenderRunningFrame(const OVRFW::ovrApplFrameIn& in, OVRFW::ovrRendererOutput& out);
};

} // namespace OVRFW
