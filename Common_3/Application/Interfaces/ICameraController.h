/*
 * Copyright (c) 2017-2025 The Forge Interactive Inc.
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
#include "../../Graphics/OpenXR/OpenXRApi.h"
#include "../../Graphics/OpenXR/OpenXRApiUtils.h"
#endif

struct CameraMotionParameters
{
    float maxSpeed = 160.0f;
    float acceleration = 600.0f; // only used with binary inputs such as keypresses
    float braking = 200.0f;      // also acceleration but orthogonal to the acceleration vector
    float movementSpeed = 1.0f;  // customize move speed
    float rotationSpeed = 1.0f;  // customize rotation speed
};

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

    void setTranslation(const vec3& translation);

    static inline const CameraMatrix inverse(const CameraMatrix& mat);
    static inline const CameraMatrix transpose(const CameraMatrix& mat);
    static inline const CameraMatrix perspective(float fovxRadians, float aspectInverse, float zNear, float zFar);
    static inline const CameraMatrix perspectiveReverseZ(float fovxRadians, float aspectInverse, float zNear, float zFar);
#ifdef QUEST_VR
    static inline void superFrustumReverseZ(const CameraMatrix& views, float zNear, float zFar, mat4& outView, mat4& outProject);
#endif

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
    GetOpenXRProjMatrixPerspective(LEFT_EYE_VIEW, zNear, zFar, &result.mLeftEye);
    GetOpenXRProjMatrixPerspective(RIGHT_EYE_VIEW, zNear, zFar, &result.mRightEye);
#else
    result.mCamera = mat4::perspectiveLH(fovxRadians, aspectInverse, zNear, zFar);
#endif
    return result;
}

inline const CameraMatrix CameraMatrix::perspectiveReverseZ(float fovxRadians, float aspectInverse, float zNear, float zFar)
{
    CameraMatrix result;
#if defined(QUEST_VR)
    GetOpenXRProjMatrixPerspectiveReverseZ(LEFT_EYE_VIEW, zNear, zFar, &result.mLeftEye);
    GetOpenXRProjMatrixPerspectiveReverseZ(RIGHT_EYE_VIEW, zNear, zFar, &result.mRightEye);
#else
    result.mCamera = mat4::perspectiveLH_ReverseZ(fovxRadians, aspectInverse, zNear, zFar);
#endif
    return result;
}

// Create a new projection matrix based on the left and right eye asymmetric FOV matrices. This combines the maximum
// field of view on each side (left, right, top, bottom). This will result in a new asymmetric matrix that will
// encompass both eyes. Move camera back the necessary amount to get the new frustum to encompass both eyes.
// This is useful to run a single pass triangle culling algorithm.
// Inspired by Oculus' work here: https://i.sstatic.net/TpHYa.jpg
#ifdef QUEST_VR
inline void CameraMatrix::superFrustumReverseZ(const CameraMatrix& views, float zNear, float zFar, mat4& outView, mat4& outProject)
{
    EyesFOV fovs = GetOpenXRViewFovs();

    // Vector pointing from left eye to right eye
    const Vector3 leftEyeToRightEyeDir = views.mRightEye.getTranslation() - views.mLeftEye.getTranslation();
    const Vector3 normalizedLeftToRight = normalize(leftEyeToRightEyeDir);

    // IPD is the distance between each view's origin, which is the real world distance between each eye
    float ipd = length(leftEyeToRightEyeDir);

    float leftFov = fovs.mLeftEye.x;
    float rightFov = fovs.mRightEye.y;
    float topFov = fovs.mLeftEye.z;
    float bottomFov = fovs.mLeftEye.w;

    // How we need the new view origin to go back
    float recession = ipd / (tan(-leftFov) + tan(rightFov));
    // The new view origin offset along the leftEyeToRightEyeDir axis
    float viewCenterOffset = ipd * tan(-leftFov);

    Vector3 viewDirection = normalize(-views.mLeftEye.getRow(2).getXYZ());

    Vector3 superFrustumOrigin = views.mLeftEye.getTranslation();
    superFrustumOrigin += viewCenterOffset * normalizedLeftToRight;
    superFrustumOrigin -= recession * viewDirection;
    outView = views.mLeftEye;
    outView.setTranslation(superFrustumOrigin);
    outProject = mat4::perspectiveLH_ReverseZ_AsymmetricFov(-leftFov, rightFov, topFov, -bottomFov, zNear, zFar, false);
}
#endif

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

class ICameraController
{
public:
    virtual ~ICameraController() {}
    virtual void setMotionParameters(const CameraMotionParameters&) = 0;
    virtual void update(float deltaTime) = 0;

    // there are also implicit dependencies on the keyboard state.

    virtual CameraMatrix getViewMatrix() const = 0;
    virtual vec3         getViewPosition() const = 0;
    virtual vec2         getRotationXY() const = 0;
    virtual void         moveTo(const vec3& location) = 0;
    virtual void         lookAt(const vec3& lookAt) = 0;
    virtual void         setViewRotationXY(const vec2& v) = 0;
    virtual void         resetView() = 0;

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

FORGE_API bool loadCameraPath(const char* pFileName, uint32_t& outNumCameraPoints, float3** pOutCameraPoints);

FORGE_API void exitCameraController(ICameraController* pCamera);
