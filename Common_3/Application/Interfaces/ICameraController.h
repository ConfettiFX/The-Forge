/*
 * Copyright (c) 2017-2024 The Forge Interactive Inc.
 *
 * This file is part of The-Forge
 * (see https://github.com/ConfettiFX/The-Forge).
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#pragma once

#include "../../Application/Config.h"

#include "../../OS/Interfaces/IOperatingSystem.h"

#include "../../Utilities/Math/MathTypes.h"

#if defined(QUEST_VR)
#include "../../OS/Quest/VrApi.h"
extern QuestVR* pQuest;
#endif

struct CameraMotionParameters
{
    float maxSpeed = 160.0f;
    float acceleration = 600.0f; // only used with binary inputs such as keypresses
    float braking = 200.0f;      // also acceleration but orthogonal to the acceleration vector
    float movementSpeed = 1.0f;  // customize move speed
    float rotationSpeed = 1.0f;  // customize rotation speed
};

class ICameraController
{
public:
    virtual ~ICameraController() {}
    virtual void setMotionParameters(const CameraMotionParameters&) = 0;
    virtual void update(float deltaTime) = 0;

    // there are also implicit dependencies on the keyboard state.

    virtual mat4 getViewMatrix() const = 0;
    virtual vec3 getViewPosition() const = 0;
    virtual vec2 getRotationXY() const = 0;
    virtual void moveTo(const vec3& location) = 0;
    virtual void lookAt(const vec3& lookAt) = 0;
    virtual void setViewRotationXY(const vec2& v) = 0;
    virtual void resetView() = 0;

    virtual void onMove(const float2& vec) = 0;
    virtual void onMoveY(float y) = 0;
    virtual void onRotate(const float2& vec) = 0;
    virtual void onZoom(const float2& vec) = 0;
};

/// \c initGuiCameraController assumes that the camera is not rotated around the look direction;
/// in its matrix, \c Z points at \c startLookAt and \c X is horizontal.
FORGE_API ICameraController* initGuiCameraController(const vec3& startPosition, const vec3& startLookAt);

/// \c initFpsCameraController does basic FPS-style god mode navigation; tf_free-look is constrained
/// to about +/- 88 degrees and WASD translates in the camera's local XZ plane.
FORGE_API ICameraController* initFpsCameraController(const vec3& startPosition, const vec3& startLookAt);

FORGE_API void exitCameraController(ICameraController* pCamera);

class FORGE_API CameraMatrix
{
public:
    CameraMatrix();
    CameraMatrix(const CameraMatrix& mat);
    inline const CameraMatrix& operator=(const CameraMatrix& mat);

    inline const CameraMatrix  operator*(const Matrix4& mat) const;
    friend inline CameraMatrix operator*(const Matrix4& mat, const CameraMatrix& cMat);
    inline const CameraMatrix  operator*(const CameraMatrix& mat) const;

    // Applies offsets to the projection matrices (useful when needing to jitter the camera for techniques like TAA)
    void applyProjectionSampleOffset(float xOffset, float yOffset);

    static inline const CameraMatrix inverse(const CameraMatrix& mat);
    static inline const CameraMatrix transpose(const CameraMatrix& mat);
    static inline const CameraMatrix perspective(float fovxRadians, float aspectInverse, float zNear, float zFar);
    static inline const CameraMatrix perspectiveReverseZ(float fovxRadians, float aspectInverse, float zNear, float zFar);
    static inline const CameraMatrix orthographic(float left, float right, float bottom, float top, float zNear, float zFar);
    static inline const CameraMatrix orthographicReverseZ(float left, float right, float bottom, float top, float zNear, float zFar);
    static inline const CameraMatrix identity();
    static inline void extractFrustumClipPlanes(const CameraMatrix& vp, Vector4& rcp, Vector4& lcp, Vector4& tcp, Vector4& bcp,
                                                Vector4& fcp, Vector4& ncp, bool const normalizePlanes);

public:
    union
    {
        mat4 mCamera;
        mat4 mLeftEye;
    };
#if defined(QUEST_VR)
    mat4 mRightEye;
#endif
};

inline const CameraMatrix& CameraMatrix::operator=(const CameraMatrix& mat)
{
    mLeftEye = mat.mLeftEye;
#if defined(QUEST_VR)
    mRightEye = mat.mRightEye;
#endif
    return *this;
}

inline const CameraMatrix CameraMatrix::operator*(const Matrix4& mat) const
{
    CameraMatrix result;
    result.mLeftEye = mLeftEye * mat;
#if defined(QUEST_VR)
    result.mRightEye = mRightEye * mat;
#endif
    return result;
}

inline CameraMatrix operator*(const Matrix4& mat, const CameraMatrix& cMat)
{
    CameraMatrix result;
    result.mLeftEye = mat * cMat.mLeftEye;
#if defined(QUEST_VR)
    result.mRightEye = mat * cMat.mRightEye;
#endif
    return result;
}

inline const CameraMatrix CameraMatrix::operator*(const CameraMatrix& mat) const
{
    CameraMatrix result;
    result.mLeftEye = mLeftEye * mat.mLeftEye;
#if defined(QUEST_VR)
    result.mRightEye = mRightEye * mat.mRightEye;
#endif
    return result;
}

inline const CameraMatrix CameraMatrix::inverse(const CameraMatrix& mat)
{
    CameraMatrix result;
    result.mLeftEye = ::inverse(mat.mLeftEye);
#if defined(QUEST_VR)
    result.mRightEye = ::inverse(mat.mRightEye);
#endif
    return result;
}

inline const CameraMatrix CameraMatrix::transpose(const CameraMatrix& mat)
{
    CameraMatrix result;
    result.mLeftEye = ::transpose(mat.mLeftEye);
#if defined(QUEST_VR)
    result.mRightEye = ::transpose(mat.mRightEye);
#endif
    return result;
}

inline const CameraMatrix CameraMatrix::perspective(float fovxRadians, float aspectInverse, float zNear, float zFar)
{
    CameraMatrix result;
#if defined(QUEST_VR)
    float4 fov;
    ovrMatrix4f_ExtractFov(&pQuest->mHeadsetTracking.Eye[VRAPI_EYE_LEFT].ProjectionMatrix, &fov.x, &fov.y, &fov.z, &fov.w);
    result.mLeftEye = mat4::perspectiveLH_AsymmetricFov(fov.x, fov.y, fov.z, fov.w, zNear, zFar);

    ovrMatrix4f_ExtractFov(&pQuest->mHeadsetTracking.Eye[VRAPI_EYE_RIGHT].ProjectionMatrix, &fov.x, &fov.y, &fov.z, &fov.w);
    result.mRightEye = mat4::perspectiveLH_AsymmetricFov(fov.x, fov.y, fov.z, fov.w, zNear, zFar);
#else
    result.mCamera = mat4::perspectiveLH(fovxRadians, aspectInverse, zNear, zFar);
#endif
    return result;
}

inline const CameraMatrix CameraMatrix::perspectiveReverseZ(float fovxRadians, float aspectInverse, float zNear, float zFar)
{
    CameraMatrix result;
#if defined(QUEST_VR)
    float4 fov;
    ovrMatrix4f_ExtractFov(&pQuest->mHeadsetTracking.Eye[VRAPI_EYE_LEFT].ProjectionMatrix, &fov.x, &fov.y, &fov.z, &fov.w);
    result.mLeftEye = mat4::perspectiveLH_AsymmetricFov(fov.x, fov.y, fov.z, fov.w, zFar, zNear);

    ovrMatrix4f_ExtractFov(&pQuest->mHeadsetTracking.Eye[VRAPI_EYE_RIGHT].ProjectionMatrix, &fov.x, &fov.y, &fov.z, &fov.w);
    result.mRightEye = mat4::perspectiveLH_AsymmetricFov(fov.x, fov.y, fov.z, fov.w, zFar, zNear);

    return result;
#else
    result.mCamera = mat4::perspectiveLH_ReverseZ(fovxRadians, aspectInverse, zNear, zFar);
#endif
    return result;
}

inline const CameraMatrix CameraMatrix::orthographic(float left, float right, float bottom, float top, float zNear, float zFar)
{
    CameraMatrix result;
#if defined(QUEST_VR)
    result.mLeftEye = mat4::orthographicLH(left, right, bottom, top, zNear, zFar);
    result.mRightEye = result.mLeftEye;
#else
    result.mCamera = mat4::orthographicLH(left, right, bottom, top, zNear, zFar);
#endif
    return result;
}

inline const CameraMatrix CameraMatrix::orthographicReverseZ(float left, float right, float bottom, float top, float zNear, float zFar)
{
    CameraMatrix result;
#if defined(QUEST_VR)
    result.mLeftEye = mat4::orthographicLH(left, right, bottom, top, zFar, zNear);
    result.mRightEye = result.mLeftEye;
#else
    result.mCamera = mat4::orthographicLH_ReverseZ(left, right, bottom, top, zNear, zFar);
#endif
    return result;
}

inline const CameraMatrix CameraMatrix::identity()
{
    CameraMatrix result;
    result.mLeftEye = mat4::identity();
#if defined(QUEST_VR)
    result.mRightEye = mat4::identity();
#endif
    return result;
}

inline void CameraMatrix::extractFrustumClipPlanes(const CameraMatrix& vp, Vector4& rcp, Vector4& lcp, Vector4& tcp, Vector4& bcp,
                                                   Vector4& fcp, Vector4& ncp, bool const normalizePlanes)
{
#if defined(QUEST_VR)
    // Left plane
    lcp = vp.mLeftEye.getRow(3) + vp.mLeftEye.getRow(0);

    // Right plane
    rcp = vp.mRightEye.getRow(3) - vp.mRightEye.getRow(0);

    // Bottom plane
    bcp = vp.mLeftEye.getRow(3) + vp.mLeftEye.getRow(1);

    // Top plane
    tcp = vp.mLeftEye.getRow(3) - vp.mLeftEye.getRow(1);

    // Near plane
    ncp = vp.mLeftEye.getRow(3) + vp.mLeftEye.getRow(2);

    // Far plane
    fcp = vp.mLeftEye.getRow(3) - vp.mLeftEye.getRow(2);

    // Normalize if needed
    if (normalizePlanes)
    {
        float lcp_norm = length(lcp.getXYZ());
        lcp /= lcp_norm;

        float rcp_norm = length(rcp.getXYZ());
        rcp /= rcp_norm;

        float bcp_norm = length(bcp.getXYZ());
        bcp /= bcp_norm;

        float tcp_norm = length(tcp.getXYZ());
        tcp /= tcp_norm;

        float ncp_norm = length(ncp.getXYZ());
        ncp /= ncp_norm;

        float fcp_norm = length(fcp.getXYZ());
        fcp /= fcp_norm;
    }
#else
    mat4::extractFrustumClipPlanes(vp.mCamera, rcp, lcp, tcp, bcp, fcp, ncp, normalizePlanes);
#endif
}