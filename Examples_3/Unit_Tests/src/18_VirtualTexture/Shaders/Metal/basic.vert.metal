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

// Shader for simple shading with a point light
// for planets in Unit Test 01 - Transformations

#include <metal_stdlib>
using namespace metal;

#define MAX_PLANETS 20

struct UniformBlock0
{
	float4x4 mvp;
    float4x4 toWorld[MAX_PLANETS];
    float4 color[MAX_PLANETS];

    // Point Light Information
    packed_float3 lightPosition;
    float _pad1;  // vec3 in the c++ side are detected as 4 x floats (due to simd use). We add padding here to match the struct sizes.
    packed_float3 lightColor;
    float _pad2;  // vec3 in the c++ side are detected as 4 x floats (due to simd use). We add padding here to match the struct sizes.
};

struct VSInput
{
    float4 Position [[attribute(0)]];
    float4 Normal   [[attribute(1)]];
};

struct VSOutput {
	float4 Position [[position]];
    float4 Color;
};

struct VSData {
    constant UniformBlock0& uniformBlock [[id(0)]];
};

vertex VSOutput stageMain(VSInput input             [[stage_in]],
                       uint InstanceID              [[instance_id]],
                       constant VSData& vsData    [[buffer(UPDATE_FREQ_PER_FRAME)]]
)
{
    VSOutput result;
    float4x4 tempMat = vsData.uniformBlock.mvp * vsData.uniformBlock.toWorld[InstanceID];
    result.Position = tempMat * input.Position;
    
    float4 normal = normalize(vsData.uniformBlock.toWorld[InstanceID] * float4(input.Normal.xyz, 0.0f)); // Assume uniform scaling
    float4 pos = vsData.uniformBlock.toWorld[InstanceID] * float4(input.Position.xyz, 1.0f);

    float lightIntensity = 1.0f;
    float ambientCoeff = 0.4;

    float3 lightDir;

    if (vsData.uniformBlock.color[InstanceID].w == 0) // Special case for Sun, so that it is lit from its top
        lightDir = float3(0.0f, 1.0f, 0.0f);
    else
        lightDir = normalize(vsData.uniformBlock.lightPosition - pos.xyz);

    float3 baseColor = vsData.uniformBlock.color[InstanceID].xyz;
    float3 blendedColor = vsData.uniformBlock.lightColor * baseColor * lightIntensity;
    float3 diffuse = blendedColor * max(dot(normal.xyz, lightDir), 0.0);
    float3 ambient = baseColor * ambientCoeff;
    result.Color = float4(diffuse + ambient, 1.0);
    return result;
}
