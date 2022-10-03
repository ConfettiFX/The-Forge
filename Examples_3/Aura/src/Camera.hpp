/*
 * Copyright (c) 2017-2022 The Forge Interactive Inc.
 *
 * This is a part of Aura.
 * 
 * This file(code) is licensed under a 
 * Creative Commons Attribution-NonCommercial 4.0 International License 
 *
 *   (https://creativecommons.org/licenses/by-nc/4.0/legalcode) 
 *
 * Based on a work at https://github.com/ConfettiFX/The-Forge.
 * You may not use the material for commercial purposes.
 *
*/

#ifndef Camera_hpp
#define Camera_hpp

#include "../../../Common_3/Application/Interfaces/ICameraController.h"

typedef struct PerspectiveProjection
{
	float  mFovY;
	float  mAspectRatio;
	float  mNear;
	float  mFar;
} PerspectiveProjection;

typedef struct OrthographicProjection
{
	float  mLeft;
	float  mRight;
	float  mTop;
	float  mBottom;
	float  mNear;
	float  mFar;
} OrthographicProjection;

typedef struct Camera {
	ICameraController* pCameraController;
	PerspectiveProjection mProjection;
} Camera;

#endif
