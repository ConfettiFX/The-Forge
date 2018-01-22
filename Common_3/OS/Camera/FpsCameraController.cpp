/*
 * Copyright (c) 2018 Confetti Interactive Inc.
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
#include "../Interfaces/ICameraController.h"

#ifdef _DURANGO
#include "../../../CommonXBOXOne_3/OS/inputDurango.h"
#endif

// Include this file as last include in all cpp files allocating memory
#include "../Interfaces/ILogManager.h"
#include "../Interfaces/IMemoryManager.h"

#define MOVE_IN_PLANE  0
#define INVERT_MOUSE_VERTICAL  0

#ifdef _DURANGO
static const float k_rotationSpeed = 0.03f;
static const float k_movementSpeed = 16.0f;
#else
static const float k_rotationSpeed = 0.003f;
#endif
static const float k_scrollSpeed = -5.0f;
static const float k_xRotLimit = (float)(M_PI_2 - 0.1);

class FpsCameraController : public ICameraController
{
public:
	FpsCameraController() :
		viewRotation { 0 },
		viewPosition { 0 },
		acceleration { 100.0f },
		deceleration { 100.0f },
		maxSpeed { 100.0f },
		currentVelocity { 0 },
#ifdef _DURANGO
		mouseCaptured { true }
#else
		mouseCaptured { false }
#endif
	{
	}
	void setMotionParameters(const CameraMotionParameters&) override;

	mat4 getViewMatrix() const override;
	vec3 getViewPosition() const override;
	vec2 getRotationXY() const override { return viewRotation; }

	void moveTo(const vec3& location) override;
	void lookAt(const vec3& lookAt) override;

private:
#ifndef _DURANGO
	void onMouseMove(const MouseMoveEventData* pData) override;
	void onMouseButton(const MouseButtonEventData* pData) override;
	void onMouseWheel(const MouseWheelEventData* pData) override;
#endif
	void update(float deltaTime) override;

private:
	vec2 viewRotation;
	vec3 viewPosition;
  vec3 currentVelocity;

	float acceleration;
	float deceleration;
	float maxSpeed;

	bool mouseCaptured;
};

ICameraController* createFpsCameraController(vec3 startPosition, vec3 startLookAt)
{
	FpsCameraController* cc = conf_placement_new<FpsCameraController>(conf_calloc(1, sizeof(FpsCameraController)));
	cc->moveTo(startPosition);
	cc->lookAt(startLookAt);
	return cc;
}

// TODO: Move to common file
void destroyCameraController(ICameraController* pCamera)
{
	pCamera->~ICameraController();
	conf_free(pCamera);
}

void FpsCameraController::setMotionParameters(const CameraMotionParameters& cmp)
{
	acceleration = cmp.acceleration;
	deceleration = cmp.braking;
	maxSpeed = cmp.maxSpeed;
}

#ifndef _DURANGO
void FpsCameraController::onMouseMove(const MouseMoveEventData* pData)
{
	mouseCaptured = pData->captured;
	if (mouseCaptured)
	{
#ifndef __APPLE__
		float2 w = v2ToF2(viewRotation);
		const float width = (float)(pData->pWindow.fullScreen ? getRectWidth(pData->pWindow.fullscreenRect) : getRectWidth(pData->pWindow.windowedRect));
		const float height = (float)(pData->pWindow.fullScreen ? getRectHeight(pData->pWindow.fullscreenRect) : getRectHeight(pData->pWindow.windowedRect));
#if INVERT_MOUSE_VERTICAL
		const bool invertMouse = true;
#else
		const bool invertMouse = false;
#endif
		w.x += (invertMouse ? 1 : -1) * k_rotationSpeed * (height / 2 - pData->y);
		w.y += k_rotationSpeed * (pData->x - width / 2);

		setMousePositionRelative(&pData->pWindow, (int)width / 2, (int)height / 2);

		viewRotation = f2Tov2(w);
#else
        float rx = viewRotation.getX();
        float ry = viewRotation.getY();
        
        // pData->deltaX: +Right, -Left | Y-Axis rotation
        // pData->deltaY: -Up   , +Down    | X-Axis rotation
        
#ifdef INVERT_MOUSE_VERTICAL
        rx += pData->deltaY * k_rotationSpeed;
#else
        rx += pData->deltaY * k_rotationSpeed;
#endif
        
        ry += pData->deltaX * k_rotationSpeed;
        
        if (rx > k_xRotLimit)
            rx = k_xRotLimit;
        else if (rx < -k_xRotLimit)
            rx = -k_xRotLimit;
        
        if (ry > M_PI_2 * 2.0)
            ry -= float(M_PI_2 * 4.0);
        if (ry < -M_PI_2 * 2.0)
            ry += float(M_PI_2 * 4.0);
        
        viewRotation = { rx, ry };
#endif
	}
}

void FpsCameraController::onMouseButton(const MouseButtonEventData* pData)
{
}

void FpsCameraController::onMouseWheel(const MouseWheelEventData* pData)
{
	mat4 m { mat4::rotationYX(viewRotation.getY(), viewRotation.getX()) };
	const vec3& v { m.getCol2().getXYZ() };
	viewPosition -= v * ((float)pData->scroll * k_scrollSpeed);
}
#endif

void FpsCameraController::update(float deltaTime)
{
#if !defined(TARGET_IOS)
	if (deltaTime < 0.00001f)
		return;

	vec3 move { 0 };

	if (mouseCaptured)
	{
#ifdef _DURANGO
		// Update velocity vector
		move += vec3(
			Input::GetCurrentGamepadReading().NormalizedLeftThumbstickX(),
			Input::GetCurrentGamepadReading().RightTrigger() - Input::GetCurrentGamepadReading().LeftTrigger(),
			Input::GetCurrentGamepadReading().NormalizedLeftThumbstickY()
		) * k_movementSpeed;

		// Update view direction
		viewRotation += vec2(
			Input::GetCurrentGamepadReading().NormalizedRightThumbstickY() * k_rotationSpeed,
			Input::GetCurrentGamepadReading().NormalizedRightThumbstickX() * k_rotationSpeed);
#else
		static const struct KeyMovers { int32_t key; vec3 delta; } deltas[] =
		{
			{ KEY_W, vec3(0.0f, 0.0f,  1.0f) },
			{ KEY_A, vec3(-1.0f, 0.0f,  0.0f) },
			{ KEY_S, vec3(0.0f, 0.0f, -1.0f) },
			{ KEY_D, vec3(1.0f, 0.0f,  0.0f) },

			{ KEY_UP,    vec3(0.0f, 0.0f,  1.0f) },
			{ KEY_LEFT,  vec3(-1.0f, 0.0f,  0.0f) },
			{ KEY_DOWN,  vec3(0.0f, 0.0f, -1.0f) },
			{ KEY_RIGHT, vec3(1.0f, 0.0f,  0.0f) },
		};

		for (KeyMovers const& keydata : deltas)
		{
			if (getKeyDown(keydata.key))
				move += keydata.delta;
		}
#endif

		float lenS = lengthSqr(move);
		if (lenS > 1.0f)
			move *= sqrtf(lenS);
	}

#if MOVE_IN_PLANE
	mat4 rot =  mat4::rotationY(viewRotation.getY());
#else
	mat4 rot{ mat4::rotationYX(viewRotation.getY(), viewRotation.getX()) };
#endif

	vec3 accelVec = (rot * move).getXYZ();


	// the acceleration vector should still be unit length.
	//assert(fabs(1.0f - lengthSqr(accelVec)) < 0.001f);
	float currentInAccelDir = dot(accelVec, currentVelocity);
	if (currentInAccelDir < 0)
		currentInAccelDir = 0;
	vec3 braking = (accelVec * currentInAccelDir) - currentVelocity;
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
	vec3 newVelocity = currentVelocity + (accelVec * deltaTime);
	float nvLen = lengthSqr(newVelocity);
	if (nvLen > (maxSpeed * maxSpeed))
	{
		nvLen = sqrtf(nvLen);
		newVelocity *= maxSpeed / nvLen;
	}

	move = ((currentVelocity + newVelocity) * .5f) * deltaTime;
	viewPosition += move;
	currentVelocity = newVelocity;
#ifndef _DURANGO
  float velocityX = currentVelocity.getX();
	ASSERT(NAN != velocityX);
#endif
#endif
}

mat4 FpsCameraController::getViewMatrix() const
{	
	mat4 r = mat4::rotationXY(-viewRotation.getX(), -viewRotation.getY());
	vec4 t = r * vec4(-viewPosition, 1.0f);
	r.setTranslation(t.getXYZ());
	return r;
}

vec3 FpsCameraController::getViewPosition() const
{
	return viewPosition;
}

void FpsCameraController::moveTo(const vec3& location)
{
	viewPosition = location;
	currentVelocity = vec3(0);
}

void FpsCameraController::lookAt(const vec3& lookAt)
{
	vec3 lookDir = normalize(lookAt- viewPosition);

	float y = lookDir.getY();
	viewRotation.setX(-asinf(y));

	float x = lookDir.getX();
	float z = lookDir.getZ();
	float n = sqrtf((x*x) + (z*z));
	if (n > 0.01f)
	{
		// don't change the Y rotation if we're too close to vertical
		x /= n;
		z /= n;
		viewRotation.setY(atan2f(x, z));
	}
}

