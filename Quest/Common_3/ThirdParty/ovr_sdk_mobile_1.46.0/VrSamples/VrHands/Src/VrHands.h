/************************************************************************************

Filename    :   VrHands.h
Content     :   Trivial use of the application framework.
Created     :   2/9/2017
Authors     :   Jonathan E. Wright

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

************************************************************************************/
#pragma once

#include <vector>
#include <string>
#include <memory>
#include <unordered_map>
#include <functional>

#include "VrApi_Input.h"

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
#include "Input/HandModel.h"
#include "Input/Skeleton.h"

namespace OVRFW {

class ovrLocale;
class ovrTextureAtlas;
class ovrParticleSystem;
class ovrBeamRenderer;
class ovrVrHands;

typedef std::vector<std::pair<ovrParticleSystem::handle_t, ovrBeamRenderer::handle_t>>
    jointHandles_t;

struct HandSampleConfigurationParameters {
   public:
    HandSampleConfigurationParameters()
        : HandScaleFactor(1.0f), ShowCapsules(false), ShowAxis(false) {}

    float HandScaleFactor;
    bool ShowCapsules;
    bool ShowAxis;
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

    ovrHandedness GetHand() const {
        return Hand;
    }
    inline bool IsLeftHand() const {
        return Hand == VRAPI_HAND_LEFT;
    }
    OVR::Posef GetHandPose() const {
        return HandPose;
    }
    OVR::Posef GetPointerPose() const {
        return PointerPose;
    }
    bool IsPinching() const {
        return ((InputStateHand.InputStateStatus & ovrInputStateHandStatus_IndexPinching) != 0);
    }
    bool IsPointerValid() const {
        return ((InputStateHand.InputStateStatus & ovrInputStateHandStatus_PointerValid) != 0);
    }
    bool IsInSystemGesture() const {
        return (
            (InputStateHand.InputStateStatus & ovrInputStateHandStatus_SystemGestureProcessing) !=
            0);
    }
    bool WasPinching() const {
        return PreviousFramePinch;
    }
    bool Clicked() const {
        return WasPinching() && !IsPinching();
    }

   protected:
    ovrHandedness Hand;
    ovrDrawSurface HandSurface;
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
    GlBuffer InstancedBoneUniformBuffer;
    menuHandle_t HitHandle;
    ovrSurfaceDef SurfaceDef;
    OVR::Vector3f GlowColor;
    OVR::Posef HandPose;
    OVR::Posef PointerPose;
    OVR::Posef HeadPose;
    bool PreviousFramePinch;
    bool IsActiveInputDevice;
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

    OVR::Matrix4f GetModelMatrix(const OVR::Posef& handPose) const;
    void InitFromSkeletonAndMesh(ovrVrHands& app, ovrHandSkeleton* skeleton, ovrHandMesh* mesh);
    void UpdateSkeleton(const OVR::Posef& handPose);
    bool Update(ovrMobile* ovr, const double displayTimeInSeconds, const float dt);
    void Render(std::vector<ovrDrawSurface>& surfaceList);

    ovrConfidence GetHandPoseConfidence() const {
        return RealHandPose.HandConfidence;
    }
    OVR::Vector3f GetRayOrigin() const {
        return PointerPose.Transform(OVR::Vector3f(0.0f, 0.0f, 0.0f));
    }
    OVR::Vector3f GetRayEnd() const {
        return PointerPose.Transform(OVR::Vector3f(0.0f, 0.0f, -1.5f));
    }

    const std::vector<ovrBoneCapsule>& GetBoneCapsules() const {
        return BoneCapsules;
    };

   private:
    ovrInputHandCapabilities Caps;
    ovrHandPose RealHandPose;
    /// Capsule rendering
    std::vector<ovrSurfaceDef> CapsuleSurfacesDef;
    std::vector<ovrDrawSurface> CapsuleSurfaces;
    std::vector<OVR::Matrix4f> CapsuleTransforms;
    std::vector<ovrBoneCapsule> BoneCapsules;

    /// Bone Axis rendering
    ovrSurfaceDef AxisSurfaceDef;
    ovrDrawSurface AxisSurface;
};

//==============================================================
// ovrVrHands
class ovrVrHands : public OVRFW::ovrAppl {
   public:
    ovrVrHands(
        const int32_t mainThreadTid,
        const int32_t renderThreadTid,
        const int cpuLevel,
        const int gpuLevel);

    virtual ~ovrVrHands();

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

    class OvrGuiSys& GetGuiSys() {
        return *GuiSys;
    }
    class ovrLocale& GetLocale() {
        return *Locale;
    }

   public:
    GlProgram ProgHandSkinned;
    GlProgram ProgHandCapsules;
    GlProgram ProgHandAxis;
    GlTexture HandsTexture;
    OVR::Vector3f SpecularLightDirection;
    OVR::Vector3f SpecularLightColor;
    OVR::Vector3f AmbientLightColor;
    OVR::Vector4f ChannelControl;

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

    VRMenuObject* TypeText;
    std::vector<ovrInputDeviceBase*> InputDevices;

    OVRFW::ovrSurfaceRender SurfaceRender;

    std::unordered_map<VRMenuObject*, std::function<void(void)>> ButtonHandlers;

   private:
    void ResetLaserPointer(ovrInputDeviceHandBase& trDevice);
    void ResetBones(jointHandles_t& handles);
    void RenderBones(
        const ovrApplFrameIn& frame,
        const OVR::Matrix4f& worldMatrix,
        const std::vector<ovrJoint>& joints,
        jointHandles_t& handles,
        const OVR::Vector4f& boneColor = OVR::Vector4f(0.0f, 0.0f, 1.0f, 1.0f),
        const float jointRadius = 0.008f,
        const float boneWidth = 0.005f);
    void RenderBoneCapsules(
        const ovrApplFrameIn& frame,
        const OVR::Matrix4f& worldMatrix,
        const std::vector<ovrJoint>& joints,
        const std::vector<ovrBoneCapsule>& capsules,
        jointHandles_t& handles);

    void EnumerateInputDevices();
    int FindInputDevice(const ovrDeviceID deviceID) const;
    void RemoveDevice(const ovrDeviceID deviceID);
    bool IsDeviceTracked(const ovrDeviceID deviceID) const;
    void OnDeviceConnected(const ovrInputCapabilityHeader& capsHeader);
    void OnDeviceDisconnected(ovrDeviceID const disconnectedDeviceID);

    void RenderRunningFrame(const OVRFW::ovrApplFrameIn& in, OVRFW::ovrRendererOutput& out);
};

} // namespace OVRFW
