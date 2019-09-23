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

struct UniformBlockData
{
    float4x4 viewProj;
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
    float4 albedo;
};

struct AsteroidDynamic
{
	float4x4 transform;
    uint indexStart;
    uint indexEnd;
    uint padding[2];
};

struct AsteroidStatic
{
	float4 rotationAxis;
	float4 surfaceColor;
	float4 deepColor;

	float scale;
	float orbitSpeed;
	float rotationSpeed;

    uint textureID;
    uint vertexStart;
    uint padding[3];
};

struct VSData {
    constant AsteroidStatic* asteroidsStatic     [[id(0)]];
    constant AsteroidDynamic* asteroidsDynamic   [[id(1)]];
};

struct VSDataPerFrame {
    constant UniformBlockData& uniformBlock      [[id(0)]];
};

vertex PsIn stageMain(VsIn In                                       [[stage_in]],
                      uint instanceID                               [[instance_id]],
                      constant VSData& vsData                       [[buffer(UPDATE_FREQ_NONE)]],
                      constant VSDataPerFrame& vsDataPerFrame       [[buffer(UPDATE_FREQ_PER_FRAME)]]
)
{
    
    PsIn result;

    AsteroidStatic asteroidStatic = vsData.asteroidsStatic[instanceID];
    AsteroidDynamic asteroidDynamic = vsData.asteroidsDynamic[instanceID];

    float4x4 worldMatrix = asteroidDynamic.transform;
    result.position = vsDataPerFrame.uniformBlock.viewProj * (worldMatrix * float4(In.position.xyz, 1.0f));
    result.posModel = In.position.xyz;
    result.normal = (worldMatrix * float4(In.normal.xyz,0)).xyz;

    float depth = saturate((length(In.position.xyz) - 0.5f) / 0.2);
    result.albedo.xyz = mix(asteroidStatic.deepColor.xyz, asteroidStatic.surfaceColor.xyz, depth);
    result.albedo.w = (float)asteroidStatic.textureID;

    return result;
}
