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
using namespace metal;

struct Fragment_Shader
{
    struct VSOutput
    {
        float4 Position [[position]];
        float2 Tex_Coord;
    };
    sampler clampNearSampler;
    texture2d<float> screenTexture;
    float4 main(VSOutput input)
    {
        float rcolor = (float)(screenTexture.sample(clampNearSampler, (input).Tex_Coord).x);
        float3 color = float3(rcolor, rcolor, rcolor);
        return float4(color, 1.0);
    };

    Fragment_Shader(
sampler clampNearSampler,texture2d<float> screenTexture) :
clampNearSampler(clampNearSampler),screenTexture(screenTexture) {}
};

struct FSData {
    sampler clampNearSampler        [[id(0)]];
    texture2d<float> screenTexture  [[id(1)]];
};

fragment float4 stageMain(
    Fragment_Shader::VSOutput input [[stage_in]],
    constant FSData& fsData         [[buffer(UPDATE_FREQ_NONE)]]
)
{
    Fragment_Shader::VSOutput input0;
    input0.Position = float4(input.Position.xyz, 1.0 / input.Position.w);
    input0.Tex_Coord = input.Tex_Coord;
    Fragment_Shader main(fsData.clampNearSampler, fsData.screenTexture);
    return main.main(input0);
}
