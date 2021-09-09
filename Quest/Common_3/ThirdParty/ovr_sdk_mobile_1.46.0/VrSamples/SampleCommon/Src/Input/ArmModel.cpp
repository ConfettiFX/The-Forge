/************************************************************************************

Filename    :   ArmModel.cpp
Content     :   An arm model for the tracked remote
Created     :   2/20/2017
Authors     :   Jonathan E. Wright

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

************************************************************************************/

#include "ArmModel.h"

#include <algorithm>

using OVR::Matrix4f;
using OVR::Posef;
using OVR::Quatf;
using OVR::Vector3f;
using OVR::Vector4f;

namespace OVRFW {

ovrArmModel::ovrArmModel()
    : TorsoYaw(0.0f),
      TorsoTracksHead(false),
      ForceRecenter(false),
      ClavicleJointIdx(-1),
      ShoulderJointIdx(-1),
      ElbowJointIdx(-1),
      WristJointIdx(-1) {}

void ovrArmModel::InitSkeleton(bool isLeft) {
    std::vector<ovrJoint> joints;

    ///
    const int noParent = -1;
    const int headJointIdx = 0;

    /// Set the joints needed to update the model
    ClavicleJointIdx = 1;
    ShoulderJointIdx = 2;
    ElbowJointIdx = 3;
    WristJointIdx = 4;

    joints.push_back(ovrJoint("head", Vector4f(0.0f, 1.0f, 0.0f, 1.0f), Posef(), noParent));
    joints.push_back(ovrJoint(
        "neck",
        Vector4f(0.0f, 1.0f, 0.0f, 1.0f),
        Posef(Quatf(), Vector3f(0.0f, -0.2032f, 0.0f)),
        headJointIdx));
    joints.push_back(ovrJoint(
        "clavicle",
        Vector4f(0.0f, 1.0f, 0.0f, 1.0f),
        Posef(Quatf(), Vector3f((isLeft ? -0.2286f : 0.2286f), 0.0f, 0.0f)),
        ClavicleJointIdx));
    joints.push_back(ovrJoint(
        "shoulder",
        Vector4f(1.0f, 0.0f, 1.0f, 1.0f),
        Posef(Quatf(), Vector3f(0.0f, -0.2441f, 0.02134f)),
        ShoulderJointIdx));
    joints.push_back(ovrJoint(
        "elbow",
        Vector4f(1.0f, 0.0f, 0.0f, 1.0f),
        Posef(Quatf(), Vector3f(0.0f, 0.0f, -0.3048f)),
        3));
    joints.push_back(ovrJoint(
        "wrist",
        Vector4f(1.0f, 1.0f, 0.0f, 1.0f),
        Posef(Quatf(), Vector3f(0.0f, 0.0f, -0.0762f)),
        ElbowJointIdx));
    joints.push_back(ovrJoint(
        "hand",
        Vector4f(1.0f, 1.0f, 0.0f, 1.0f),
        Posef(Quatf(), Vector3f(0.0f, 0.0381f, -0.0381f)),
        WristJointIdx));

    Skeleton.SetJoints(joints);
    TransformedJoints = Skeleton.GetJoints();
}

void ovrArmModel::Update(
    const Posef& headPose,
    const Posef& remotePose,
    const ovrHandedness handedness,
    const bool recenteredController,
    Posef& outPose) {
    Matrix4f eyeMatrix(headPose);

    float eyeYaw;
    float eyePitch;
    float eyeRoll; // ya... like, seriously???
    eyeMatrix.ToEulerAngles<OVR::Axis_Y, OVR::Axis_X, OVR::Axis_Z, OVR::Rotate_CCW, OVR::Handed_R>(
        &eyeYaw, &eyePitch, &eyeRoll);

    auto ConstrainTorsoYaw = [](const Quatf& headRot, const float torsoYaw) {
        const Vector3f worldUp(0.0f, 1.0f, 0.0f);
        const Vector3f worldFwd(0.0f, 0.0f, -1.0f);

        const Vector3f projectedHeadFwd = (headRot * worldFwd).ProjectToPlane(worldUp);
        if (projectedHeadFwd.LengthSq() < 0.001f) {
            return torsoYaw;
        }

        // calculate the world rotation of the head on the horizon plane
        const Vector3f headFwd = projectedHeadFwd.Normalized();

        // calculate the world rotation of the torso
        const Quatf torsoRot(worldUp, torsoYaw);
        const Vector3f torsoFwd = torsoRot * worldFwd;

        // find the angle between the torso and head
        const float torsoMaxYawOffset = MATH_FLOAT_DEGREETORADFACTOR * 30.0f;
        const float torsoDot = torsoFwd.Dot(headFwd);
        if (torsoDot >= cosf(torsoMaxYawOffset)) {
            return torsoYaw;
        }

        // calculate the rotation of the torso when it's constrained in that direction
        const Vector3f headRight(-headFwd.z, 0.0f, headFwd.x);
        const Quatf projectedHeadRot = Quatf::FromBasisVectors(headFwd, headRight, worldUp);

        const float offsetDir = headRight.Dot(torsoFwd) < 0.0f ? 1.0f : -1.0f;
        const float offsetYaw = torsoMaxYawOffset * offsetDir;
        const Quatf constrainedTorsoRot = projectedHeadRot * Quatf(worldUp, offsetYaw);

        // slerp torso towards the constrained rotation
        float const slerpFactor = 1.0f / 15.0f;
        const Quatf slerped = torsoRot.Slerp(constrainedTorsoRot, slerpFactor);

        float y;
        float p;
        float r;
        slerped.GetYawPitchRoll(&y, &p, &r);
        return y;
    };

    TorsoYaw = ConstrainTorsoYaw(headPose.Rotation, TorsoYaw);

    if (ForceRecenter || TorsoTracksHead || recenteredController) {
        ForceRecenter = false;
        TorsoYaw = eyeYaw;
    }

    FootPose = Posef(Quatf(Vector3f(0.0f, 1.0f, 0.0f), TorsoYaw), eyeMatrix.GetTranslation());
    Quatf remoteRot(remotePose.Rotation);

    const float MAX_ROLL = MATH_FLOAT_PIOVER2;
    const float MIN_PITCH = MATH_FLOAT_PIOVER2 * 0.825f;
    float remoteYaw;
    float remotePitch;
    float remoteRoll;
    remoteRot.GetYawPitchRoll(&remoteYaw, &remotePitch, &remoteRoll);
    if ((remoteRoll >= -MAX_ROLL && remoteRoll <= MAX_ROLL) ||
        (remotePitch <= -MIN_PITCH || remotePitch >= MIN_PITCH)) {
        LastUnclampedRoll = remoteRoll;
    } else {
        remoteRoll = LastUnclampedRoll;
    }

    Matrix4f m = Matrix4f::RotationY(remoteYaw) * Matrix4f::RotationX(remotePitch) *
        Matrix4f::RotationZ(remoteRoll);
    remoteRot = Quatf(m);

    Quatf localRemoteRot(FootPose.Rotation.Inverted() * remoteRot);

    Quatf shoulderRot = Quatf().Slerp(localRemoteRot, 0.0f);
    Quatf elbowRot = Quatf().Slerp(localRemoteRot, 0.6f);
    Quatf wristRot = Quatf().Slerp(localRemoteRot, 0.4f);

    /// Pose the skeleton
    Skeleton.UpdateLocalRotation(shoulderRot, ShoulderJointIdx);
    Skeleton.UpdateLocalRotation(elbowRot, ElbowJointIdx);
    Skeleton.UpdateLocalRotation(wristRot, WristJointIdx);
    /// Move the skeleton root
    Skeleton.TransformLocal(FootPose, 0);

    /// Update the joints for beam rendering
    for (int i = 0; i < (int)TransformedJoints.size(); ++i) {
        TransformedJoints[i].Pose = Skeleton.GetWorldSpacePoses()[i];
    }
    outPose = TransformedJoints[static_cast<int>(TransformedJoints.size()) - 1].Pose;
}

} // namespace OVRFW
