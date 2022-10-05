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

#ifndef Shadows_hpp
#define Shadows_hpp

#include "../../../Common_3/Utilities/Math/MathTypes.h"
#include "Camera.hpp"


void calculateShadowCascades(const PerspectiveProjection& viewFrustum, const mat4& viewMatrix, const mat4& lightView, int cascadeCount, mat4* cascadeProjections, mat4* cascadeTransforms, float* viewSize, uint32_t shadowMapResolution); 

#endif