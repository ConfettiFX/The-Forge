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
// Include this file as last include in all cpp files allocating memory
#include "../Interfaces/IMemoryManager.h"

static const float k_mouseTranslationScale = 0.05f;
static const float k_rotationSpeed = 0.003f;
static const float k_scrollSpeed = -5.0f;
static const float k_xRotLimit = (float)(M_PI_2 - 0.1);

static const float k_vJoystickIntRadius = 0.25f;
static const float k_vJoystickExtRadius = 0.33f;
static const float k_vJoystickRange = k_vJoystickExtRadius - k_vJoystickIntRadius;
static const float k_vJoystickDeadzone = k_vJoystickRange / 10.0f;
static const float k_vJoystickRecovery = k_vJoystickRange * 8.0f;

static const vec2 k_vLeftJoystickCenter = vec2(0.175f, 0.75f);
static const vec2 k_vRightJoystickCenter = vec2(0.80f, 0.75f);

class GuiCameraController : public ICameraController
{
public:
	GuiCameraController() :
		viewPosition { 0 },
		velocity { 0 },
		viewRotation { 0 },
		maxSpeed { 1.0f },
#ifdef TARGET_IOS
        virtualLeftJoystickPos { k_vLeftJoystickCenter },
        virtualLeftJoysticPressed { false },
        virtualRightJoystickPos { k_vRightJoystickCenter },
        virtualRightJoysticPressed { false },
#endif
		mouseButtons { false }
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
#ifndef _DURANGO
	void onMouseMove(const RawMouseMoveEventData* pData) override;
	void onMouseButton(const MouseButtonEventData* pData) override;
	void onMouseWheel(const MouseWheelEventData* pData) override;
#endif
#if defined(TARGET_IOS)
    void onTouch(const TouchEventData *pData) override;
    void onTouchMove(const TouchEventData *pData) override;
#endif
	void update(float deltaTime) override;

private:
	vec2 viewRotation;//We put viewRotation at first becuase viewPosition is 16 bytes aligned. We have vtable pointer 8 bytes + vec2(8 bytes). This avoids unnecessary padding.
	vec3 viewPosition;
	vec3 velocity;
	float maxSpeed;
	bool mouseButtons[MOUSE_BUTTON_COUNT];
    
#ifdef TARGET_IOS
    vec2 virtualLeftJoystickPos;
    bool virtualLeftJoysticPressed;
    vec2 virtualRightJoystickPos;
    bool virtualRightJoysticPressed;
#endif
};

ICameraController* createGuiCameraController(vec3 startPosition, vec3 startLookAt)
{
	GuiCameraController* cc = conf_placement_new<GuiCameraController>(conf_calloc(1, sizeof(GuiCameraController)));
	cc->moveTo(startPosition);
	cc->lookAt(startLookAt);
	return cc;
}

void destroyGuiCameraController(ICameraController* pCamera)
{
	pCamera->~ICameraController();
	conf_free(pCamera);
}

void GuiCameraController::setMotionParameters(const CameraMotionParameters& cmp)
{
	maxSpeed = cmp.maxSpeed;
}

#ifndef _DURANGO
void GuiCameraController::onMouseMove(const RawMouseMoveEventData* pData)
{
	if (mouseButtons[MOUSE_LEFT] && mouseButtons[MOUSE_RIGHT])
	{
		vec3 move { (float)-pData->x, (float)pData->y, 0 };
		mat4 rot{ mat4::rotationYX(viewRotation.getY(), viewRotation.getX()) };
		velocity = (rot * (move * maxSpeed * k_mouseTranslationScale)).getXYZ();
	}
	else if (mouseButtons[MOUSE_LEFT])
	{
		vec3 move { (float)pData->x, 0, (float)pData->y };
		mat4 rot{ mat4::rotationYX(viewRotation.getY(), viewRotation.getX()) };
		//velocity = rotateV3(rot, move * maxSpeed * k_mouseTranslationScale);
		velocity = (rot * (move * maxSpeed * k_mouseTranslationScale)).getXYZ();
	}
	else if (mouseButtons[MOUSE_RIGHT])
	{
		viewRotation += { pData->y * k_rotationSpeed, pData->x * k_rotationSpeed };
	}

	if (viewRotation.getX() > k_xRotLimit)
		viewRotation.setX(k_xRotLimit);
	else if (viewRotation.getX() < -k_xRotLimit)
		viewRotation.setX(-k_xRotLimit);
}

void GuiCameraController::onMouseButton(const MouseButtonEventData* pData)
{
	mouseButtons[pData->button] = pData->pressed;
}

void GuiCameraController::onMouseWheel(const MouseWheelEventData* pData)
{
	mat4 m{ mat4::rotationYX(viewRotation.getY(), viewRotation.getX()) };
	const vec3& v { m.getCol2().getXYZ() };
	viewPosition -= v * ((float)pData->scroll * k_scrollSpeed);
}
#endif

#if defined(TARGET_IOS)
void GuiCameraController::onTouch(const TouchEventData *pData)
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

void GuiCameraController::onTouchMove(const TouchEventData *pData)
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
    else virtualLeftJoysticPressed = false;
    
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
    else virtualRightJoysticPressed = false;
}
#endif

void GuiCameraController::update(float deltaTime)
{
    // TODO: Implement GUI controller behaviour iOS/Durango.
#ifdef TARGET_IOS
    {
        vec2 leftJoystickDir = virtualLeftJoystickPos - k_vLeftJoystickCenter;
        if(virtualLeftJoysticPressed)
        {
            
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
            
        }
        else if (length(rightJoystickDir) > 0)
        {
            vec2 recoveryVector = normalize(rightJoystickDir) * k_vJoystickRecovery * deltaTime;
            if(length(recoveryVector) >= length(rightJoystickDir)) virtualRightJoystickPos = k_vRightJoystickCenter;
            else virtualRightJoystickPos -= recoveryVector;
        }
    }
#endif
    
	viewPosition += velocity * deltaTime;
	velocity = vec3{ 0 };
}

mat4 GuiCameraController::getViewMatrix() const
{
	mat4 r { mat4::rotationXY(-viewRotation.getX(), -viewRotation.getY()) };
	vec4 t = r * vec4(-viewPosition, 1.0f);
	r.setTranslation(t.getXYZ());
	return r;
}

vec3 GuiCameraController::getViewPosition() const
{
	return viewPosition;
}

#ifdef TARGET_IOS
vec2 GuiCameraController::getVirtualLeftJoystickPos() const
{
    return virtualLeftJoystickPos;
}

vec2 GuiCameraController::getVirtualRightJoystickPos() const
{
    return virtualRightJoystickPos;
}
#endif

void GuiCameraController::moveTo(const vec3& location)
{
	viewPosition = location;
}

void GuiCameraController::lookAt(const vec3& lookAt)
{
	vec3 lookDir = normalize(lookAt - viewPosition);

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
