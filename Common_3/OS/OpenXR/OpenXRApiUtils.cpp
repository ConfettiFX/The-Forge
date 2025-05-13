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

#include "../../Utilities/Math/MathTypes.h"

#include "openxr/openxr.h"

#define LEFT_EYE_VIEW  0
#define RIGHT_EYE_VIEW 1

extern "C"
{
    extern XrView GetXRView(uint32_t viewIndex);
}

void GetOpenXRViewMatrix(uint32_t eyeIndex, mat4* outMatrix)
{
#ifdef AUTOMATED_TESTING
    UNREF_PARAM(eyeIndex);
    // Need to keep the Quest headset absolutely static for screenshot testing.
    *outMatrix = mat4::identity();
#else
    XrView view = GetXRView(eyeIndex);
    vec3   pos = f3Tov3(*(float3*)&view.pose.position);
    Quat   orientation = *(Quat*)&view.pose.orientation;

    // Invert the roll to use left handed system
    orientation.setZ(-orientation.getZ());
    *outMatrix = mat4::translation(pos) * mat4::rotation(orientation);
#endif
}

void GetOpenXRProjMatrixPerspective(uint32_t eyeIndex, float zNear, float zFar, mat4* outMatrix)
{
    XrView view = GetXRView(eyeIndex);
    XrFovf fov = view.fov;
    *outMatrix = mat4::perspectiveLH_AsymmetricFov(-fov.angleLeft, fov.angleRight, fov.angleUp, -fov.angleDown, zNear, zFar, false);
}

void GetOpenXRProjMatrixPerspectiveReverseZ(uint32_t eyeIndex, float zNear, float zFar, mat4* outMatrix)
{
    XrView view = GetXRView(eyeIndex);
    XrFovf fov = view.fov;
    *outMatrix =
        mat4::perspectiveLH_ReverseZ_AsymmetricFov(-fov.angleLeft, fov.angleRight, fov.angleUp, -fov.angleDown, zNear, zFar, false);
}

void GetOpenXRViewFovs(float4& outLeftEye, float4& outRightEye)
{
    XrView leftView = GetXRView(LEFT_EYE_VIEW);
    XrView rightView = GetXRView(RIGHT_EYE_VIEW);
    outLeftEye = *(float4*)&leftView.fov;
    outRightEye = *(float4*)&rightView.fov;
}
