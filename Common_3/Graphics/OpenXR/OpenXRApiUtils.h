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

#if defined(QUEST_VR)
#include "OpenXRApi.h"
#include "../../Utilities/Math/MathTypes.h"

void GetOpenXRViewMatrix(uint32_t eyeIndex, mat4* outMatrix);
void GetOpenXRProjMatrixPerspective(uint32_t eyeIndex, float zNear, float zFar, mat4* outMatrix);
void GetOpenXRProjMatrixPerspectiveReverseZ(uint32_t eyeIndex, float zNear, float zFar, mat4* outMatrix);
struct EyesFOV
{
    float4 mLeftEye; // Order is (left, right, up, down)
    float4 mRightEye;
};
EyesFOV GetOpenXRViewFovs();

#endif // QUEST_VR