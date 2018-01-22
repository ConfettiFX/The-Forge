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

class GuiCameraController : public ICameraController
{
public:
	GuiCameraController() :
		viewPosition { 0 },
		velocity { 0 },
		viewRotation { 0 },
		maxSpeed { 1.0f },
		mouseButtons { false }
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
	vec3 viewPosition;
	vec3 velocity;
	vec2 viewRotation;

	float maxSpeed;
	bool mouseButtons[MOUSE_BUTTON_COUNT];
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
void GuiCameraController::onMouseMove(const MouseMoveEventData* pData)
{
	if (mouseButtons[MOUSE_LEFT] && mouseButtons[MOUSE_RIGHT])
	{
		vec3 move { (float)-pData->deltaX, (float)pData->deltaY, 0 };
		mat4 rot{ mat4::rotationYX(viewRotation.getY(), viewRotation.getX()) };
		velocity = (rot * (move * maxSpeed * k_mouseTranslationScale)).getXYZ();
	}
	else if (mouseButtons[MOUSE_LEFT])
	{
		vec3 move { (float)pData->deltaX, 0, (float)pData->deltaY };
		mat4 rot{ mat4::rotationYX(viewRotation.getY(), viewRotation.getX()) };
		//velocity = rotateV3(rot, move * maxSpeed * k_mouseTranslationScale);
		velocity = (rot * (move * maxSpeed * k_mouseTranslationScale)).getXYZ();
	}
	else if (mouseButtons[MOUSE_RIGHT])
	{
		viewRotation += { pData->deltaY * k_rotationSpeed, pData->deltaX * k_rotationSpeed };
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

void GuiCameraController::update(float deltaTime)
{
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
