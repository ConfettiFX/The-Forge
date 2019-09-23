/*
 * Copyright (c) 2018 Kostas Anagnostou (https://twitter.com/KostasAAA).
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

struct Vertex_Shader
{
    struct Uniforms_ShadowUniformBuffer
    {
        float4x4 LightViewProj;
    };
    struct Uniforms_cbPerFrame
    {
        float4x4 worldMat;
        float4x4 projViewMat;
        float4 screenSize;
    };
    constant Uniforms_cbPerFrame & cbPerFrame;
    struct VSInput
    {
        float3 Position [[attribute(0)]];
        float2 TexCoord [[attribute(1)]];
    };
    struct VSOutput
    {
        float4 Position [[position]];
        float3 WorldPos;
        float2 TexCoord;
    };
    VSOutput main(VSInput input)
    {
        VSOutput Out;
        float4 worldPos = float4((input).Position, 1.0);
        (worldPos = ((cbPerFrame.worldMat)*(worldPos)));
        ((Out).Position = ((cbPerFrame.projViewMat)*(worldPos)));
        ((Out).WorldPos = (worldPos).xyz);
        ((Out).TexCoord = (input).TexCoord);
        return Out;
    };

    Vertex_Shader(
constant Uniforms_cbPerFrame & cbPerFrame) :
cbPerFrame(cbPerFrame) {}
};

struct PerFrame
{
	constant Vertex_Shader::Uniforms_ShadowUniformBuffer& ShadowUniformBuffer [[id(0)]];
	constant Vertex_Shader::Uniforms_cbPerFrame& cbPerFrame [[id(1)]];
};

vertex Vertex_Shader::VSOutput stageMain(
    Vertex_Shader::VSInput input [[stage_in]],
    constant PerFrame& argBufferPerFrame [[buffer(UPDATE_FREQ_PER_FRAME)]])
{
    Vertex_Shader::VSInput input0;
    input0.Position = input.Position;
    input0.TexCoord = input.TexCoord;
    Vertex_Shader main(
    argBufferPerFrame.cbPerFrame);
    return main.main(input0);
}
