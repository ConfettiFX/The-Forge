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
#include "../../../Middleware_3/Input/InputSystem.h"
#include "../../../Middleware_3/Input/InputMappings.h"
#include "../../../Middleware_3/UI/AppUI.h"

// Include this file as last include in all cpp files allocating memory
#include "../Interfaces/IMemoryManager.h"

static const float k_mouseTranslationScale = 0.05f;
static const float k_rotationSpeed = 0.003f;
static const float k_xRotLimit = (float)(M_PI_2 - 0.1);

static const vec2 k_vLeftJoystickCenter = vec2(0.175f, 0.75f);
static const vec2 k_vRightJoystickCenter = vec2(0.80f, 0.75f);

class GuiCameraController: public ICameraController
{
	public:
	GuiCameraController(): viewRotation{ 0 }, viewPosition{ 0 }, velocity{ 0 }, maxSpeed{ 1.0f }, pVirtualJoystickUI{ NULL } {}
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
	vec2
					   viewRotation;    //We put viewRotation at first becuase viewPosition is 16 bytes aligned. We have vtable pointer 8 bytes + vec2(8 bytes). This avoids unnecessary padding.
	vec3               viewPosition;
	vec3               velocity;
	float              maxSpeed;
	VirtualJoystickUI* pVirtualJoystickUI;
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

void GuiCameraController::setVirtualJoystick(VirtualJoystickUI* virtualJoystick) {}

void GuiCameraController::setMotionParameters(const CameraMotionParameters& cmp) { maxSpeed = cmp.maxSpeed; }

bool GuiCameraController::onInputEvent(const ButtonData* pData)
{
	if (InputSystem::IsButtonPressed(KEY_CONFIRM) && InputSystem::IsButtonPressed(KEY_RIGHT_BUMPER))
	{
		vec3 move{ (float)-pData->mValue[0], (float)pData->mValue[1], 0 };
		mat4 rot{ mat4::rotationYX(viewRotation.getY(), viewRotation.getX()) };
		velocity = (rot * (move * maxSpeed * k_mouseTranslationScale)).getXYZ();
	}
	else if (InputSystem::IsButtonPressed(KEY_LEFT_STICK))
	{
		vec3 move{ (float)pData->mValue[0], 0, (float)pData->mValue[1] };
		mat4 rot{ mat4::rotationYX(viewRotation.getY(), viewRotation.getX()) };
		//velocity = rotateV3(rot, move * maxSpeed * k_mouseTranslationScale);
		velocity = (rot * (move * maxSpeed * k_mouseTranslationScale)).getXYZ();
	}
	else if (InputSystem::IsButtonPressed(KEY_RIGHT_STICK))
	{
		viewRotation += { pData->mValue[0] * k_rotationSpeed, pData->mValue[1] * k_rotationSpeed };
	}

	if (viewRotation.getX() > k_xRotLimit)
		viewRotation.setX(k_xRotLimit);
	else if (viewRotation.getX() < -k_xRotLimit)
		viewRotation.setX(-k_xRotLimit);

	return false;
}
#if 0
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
//	// Find the closest touches to the left and right virtual joysticks.
//	float minDistLeft = 1.0f;
//	int closestTouchLeft = -1;
//	float minDistRight = 1.0f;
//	int closestTouchRight = -1;
//	for (uint32_t i = 0; i < pData->touchesRecorded; i++)
//	{
//		vec2 touchPos = vec2(pData->touchData[i].screenX, pData->touchData[i].screenY);
//
//		float distToLeft = length(touchPos - virtualLeftJoystickPos);
//		if(distToLeft < minDistLeft && distToLeft <= (k_vJoystickIntRadius * 0.33f)) { minDistLeft = distToLeft; closestTouchLeft = i; }
//		float distToRight = length(touchPos - virtualRightJoystickPos);
//		if(distToRight < minDistRight && distToRight <= (k_vJoystickIntRadius * 0.33f)) { minDistRight = distToRight; closestTouchRight = i; }
//	}
//
//	// Calculate the new joystick positions.
//	if(closestTouchLeft >= 0)
//	{
//		TouchData touch = pData->touchData[closestTouchLeft];
//		vec2 newPos = virtualLeftJoystickPos - vec2(touch.screenDeltaX, touch.screenDeltaY);
//
//		// Clamp the joystick's position to the max range.
//		vec2 tiltDir = newPos - k_vLeftJoystickCenter;
//		if(length(tiltDir) > k_vJoystickRange) newPos = k_vLeftJoystickCenter + normalize(tiltDir) * k_vJoystickRange;
//
//		virtualLeftJoystickPos = newPos;
//		virtualLeftJoysticPressed = true;
//	}
//	else virtualLeftJoysticPressed = false;
//
//	if(closestTouchRight >= 0)
//	{
//		TouchData touch = pData->touchData[closestTouchRight];
//		vec2 newPos = virtualRightJoystickPos - vec2(touch.screenDeltaX, touch.screenDeltaY);
//
//		// Clamp the joystick's position to the max range.
//		vec2 tiltDir = newPos - k_vRightJoystickCenter;
//		if(length(tiltDir) > k_vJoystickRange) newPos = k_vRightJoystickCenter + normalize(tiltDir) * k_vJoystickRange;
//
//		virtualRightJoystickPos = newPos;
//		virtualRightJoysticPressed = true;
//	}
//	else virtualRightJoysticPressed = false;
}
#endif

void GuiCameraController::update(float deltaTime)
{
	viewPosition += velocity * deltaTime;
	velocity = vec3{ 0 };
}

mat4 GuiCameraController::getViewMatrix() const
{
	mat4 r{ mat4::rotationXY(-viewRotation.getX(), -viewRotation.getY()) };
	vec4 t = r * vec4(-viewPosition, 1.0f);
	r.setTranslation(t.getXYZ());
	return r;
}

vec3 GuiCameraController::getViewPosition() const { return viewPosition; }

void GuiCameraController::moveTo(const vec3& location) { viewPosition = location; }

void GuiCameraController::lookAt(const vec3& lookAt)
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
