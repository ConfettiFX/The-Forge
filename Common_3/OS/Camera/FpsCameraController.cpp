/*
 * Copyright (c) 2018-2019 Confetti Interactive Inc.
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

#ifdef _WIN32
#ifndef _DURANGO
#include <ntverp.h>
#endif
#endif

#include "../../../Middleware_3/Input/InputSystem.h"
#include "../../../Middleware_3/Input/InputMappings.h"
#include "../../../Middleware_3/UI/AppUI.h"

// Include this file as last include in all cpp files allocating memory
#include "../Interfaces/ILogManager.h"

#include "../Interfaces/IMemoryManager.h"

static float       k_joystickRotationSpeed = 0.06f;
static const float k_scrollSpeed = -5.0f;
#if !defined(METAL) && !defined(__ANDROID__)
static const float k_xRotLimit = (float)(M_PI_2 - 0.1);
#endif
static float k_mouseRotationSpeed = 0.003f;

class FpsCameraController: public ICameraController
{
	public:
	FpsCameraController():
		viewRotation{ 0 },
		viewPosition{ 0 },
		currentVelocity{ 0 },
		acceleration{ 100.0f },
		deceleration{ 100.0f },
		maxSpeed{ 100.0f },
		pVirtualJoystick{ NULL }
	{
	}
	void setMotionParameters(const CameraMotionParameters&) override;
	void setVirtualJoystick(VirtualJoystickUI* virtualJoystick = NULL) override;

	mat4 getViewMatrix() const override;
	vec3 getViewPosition() const override;
	vec2 getRotationXY() const override { return viewRotation; }

	void moveTo(const vec3& location) override;
	void lookAt(const vec3& lookAt) override;

	private:
	bool onInputEvent(const ButtonData* pData) override;
	void update(float deltaTime) override;

	private:
	vec2 viewRotation;
	vec3 viewPosition;
	vec3 currentVelocity;

	float acceleration;
	float deceleration;
	float maxSpeed;

	VirtualJoystickUI* pVirtualJoystick;
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

void FpsCameraController::setVirtualJoystick(VirtualJoystickUI* virtualJoystick) { pVirtualJoystick = virtualJoystick; }

static vec3 moveVec{ 0 };
bool        FpsCameraController::onInputEvent(const ButtonData* pData)
{
	// If mouse is not captured or the input event was consumed
	// Do nothing
	if (!InputSystem::IsMouseCaptured() || pData->mEventConsumed)
		return false;

	// update joystick if available and consume event
	if (pVirtualJoystick && pVirtualJoystick->OnInputEvent(pData))
		return true;

	if (pData->mUserId == KEY_MOUSE_WHEEL)
	{
		mat4        m{ mat4::rotationYX(viewRotation.getY(), viewRotation.getX()) };
		const vec3& v{ m.getCol2().getXYZ() };
		viewPosition -= v * ((float)pData->mValue[0] * k_scrollSpeed);
	}

	return true;
}

void FpsCameraController::update(float deltaTime)
{
	//when frame time is too small (01 in releaseVK) the float comparison with zero is imprecise.
	//It returns when it shouldn't causing stutters
	//We should use doubles for frame time instead of just do this for now.
	deltaTime = max(deltaTime, 0.000001f);

	//input should be captured by default on consoles/mobiles.
	if (!InputSystem::IsMouseCaptured())
		return;

	if (pVirtualJoystick)
	{
		pVirtualJoystick->Update(deltaTime);

		vec2 leftStickDir = pVirtualJoystick->GetLeftStickDir();
		if (leftStickDir.getX() + leftStickDir.getY() != 0.0f)
		{
			moveVec += vec3(leftStickDir.getX(), 0, -leftStickDir.getY());
		}
		vec2 rightStickDir = pVirtualJoystick->GetRightStickDir();
		if (rightStickDir.getX() + rightStickDir.getY() != 0.0f)
		{
			vec2 joystickValue = vec2(rightStickDir.getY(), rightStickDir.getX());

			viewRotation += joystickValue * k_mouseRotationSpeed * 10.f;
		}
	}
	else
	{
		//We want to keep checking instead of on doing it on events
		//Events get triggered on press or release (except for mouse)
		//this is in case a button is pressed down but hasn't changed.
		ButtonData leftStick = InputSystem::GetButtonData(KEY_LEFT_STICK);
		if (leftStick.mIsPressed)
		{
			moveVec.setX(leftStick.mValue[0]);
			moveVec.setY(0);
			moveVec.setZ(leftStick.mValue[1]);
		}

		//We want to keep checking instead of on events
		//Events get triggered on press or release(except for mouse)
		//will need to handle iOS differently for now
		ButtonData rightStick = InputSystem::GetButtonData(KEY_RIGHT_STICK);
		if (rightStick.mIsPressed)
		{
			//Windows Fall Creators Update breaks this camera controller
			//  So only use this controller if we are running on macOS or before Fall Creators Update
			float rx = viewRotation.getX();
			float ry = viewRotation.getY();
			float interpolant = 0.75f;

			float newRx = rx;
			float newRy = ry;

			//Use different values for different input devices.
			//For mouse we want deltas.
			//For controllers and touch we want current value.
			if (rightStick.mActiveDevicesMask & GainputDeviceType::GAINPUT_GAMEPAD)
			{
				newRx = rx + (rightStick.mValue[1] * k_joystickRotationSpeed) * interpolant;
				newRy = ry + (rightStick.mValue[0] * k_joystickRotationSpeed) * interpolant;
			}
			else if (rightStick.mActiveDevicesMask & GainputDeviceType::GAINPUT_RAW_MOUSE)
			{
				newRx = rx + (rightStick.mDeltaValue[1] * k_mouseRotationSpeed) * interpolant;
				newRy = ry + (rightStick.mDeltaValue[0] * k_mouseRotationSpeed) * interpolant;
			}

			//set new target view to interpolate to
			viewRotation = { newRx, newRy };
		}
	}

	//divide by length to normalize if necessary
	float lenS = lengthSqr(moveVec);
	//one reason the check with > 1.0 instead of 0.0 is to avoid
	//normalizing when joystick is not fully down.
	if (lenS > 1.0f)
		moveVec /= sqrtf(lenS);

	//create rotation matrix
	mat4 rot{ mat4::rotationYX(viewRotation.getY(), viewRotation.getX()) };

	vec3 accelVec = (rot * moveVec).getXYZ();
	//divide by length to normalize if necessary
	//this determines the directional of acceleration, should be normalized.
	lenS = lengthSqr(accelVec);
	if (lenS > 1.0f)
		accelVec /= sqrtf(lenS);

	// the acceleration vector should still be unit length.
	//assert(fabs(1.0f - lengthSqr(accelVec)) < 0.001f);
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

	moveVec = ((currentVelocity + newVelocity) * .5f) * deltaTime;
	viewPosition += moveVec;
	currentVelocity = newVelocity;

	moveVec = { 0, 0, 0 };
}

mat4 FpsCameraController::getViewMatrix() const
{
	mat4 r = mat4::rotationXY(-viewRotation.getX(), -viewRotation.getY());
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
