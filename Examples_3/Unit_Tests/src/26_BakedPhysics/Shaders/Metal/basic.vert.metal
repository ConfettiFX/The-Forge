/*
 * Copyright (c) 2018-2021 The Forge Interactive Inc.
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

/* Write your header comments here */
#include <metal_stdlib>
#include <metal_atomic>
using namespace metal;

inline float3x3 matrix_ctor(float4x4 m) {
        return float3x3(m[0].xyz, m[1].xyz, m[2].xyz);
}
struct Vertex_Shader
{
#define MAX_JOINTS 815

    struct Uniforms_uniformBlock {

        float4x4 mvp;
        float4 color[MAX_JOINTS];

        float4 lightPosition;
        float4 lightColor;
        float4x4 toWorld[MAX_JOINTS];
    };
    constant Uniforms_uniformBlock & uniformBlock;
    struct VSInput
    {
        float4 Position [[attribute(0)]];
        float4 Normal [[attribute(1)]];
    };

    struct VSOutput
    {

        float4 Position [[position]];
        float4 Color;
    };

    VSOutput main(VSInput input, uint InstanceID)
    {
        VSOutput result;
		float scaleFactor = 0.065f;
		float4x4 scaleMat = {{scaleFactor, 0.0, 0.0, 0.0}, {0.0f, scaleFactor, 0.0, 0.0}, {0.0f, 0.0, scaleFactor, 0.0}, {0.0f, 0.0, 0.0, 1.0f}};
		float4x4 tempMat = uniformBlock.mvp * uniformBlock.toWorld[InstanceID] * scaleMat;
		result.Position = ((tempMat)*(input.Position));

        float4 normal = normalize(((uniformBlock.toWorld[InstanceID])*(float4(input.Normal.xyz, 0.0))));
        float4 pos = ((uniformBlock.toWorld[InstanceID])*(float4(input.Position.xyz, 1.0)));

        float lightIntensity = 1.0;
		float ambientCoeff = 0.4;

        float3 lightDir = (float3)(normalize(uniformBlock.lightPosition.xyz - pos.xyz));
		
        float3 baseColor = uniformBlock.color[InstanceID].xyz;
        float3 blendedColor = ((uniformBlock.lightColor.xyz * baseColor)*(lightIntensity));
        float3 diffuse = ((blendedColor)*(max(dot(normal.xyz, lightDir), 0.0)));
        float3 ambient = ((baseColor)*(ambientCoeff));
        result.Color = float4(diffuse + ambient, 1.0);

        return result;
    };

    Vertex_Shader(constant Uniforms_uniformBlock & uniformBlock) : uniformBlock(uniformBlock) {}
};

vertex Vertex_Shader::VSOutput stageMain(
    Vertex_Shader::VSInput input                                 [[stage_in]],
    uint InstanceID                                              [[instance_id]],
    constant Vertex_Shader::Uniforms_uniformBlock & uniformBlock [[buffer(0)]]
)
{
    Vertex_Shader::VSInput input0;
    input0.Position = input.Position;
    input0.Normal = input.Normal;
    uint InstanceID0;
    InstanceID0 = InstanceID;
    Vertex_Shader main(uniformBlock);
    return main.main(input0, InstanceID0);
}
