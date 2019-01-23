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

#include <metal_stdlib>
using namespace metal;

struct CameraData
{
    float4x4 viewMat;
    float4x4 projMat;
    float4x4 viewProjMat;
    float4x4 InvViewProjMat;

    float4 cameraWorldPos;
    float4 viewPortSize;
};
struct ObjectData
{
    float4x4 worldMat;
    float roughness;
    float metalness;
};

struct VSInput
{
    float3 position [[attribute(0)]];
    float2 uv   [[attribute(1)]];
};

struct VSOutput {
    float4 position [[position]];
    float2 uv;
};

vertex VSOutput stageMain(VSInput In                    [[stage_in]])
{
    VSOutput result;
    result.position = float4(In.position,1.0);
    result.uv= In.uv;
    return result;
}
