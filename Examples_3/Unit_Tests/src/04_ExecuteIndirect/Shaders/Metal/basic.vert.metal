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

struct InstanceData
{
    float4x4 mvp;
    float4x4 normalMat;
    float4 surfaceColor;
    float4 deepColor;
    int textureID;
    uint _pad0[3];
};

struct RootConstantData
{
    uint index;
};

struct VsIn
{
    float4 position [[attribute(0)]];
    float4 normal [[attribute(1)]];
};

struct PsIn
{
    float4 position [[position]];
    float3 posModel;
    float3 normal;
    float3 albedo;
};

float linstep(float min, float max, float s)
{
    return saturate((s - min) / (max - min));
}

vertex PsIn stageMain(VsIn In [[stage_in]],
                      constant InstanceData* instanceBuffer     [[buffer(1)]],
                      constant RootConstantData& rootConstant   [[buffer(2)]])
{
    PsIn result;
    result.position = instanceBuffer[rootConstant.index].mvp * float4(In.position.xyz, 1);
    result.posModel = In.position.xyz;
    result.normal = normalize((instanceBuffer[rootConstant.index].normalMat * float4(In.normal.xyz, 0)).xyz);

    float depth = linstep(0.5f, 0.7f, length(In.position.xyz));
    result.albedo = mix(instanceBuffer[rootConstant.index].deepColor.xyz, instanceBuffer[rootConstant.index].surfaceColor.xyz, depth);

    return result;
}
