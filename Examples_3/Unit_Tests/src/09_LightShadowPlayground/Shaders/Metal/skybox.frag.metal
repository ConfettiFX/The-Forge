/*
* Copyright (c) 2018 Confetti Interactive Inc.
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

struct Uniforms_cameraUniformBlock
{
    float4x4 View;
    float4x4 Project;
    float4x4 ViewProject;
    float4x4 ViewInverse;
    float4x4 ProjectInverse;
};

struct Uniforms_renderSettingUniformBlock
{
    float4 WindowDimension;
    int ShadowType;
};
	
struct PsIn
{
    float4 Position [[position]];
};
struct PsOut
{
    float4 Color [[color(0)]];
};


fragment PsOut stageMain(
    float4 Position [[position]],
    constant Uniforms_cameraUniformBlock & cameraUniformBlock [[buffer(1)]],
    constant Uniforms_renderSettingUniformBlock & renderSettingUniformBlock [[buffer(2)]],
    texturecube<float> Skybox [[texture(0)]],
    sampler skySampler [[sampler(0)]])
{
    //float3 uvw = ((normalize((((float4((((float2(((input).Position).xy) * float2(2.0, (-2.0))) / (renderSettingUniformBlock.WindowDimension).xy) + float2((-1.0), 1.0)), 1.0, 1.0))*(cameraUniformBlock.ProjectInverse))).xyz))*(transpose(float3x3((cameraUniformBlock.View[0]).xyz, (cameraUniformBlock.View[1]).xyz, (cameraUniformBlock.View[2]).xyz))));
    float3 uvw = transpose(float3x3(cameraUniformBlock.View[0].xyz, cameraUniformBlock.View[1].xyz, cameraUniformBlock.View[2].xyz))*normalize((cameraUniformBlock.ProjectInverse*float4(float2(Position.xy)*float2(2.0,-2.0)/renderSettingUniformBlock.WindowDimension.xy+float2(-1.0,1.0),1.0,1.0)).xyz);
	PsOut output;
    output.Color = (float4)(Skybox.sample(skySampler, uvw));
    return output;
}
