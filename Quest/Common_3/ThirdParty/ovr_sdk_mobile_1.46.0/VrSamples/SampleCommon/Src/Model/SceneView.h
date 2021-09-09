/************************************************************************************

Filename    :   SceneView.h
Content     :   Basic viewing and movement in a scene.
Created     :   December 19, 2013
Authors     :   John Carmack

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

************************************************************************************/

#pragma once

#include "FrameParams.h"
#include "ModelFile.h"

namespace OVRFW {

//-----------------------------------------------------------------------------------
// ModelInScene
//
class ModelInScene {
   public:
    ModelInScene() : Definition(NULL) {}

    void SetModelFile(const ModelFile* mf);
    void AnimateJoints(const double timeInSeconds);

    ModelState State; // passed to rendering code
    const ModelFile* Definition; // will not be freed by OvrSceneView
};

//-----------------------------------------------------------------------------------
// OvrSceneView
//
class OvrSceneView {
   public:
    OvrSceneView();

    // The default view will be located at the origin, looking down the -Z axis,
    // with +X to the right and +Y up.
    // Increasing yaw looks to the left (rotation around Y axis).

    // loads the default GL shader programs
    ModelGlPrograms GetDefaultGLPrograms();

    // Blocking load of a scene from the filesystem.
    // This model will be freed when a new world model is set.
    void LoadWorldModel(const char* sceneFileName, const MaterialParms& materialParms);
    void LoadWorldModelFromApplicationPackage(
        const char* sceneFileName,
        const MaterialParms& materialParms);
    void
    LoadWorldModel(class ovrFileSys& fileSys, const char* uri, const MaterialParms& materialParms);

    // Set an already loaded scene, which will not be freed when a new
    // world model is set.
    void SetWorldModel(ModelFile& model);
    ModelInScene* GetWorldModel() {
        return &WorldModel;
    }

    // Passed on to world model
    ovrSurfaceDef* FindNamedSurface(const char* name) const;
    const ModelTexture* FindNamedTexture(const char* name) const;
    const ModelTag* FindNamedTag(const char* name) const;
    OVR::Bounds3f GetBounds() const;

    // Returns the new modelIndex
    int AddModel(ModelInScene* model);
    void RemoveModelIndex(int index);

    void PauseAnimations(bool pauseAnimations) {
        Paused = pauseAnimations;
    }

    // Allow movement inside the scene based on the joypad.
    // Models that have DontRenderForClientUid == suppressModelsWithClientId will be skipped
    // to prevent the client's own head model from drawing in their view.
    void Frame(const ovrApplFrameIn& vrFrame, const long long suppressModelsWithClientId = -1);

    // Populate frameMatrices with the view and projection matrices for the scene.
    void GetFrameMatrices(
        const float fovDegreesX,
        const float fovDegreesY,
        FrameMatrices& frameMatrices) const;
    // Generates a sorted surface list for the scene (including emit surfaces).
    void GenerateFrameSurfaceList(
        const FrameMatrices& matrices,
        std::vector<ovrDrawSurface>& surfaceList) const;

    // Systems that want to manage individual surfaces instead of complete models
    // can add surfaces to this list during Frame().  They will be drawn for
    // both eyes, then the list will be cleared.
    std::vector<ovrDrawSurface>& GetEmitList() {
        return EmitSurfaces;
    }

    float GetEyeYaw() const {
        return EyeYaw;
    }
    float GetEyePitch() const {
        return EyePitch;
    }
    float GetEyeRoll() const {
        return EyeRoll;
    }

    float GetYawOffset() const {
        return SceneYaw;
    }
    void SetYawOffset(const float yaw) {
        EyeYaw += (yaw - SceneYaw);
        SceneYaw = yaw;
    }

    float GetZnear() const {
        return Znear;
    }
    void SetZnear(float z) {
        Znear = z;
    }

    void SetMoveSpeed(const float speed) {
        MoveSpeed = speed;
    }
    float GetMoveSpeed() const {
        return MoveSpeed;
    }

    void SetFreeMove(const bool allowFreeMovement) {
        FreeMove = allowFreeMovement;
    }

    // Derived from state after last Frame()
    const OVR::Vector3f& GetFootPos() const {
        return FootPos;
    }
    void SetFootPos(const OVR::Vector3f& pos, bool updateCenterEye = true);

    OVR::Vector3f GetNeutralHeadCenter() const; // FootPos + EyeHeight
    OVR::Vector3f GetCenterEyePosition() const;
    OVR::Vector3f GetCenterEyeForward() const;
    OVR::Matrix4f GetCenterEyeTransform() const;
    OVR::Matrix4f GetCenterEyeViewMatrix() const;

    OVR::Matrix4f GetEyeViewMatrix(const int eye) const;
    OVR::Matrix4f
    GetEyeProjectionMatrix(const int eye, const float fovDegreesX, const float fovDegreesY) const;
    OVR::Matrix4f GetEyeViewProjectionMatrix(
        const int eye,
        const float fovDegreesX,
        const float fovDegreesY) const;

    float GetEyeHeight() const;

    // When head tracking is reset, any joystick offsets should be cleared
    // so the viewer is looking ehere the application wants.
    void ClearStickAngles();

    void UpdateCenterEye();

    // Mod stick turning by this to help with sickness. If <= 0 then ignored
    void SetYawMod(const float yawMod) {
        YawMod = yawMod;
    }

   private:
    void LoadWorldModel(
        const char* sceneFileName,
        const MaterialParms& materialParms,
        const bool fromApk);

    // The only ModelInScene that OvrSceneView actually owns.
    bool FreeWorldModelOnChange;
    ModelInScene WorldModel;

    // Entries can be NULL.
    // None of these will be directly freed by OvrSceneView.
    std::vector<ModelInScene*> Models;

    // Externally generated surfaces
    std::vector<ovrDrawSurface> EmitSurfaces;

    GlProgram ProgVertexColor;
    GlProgram ProgSingleTexture;
    GlProgram ProgLightMapped;
    GlProgram ProgReflectionMapped;
    GlProgram ProgSimplePBR;
    GlProgram ProgBaseColorPBR;
    GlProgram ProgBaseColorEmissivePBR;
    GlProgram ProgSkinnedVertexColor;
    GlProgram ProgSkinnedSingleTexture;
    GlProgram ProgSkinnedLightMapped;
    GlProgram ProgSkinnedReflectionMapped;
    GlProgram ProgSkinnedSimplePBR;
    GlProgram ProgSkinnedBaseColorPBR;
    GlProgram ProgSkinnedBaseColorEmissivePBR;
    bool LoadedPrograms;

    ModelGlPrograms GlPrograms;

    // Don't animate if true.
    bool Paused;

    // Updated each Frame()
    long long SuppressModelsWithClientId;

    float EyeHeight;
    float InterPupillaryDistance;

    float Znear;

    // Angle offsets in radians for joystick movement, which is
    // the moral equivalent of head tracking.  Reset head tracking
    // should also clear these.
    float StickYaw; // added on top of the sensor reading
    float StickPitch; // only applied if the tracking sensor isn't active

    // An application can turn the primary view direction, which is where
    // the view will go if head tracking is reset.  Should only be changed
    // at discrete transition points to avoid sickness.
    float SceneYaw;

    // Applied one frame later to avoid bounce-back from async time warp yaw velocity prediction.
    float YawVelocity;

    // 3.0 m/s by default.  Different apps may want different move speeds
    float MoveSpeed;

    // Allows vertical movement when holding right shoulder button
    bool FreeMove;

    // Modified by joypad movement and collision detection
    OVR::Vector3f FootPos;

    // Calculated in Frame()
    OVR::Matrix4f CenterEyeTransform;
    OVR::Matrix4f CenterEyeViewMatrix;
    float EyeYaw; // Rotation around Y, CCW positive when looking at RHS (X,Z) plane.
    float EyePitch; // Pitch. If sensor is plugged in, only read from sensor.
    float EyeRoll; // Roll, only read from sensor.
    ovrApplFrameIn CurrentTracking;

    float YawMod;
};

// It probably isn't worth keeping these shared here, each user
// should just duplicate them.
extern const char* VertexColorVertexShaderSrc;
extern const char* VertexColorSkinned1VertexShaderSrc;
extern const char* VertexColorFragmentShaderSrc;

extern const char* SingleTextureVertexShaderSrc;
extern const char* SingleTextureSkinned1VertexShaderSrc;
extern const char* SingleTextureFragmentShaderSrc;

extern const char* LightMappedVertexShaderSrc;
extern const char* LightMappedSkinned1VertexShaderSrc;
extern const char* LightMappedFragmentShaderSrc;

extern const char* ReflectionMappedVertexShaderSrc;
extern const char* ReflectionMappedSkinned1VertexShaderSrc;
extern const char* ReflectionMappedFragmentShaderSrc;

} // namespace OVRFW
