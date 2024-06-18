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

#define _USE_MATH_DEFINES
#include "../Utilities/Interfaces/ILog.h"
#include "Interfaces/ICameraController.h"

// Include this file as last include in all cpp files allocating memory
#include "../Utilities/Interfaces/IMemory.h"

static const float k_scrollSpeed = -5.0f;

class FpsCameraController: public ICameraController
{
public:
    FpsCameraController():
        viewRotation{ 0 }, viewPosition{ 0 }, currentVelocity{ 0 }, acceleration{ 100.0f }, deceleration{ 100.0f }, maxSpeed{ 100.0f },
        movementSpeed{ 1.0f }, rotationSpeed{ 1.0f }
    {
    }
    void setMotionParameters(const CameraMotionParameters&) override;

    mat4 getViewMatrix() const override;
    vec3 getViewPosition() const override;
    vec2 getRotationXY() const override { return viewRotation; }

    void moveTo(const vec3& location) override;
    void lookAt(const vec3& lookAt) override;
    void setViewRotationXY(const vec2& v) override { viewRotation = v; }

    void resetView() override
    {
        moveTo(startPosition);
        lookAt(startLookAt);
    }
    void onMove(const float2& vec) override
    {
        dx = vec[0];
        dz = vec[1];
    }
    void onMoveY(float y) override { dy = y; }
    void onRotate(const float2& vec) override
    {
        drx = -vec[1];
        dry = vec[0];
    }
    void onZoom(const float2& vec) override { zoom = vec[1]; }

    void update(float deltaTime) override;

    vec3 startPosition;
    vec3 startLookAt;

    vec2 viewRotation;
    vec3 viewPosition;
    vec3 currentVelocity;

    float acceleration;
    float deceleration;
    float maxSpeed;
    float movementSpeed;
    float rotationSpeed;

    float drx = 0.0f;
    float dry = 0.0f;
    float dx = 0.0f;
    float dy = 0.0f;
    float dz = 0.0f;
    float zoom = 0.0f;
};

ICameraController* initFpsCameraController(const vec3& startPosition, const vec3& startLookAt)
{
    FpsCameraController* cc = tf_placement_new<FpsCameraController>(tf_calloc(1, sizeof(FpsCameraController)));
    cc->moveTo(startPosition);
    cc->lookAt(startLookAt);

    cc->startPosition = startPosition;
    cc->startLookAt = startLookAt;

    return cc;
}

// TODO: Move to common file
void exitCameraController(ICameraController* pCamera)
{
    pCamera->~ICameraController();
    tf_free(pCamera);
}

void FpsCameraController::setMotionParameters(const CameraMotionParameters& cmp)
{
    acceleration = cmp.acceleration;
    deceleration = cmp.braking;
    maxSpeed = cmp.maxSpeed;
    movementSpeed = cmp.movementSpeed;
    rotationSpeed = cmp.rotationSpeed;
}

void FpsCameraController::update(float deltaTime)
{
    // when frame time is too small (01 in releaseVK) the float comparison with zero is imprecise.
    // It returns when it shouldn't causing stutters
    // We should use doubles for frame time instead of just do this for now.
    deltaTime = max(deltaTime, 0.000001f);

    vec3 moveVec = { dx, dy, dz };

    viewRotation += vec2(drx, dry) * rotationSpeed * deltaTime;

    // divide by length to normalize if necessary
    float lenS = lengthSqr(moveVec);
    // one reason the check with > 1.0 instead of 0.0 is to avoid
    // normalizing when joystick is not fully down.
    if (lenS > 1.0f)
        moveVec /= sqrtf(lenS);

    vec3 accelVec = vec4(moveVec).getXYZ();
    // divide by length to normalize if necessary
    // this determines the directional of acceleration, should be normalized.
    lenS = lengthSqr(accelVec);
    if (lenS > 1.0f)
        accelVec /= sqrtf(lenS);

    // the acceleration vector should still be unit length.
    // assert(fabs(1.0f - lengthSqr(accelVec)) < 0.001f);
    float currentInAccelDir = dot(accelVec, currentVelocity);
    if (currentInAccelDir < 0)
        currentInAccelDir = 0;

    vec3  braking = (accelVec * currentInAccelDir) - currentVelocity;
    float brakingLen = length(braking);
    if (brakingLen > (deceleration * deltaTime))
    {
        braking *= deceleration / brakingLen;
    }
    else
    {
        braking /= deltaTime;
    }

    accelVec = (accelVec * acceleration) + braking;
    vec3  newVelocity = currentVelocity + (accelVec * deltaTime);
    float nvLen = lengthSqr(newVelocity);
    if (nvLen > (maxSpeed * maxSpeed))
    {
        nvLen = sqrtf(nvLen);
        newVelocity *= (maxSpeed / nvLen);
    }

    // create rotation matrix
    mat4 vrRotation = mat4::identity();
#if defined(QUEST_VR)
    vrRotation.setUpper3x3(inverse(pQuest->mViewMatrix.getUpper3x3()));
    viewRotation.setX(0.0f); // No rotation around the x axis when using vr
#endif
    mat4 rot = mat4::rotationYX(viewRotation.getY(), viewRotation.getX()) * vrRotation;

    moveVec = (rot * ((currentVelocity + newVelocity) * .5f) * deltaTime).getXYZ();
    viewPosition += moveVec * movementSpeed;
    currentVelocity = newVelocity;

    if (zoom)
    {
        mat4        m{ mat4::rotationYX(viewRotation.getY(), viewRotation.getX()) };
        const vec3& v{ m.getCol2().getXYZ() };
        viewPosition -= v * (zoom * k_scrollSpeed);
    }
}

mat4 FpsCameraController::getViewMatrix() const
{
    mat4 r = mat4::rotationXY(-viewRotation.getX(), -viewRotation.getY());
#if defined(QUEST_VR)
    mat4 vrViewMat = pQuest->mViewMatrix;
    vrViewMat.setTranslation(vec3(0.0f));
    r = vrViewMat * r;
#endif
    vec4 t = r * vec4(-viewPosition, 1.0f);
    r.setTranslation(t.getXYZ());
    return r;
}

vec3 FpsCameraController::getViewPosition() const { return viewPosition; }

void FpsCameraController::moveTo(const vec3& location)
{
    viewPosition = location;
    currentVelocity = vec3(0);
}

void FpsCameraController::lookAt(const vec3& lookAt)
{
    vec3 lookDir = normalize(lookAt - viewPosition);

    float y = lookDir.getY();
    viewRotation.setX(-asinf(y));

    float x = lookDir.getX();
    float z = lookDir.getZ();
    float n = sqrtf((x * x) + (z * z));
    if (n > 0.01f)
    {
        // don't change the Y rotation if we're too close to vertical
        x /= n;
        z /= n;
        viewRotation.setY(atan2f(x, z));
    }
}

class GuiCameraController: public ICameraController
{
public:
    GuiCameraController(): viewRotation{ 0 }, viewPosition{ 0 }, velocity{ 0 }, maxSpeed{ 1.0f } {}
    void setMotionParameters(const CameraMotionParameters& cmp) override { maxSpeed = cmp.maxSpeed; }

    void update(float deltaTime) override
    {
        viewPosition += velocity * deltaTime;
        velocity = vec3{ 0 };
    }

    mat4 getViewMatrix() const override
    {
        mat4 r{ mat4::rotationXY(-viewRotation.getX(), -viewRotation.getY()) };
        vec4 t = r * vec4(-viewPosition, 1.0f);
        r.setTranslation(t.getXYZ());
        return r;
    }

    vec3 getViewPosition() const override { return viewPosition; }

    void moveTo(const vec3& location) override { viewPosition = location; }

    void lookAt(const vec3& lookAt) override
    {
        vec3 lookDir = normalize(lookAt - viewPosition);

        float y = lookDir.getY();
        viewRotation.setX(-asinf(y));

        float x = lookDir.getX();
        float z = lookDir.getZ();
        float n = sqrtf((x * x) + (z * z));
        if (n > 0.01f)
        {
            // don't change the Y rotation if we're too close to vertical
            x /= n;
            z /= n;
            viewRotation.setY(atan2f(x, z));
        }
    }

    void setViewRotationXY(const vec2& v) override { viewRotation = v; }

    vec2 getRotationXY() const override { return viewRotation; }

    void resetView() override
    {
        moveTo(startPosition);
        lookAt(startLookAt);
    }
    void onMove(const float2& vec) override { UNREF_PARAM(vec); }
    void onMoveY(float y) override { UNREF_PARAM(y); }
    void onRotate(const float2& vec) override { UNREF_PARAM(vec); }
    void onZoom(const float2& vec) override { UNREF_PARAM(vec); }

    // We put viewRotation at first becuase viewPosition is 16 bytes aligned. We have vtable pointer 8 bytes + vec2(8 bytes). This avoids
    // unnecessary padding.
    vec2  viewRotation;
    vec3  viewPosition;
    vec3  velocity;
    float maxSpeed;
    vec3  startPosition;
    vec3  startLookAt;
};

ICameraController* initGuiCameraController(const vec3& startPosition, const vec3& startLookAt)
{
    GuiCameraController* cc = tf_placement_new<GuiCameraController>(tf_calloc(1, sizeof(GuiCameraController)));
    cc->moveTo(startPosition);
    cc->lookAt(startLookAt);
    cc->startPosition = startPosition;
    cc->startLookAt = startLookAt;
    return cc;
}

void destroyGuiCameraController(ICameraController* pCamera)
{
    pCamera->~ICameraController();
    tf_free(pCamera);
}

CameraMatrix::CameraMatrix() {}

CameraMatrix::CameraMatrix(const CameraMatrix& mat)
{
    mLeftEye = mat.mLeftEye;
#if defined(QUEST_VR)
    mRightEye = mat.mRightEye;
#endif
}

void CameraMatrix::applyProjectionSampleOffset(float xOffset, float yOffset)
{
    mLeftEye[2][0] += xOffset;
    mLeftEye[2][1] += yOffset;
#if defined(QUEST_VR)
    mRightEye[2][0] += xOffset;
    mRightEye[2][1] += yOffset;
#endif
}