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

#ifdef _WIN32
#ifndef _DURANGO
#include <ntverp.h>
#endif
#endif

#ifdef _DURANGO
#include "../../../CommonXBOXOne_3/OS/inputDurango.h"
#endif

// Include this file as last include in all cpp files allocating memory
#include "../Interfaces/ILogManager.h"
#include "../../../Middleware_3/Input/InputSystem.h"
#include "../../../Middleware_3/Input/InputMappings.h"

#include "../Interfaces/IMemoryManager.h"

#define MOVE_IN_PLANE  0
#define INVERT_MOUSE_VERTICAL  0

#if defined(_DURANGO) || defined(TARGET_IOS)
static const float k_rotationSpeed = 0.03f;
#if defined(_DURANGO)
	static const float k_movementSpeed = 16.0f;
#endif
#else
static const float k_rotationSpeed = 0.003f;
#endif
static const float k_scrollSpeed = -5.0f;
#if !defined(METAL)
static const float k_xRotLimit = (float)(M_PI_2 - 0.1);
#endif

#if defined(TARGET_IOS) || defined(_DURANGO)
static const float k_vJoystickIntRadius = 0.25f;
static const float k_vJoystickExtRadius = 0.33f;
static const float k_vJoystickRange = k_vJoystickExtRadius - k_vJoystickIntRadius;
static const float k_vJoystickDeadzone = k_vJoystickRange / 10.0f;
static const float k_vJoystickRecovery = k_vJoystickRange * 8.0f;
#endif

static const vec2 k_vLeftJoystickCenter = vec2(0.175f, 0.75f);
static const vec2 k_vRightJoystickCenter = vec2(0.80f, 0.75f);

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
#ifdef TARGET_IOS
        virtualLeftJoystickPos { k_vLeftJoystickCenter },
        virtualLeftJoysticPressed { false },
        virtualRightJoystickPos { k_vRightJoystickCenter },
        virtualRightJoysticPressed { false },
#endif
#if defined(_DURANGO) || defined(TARGET_IOS)
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
    
#ifdef TARGET_IOS
    float getVirtualJoystickInternalRadius() const override {return k_vJoystickIntRadius;}
    float getVirtualJoystickExternalRadius() const override {return k_vJoystickExtRadius;}
    vec2 getVirtualLeftJoystickCenter() const override { return k_vLeftJoystickCenter; }
    vec2 getVirtualLeftJoystickPos() const override;
    vec2 getVirtualRightJoystickCenter() const override { return k_vRightJoystickCenter; }
    vec2 getVirtualRightJoystickPos() const override;
#endif

	void moveTo(const vec3& location) override;
	void lookAt(const vec3& lookAt) override;

private:
	void onInputEvent(const ButtonData* pData) override;
#ifndef _DURANGO
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
    
#ifdef TARGET_IOS
    vec2 virtualLeftJoystickPos;
    bool virtualLeftJoysticPressed;
    vec2 virtualRightJoystickPos;
    bool virtualRightJoysticPressed;
#endif
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

static vec3 moveVec { 0 };
void FpsCameraController::onInputEvent(const ButtonData* pData)
{
	//TODO: Need to rework this logic not to be event based.
#if defined(TARGET_IOS)
	if(pData->mUserId == KEY_LEFT_STICK || pData->mUserId == KEY_RIGHT_STICK)
	{
		if(pData->mIsPressed == false && pData->mIsReleased == false && pData->mIsTriggered == false)
		{
			virtualLeftJoysticPressed = false;
			virtualRightJoysticPressed = false;
			return;
		}
		if(pData->mIsReleased)
		{
			virtualLeftJoysticPressed = false;
			virtualRightJoysticPressed = false;
			return;
		}
			
		// Find the closest touches to the left and right virtual joysticks.
		float minDistLeft = 1.0f;
		int closestTouchLeft = -1;
		float minDistRight = 1.0f;
		int closestTouchRight = -1;
		{
			vec2 touchPos = vec2(pData->mValue[0], pData->mValue[1]);
			
			float distToLeft = length(touchPos - virtualLeftJoystickPos);
			if(distToLeft < minDistLeft && distToLeft <= (k_vJoystickIntRadius * 0.33f)) { minDistLeft = distToLeft; closestTouchLeft = 0; }
			
			float distToRight = length(touchPos - virtualRightJoystickPos);
			if(distToRight < minDistRight && distToRight <= (k_vJoystickIntRadius * 0.33f)) { minDistRight = distToRight; closestTouchRight = 0; }
		}
		
		// Calculate the new joystick positions.
		if(closestTouchLeft >= 0)
		{
			vec2 newPos = virtualLeftJoystickPos - vec2(-pData->mDeltaValue[0], -pData->mDeltaValue[1]);
			
			// Clamp the joystick's position to the max range.
			vec2 tiltDir = newPos - k_vLeftJoystickCenter;
			if(length(tiltDir) > k_vJoystickRange) newPos = k_vLeftJoystickCenter + normalize(tiltDir) * k_vJoystickRange;
			
			virtualLeftJoystickPos = newPos;
			virtualLeftJoysticPressed = true;
		}
		if(closestTouchRight >= 0)
		{
			vec2 newPos = virtualRightJoystickPos - vec2(-pData->mDeltaValue[0], -pData->mDeltaValue[1]);
			
			// Clamp the joystick's position to the max range.
			vec2 tiltDir = newPos - k_vRightJoystickCenter;
			if(length(tiltDir) > k_vJoystickRange) newPos = k_vRightJoystickCenter + normalize(tiltDir) * k_vJoystickRange;
			
			virtualRightJoystickPos = newPos;
			virtualRightJoysticPressed = true;
		}
		// Only disable a joystick if there are more than 1 recorded touches or the other joystick is already disabled.
		// NOTE: iOS sometimes sends touchMove messages one by one when there are multiple touches.
		if(closestTouchLeft < 0 && (closestTouchRight < 0)) virtualLeftJoysticPressed = false;
		if(closestTouchRight < 0 && (closestTouchLeft < 0)) virtualRightJoysticPressed = false;
	}
#endif
}

#ifndef _DURANGO
void FpsCameraController::onMouseWheel(const MouseWheelEventData* pData)
{
	mat4 m { mat4::rotationYX(viewRotation.getY(), viewRotation.getX()) };
	const vec3& v { m.getCol2().getXYZ() };
	viewPosition -= v * ((float)pData->scroll * k_scrollSpeed);
}
#endif

//keeping it for reference for now.
#if 0
void FpsCameraController::onTouch(const TouchEventData *pData)
{
    // Find the closest touches to the left and right virtual joysticks.
    float minDistLeft = 1.0f;
    int closestTouchLeft = -1;
    float minDistRight = 1.0f;
    int closestTouchRight = -1;
    for (uint32_t i = 0; i < pData->touchesRecorded; i++)
    {
        vec2 touchPos = vec2(pData->touchData[i].screenX, pData->touchData[i].screenY);
        
        float distToLeft = length(touchPos - virtualLeftJoystickPos);
        if(distToLeft < minDistLeft && distToLeft <= (k_vJoystickIntRadius * 0.33f)) { minDistLeft = distToLeft; closestTouchLeft = i; }
        
        float distToRight = length(touchPos - virtualRightJoystickPos);
        if(distToRight < minDistRight && distToRight <= (k_vJoystickIntRadius * 0.33f)) { minDistRight = distToRight; closestTouchRight = i; }
    }
    
    // Check if one of the closest joystick touches has ended.
    if(closestTouchLeft >= 0)
    {
        TouchData touch = pData->touchData[closestTouchLeft];
        if(!touch.pressed) virtualLeftJoysticPressed = false;
    }
    if(closestTouchRight >= 0)
    {
        TouchData touch = pData->touchData[closestTouchRight];
        if(!touch.pressed) virtualRightJoysticPressed = false;
    }
}

void FpsCameraController::onTouchMove(const TouchEventData *pData)
{
    // Find the closest touches to the left and right virtual joysticks.
    float minDistLeft = 1.0f;
    int closestTouchLeft = -1;
    float minDistRight = 1.0f;
    int closestTouchRight = -1;
    for (uint32_t i = 0; i < pData->touchesRecorded; i++)
    {
        vec2 touchPos = vec2(pData->touchData[i].screenX, pData->touchData[i].screenY);
        
        float distToLeft = length(touchPos - virtualLeftJoystickPos);
        if(distToLeft < minDistLeft && distToLeft <= (k_vJoystickIntRadius * 0.33f)) { minDistLeft = distToLeft; closestTouchLeft = i; }
        
        float distToRight = length(touchPos - virtualRightJoystickPos);
        if(distToRight < minDistRight && distToRight <= (k_vJoystickIntRadius * 0.33f)) { minDistRight = distToRight; closestTouchRight = i; }
    }
    
    // Calculate the new joystick positions.
    if(closestTouchLeft >= 0)
    {
        TouchData touch = pData->touchData[closestTouchLeft];
        vec2 newPos = virtualLeftJoystickPos - vec2(touch.screenDeltaX, touch.screenDeltaY);
        
        // Clamp the joystick's position to the max range.
        vec2 tiltDir = newPos - k_vLeftJoystickCenter;
        if(length(tiltDir) > k_vJoystickRange) newPos = k_vLeftJoystickCenter + normalize(tiltDir) * k_vJoystickRange;
        
        virtualLeftJoystickPos = newPos;
        virtualLeftJoysticPressed = true;
    }
    if(closestTouchRight >= 0)
    {
        TouchData touch = pData->touchData[closestTouchRight];
        vec2 newPos = virtualRightJoystickPos - vec2(touch.screenDeltaX, touch.screenDeltaY);
        
        // Clamp the joystick's position to the max range.
        vec2 tiltDir = newPos - k_vRightJoystickCenter;
        if(length(tiltDir) > k_vJoystickRange) newPos = k_vRightJoystickCenter + normalize(tiltDir) * k_vJoystickRange;
        
        virtualRightJoystickPos = newPos;
        virtualRightJoysticPressed = true;
    }
    // Only disable a joystick if there are more than 1 recorded touches or the other joystick is already disabled.
    // NOTE: iOS sometimes sends touchMove messages one by one when there are multiple touches.
    if(closestTouchLeft < 0 && (pData->touchesRecorded != 1 || closestTouchRight < 0)) virtualLeftJoysticPressed = false;
    if(closestTouchRight < 0 && (pData->touchesRecorded != 1 || closestTouchLeft < 0)) virtualRightJoysticPressed = false;
}
#endif

void FpsCameraController::update(float deltaTime)
{
	if (deltaTime < 0.00001f)
		return;

#ifndef TARGET_IOS
	//We wamt to keep checking instead of on doing it on events
	//Events get triggered on press or release (except for mouse)
	//this is in case a button is pressed down but hasn't changed.
	if (InputSystem::IsButtonPressed(KEY_LEFT_STICK))
	{
			ButtonData leftStick = InputSystem::GetButtonData(KEY_LEFT_STICK);
			moveVec.setX(leftStick.mValue[0]);
			moveVec.setZ(leftStick.mValue[1]);
	}

	//We wamt to keep checking instead of on events
	//Events get triggered on press or release (except for mouse)
	//will need to handle iOS differently for now
	if (InputSystem::IsButtonPressed(KEY_RIGHT_STICK))
	{

		ButtonData rightStick = InputSystem::GetButtonData(KEY_RIGHT_STICK);
		if (!InputSystem::IsMouseCaptured())
			return;

		if (rightStick.mDeltaValue[0] != 0.0f || rightStick.mDeltaValue[1] != 0.0f)
		{
			// Windows Fall Creators Update breaks this camera controller
			// So only use this controller if we are running on macOS or before Fall Creators Update
			float rx = viewRotation.getX();
			float ry = viewRotation.getY();

			float newRx, newRy;
#if INVERT_MOUSE_VERTICAL
			newRx = rx - rightStick.mValue[1] * k_rotationSpeed;
#else
			newRx = rx + rightStick.mDeltaValue[1] * k_rotationSpeed;
			newRy = ry + rightStick.mDeltaValue[0] * k_rotationSpeed;
#endif
			
			rx = newRx;
			ry = newRy;
			viewRotation = { rx, ry };
			
		
			//LOGINFOF("[FPS] view Rotation: %f, %f", rx, ry);
		}
		else
		{
			// Windows Fall Creators Update breaks this camera controller
			// So only use this controller if we are running on macOS or before Fall Creators Update
			float rx = viewRotation.getX();
			float ry = viewRotation.getY();

			viewRotation = { rx, ry };
		}
	}
#endif
    
#ifdef TARGET_IOS
    {
        vec2 leftJoystickDir = virtualLeftJoystickPos - k_vLeftJoystickCenter;
        if(virtualLeftJoysticPressed)
        {
            // Update velocity vector
            if(length(leftJoystickDir) > k_vJoystickDeadzone)
            {
                vec2 normalizedLeftJoystickDir = normalize(leftJoystickDir);
                float leftJoystickTilt = length(leftJoystickDir) / k_vJoystickRange;
                moveVec += vec3(normalizedLeftJoystickDir.getX(), 0.0f, -normalizedLeftJoystickDir.getY()) * leftJoystickTilt;
            }
        }
        else if (length(leftJoystickDir) > 0)
        {
            vec2 recoveryVector = normalize(leftJoystickDir) * k_vJoystickRecovery * deltaTime;
            if(length(recoveryVector) >= length(leftJoystickDir)) virtualLeftJoystickPos = k_vLeftJoystickCenter;
            else virtualLeftJoystickPos -= recoveryVector;
        }
        
        vec2 rightJoystickDir = virtualRightJoystickPos - k_vRightJoystickCenter;
        if(virtualRightJoysticPressed)
        {
            // Update view direction
            if(length(rightJoystickDir) > k_vJoystickDeadzone)
            {
                vec2 normalizedRightJoystickDir = normalize(rightJoystickDir);
                float rightJoystickTilt = length(rightJoystickDir) / k_vJoystickRange;
                viewRotation += vec2(normalizedRightJoystickDir.getY(), normalizedRightJoystickDir.getX()) * rightJoystickTilt * k_rotationSpeed;
            }
        }
        else if (length(rightJoystickDir) > 0)
        {
            vec2 recoveryVector = normalize(rightJoystickDir) * k_vJoystickRecovery * deltaTime;
            if(length(recoveryVector) >= length(rightJoystickDir)) virtualRightJoystickPos = k_vRightJoystickCenter;
            else virtualRightJoystickPos -= recoveryVector;
        }
#else
		if (InputSystem::IsMouseCaptured())
    {
#if defined(_DURANGO)
        // Update velocity vector
				moveVec += vec3(
                     Input::GetCurrentGamepadReading().NormalizedLeftThumbstickX(),
                     Input::GetCurrentGamepadReading().RightTrigger() - Input::GetCurrentGamepadReading().LeftTrigger(),
                     Input::GetCurrentGamepadReading().NormalizedLeftThumbstickY()
                     ) * k_movementSpeed;
        
        // Update view direction
        viewRotation += vec2(
                             Input::GetCurrentGamepadReading().NormalizedRightThumbstickY() * k_rotationSpeed,
                             Input::GetCurrentGamepadReading().NormalizedRightThumbstickX() * k_rotationSpeed);
#endif
#endif

		float lenS = lengthSqr(moveVec);
		if (lenS > 1.0f)
			moveVec *= sqrtf(lenS);
	}

#if MOVE_IN_PLANE
	mat4 rot =  mat4::rotationY(viewRotation.getY());
#else
	mat4 rot{ mat4::rotationYX(viewRotation.getY(), viewRotation.getX()) };
#endif

	vec3 accelVec = (rot * moveVec).getXYZ();


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

	moveVec = ((currentVelocity + newVelocity) * .5f) * deltaTime;
	viewPosition += moveVec;
	currentVelocity = newVelocity;
#ifndef _DURANGO
  float velocityX = currentVelocity.getX();
	ASSERT(NAN != velocityX);
#endif

	moveVec = { 0,0,0 };
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

#ifdef TARGET_IOS
vec2 FpsCameraController::getVirtualLeftJoystickPos() const
{
    return virtualLeftJoystickPos;
}

vec2 FpsCameraController::getVirtualRightJoystickPos() const
{
    return virtualRightJoystickPos;
}
#endif

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

