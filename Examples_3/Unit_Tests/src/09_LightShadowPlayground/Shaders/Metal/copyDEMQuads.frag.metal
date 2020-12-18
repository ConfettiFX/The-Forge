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
        float2 UV;
        float4 MiscData;
    };
    sampler clampToEdgeNearSampler;
    texture2d<float> DepthPassTexture;
    float main(VSOutput input)
    {
        float tileDepth = (float)(DepthPassTexture.sample(clampToEdgeNearSampler, (input).UV, level(0.0))).r;
        return tileDepth;
    };

    Fragment_Shader(
sampler clampToEdgeNearSampler,texture2d<float> DepthPassTexture) :
clampToEdgeNearSampler(clampToEdgeNearSampler),DepthPassTexture(DepthPassTexture) {}
};

struct FSData {
    sampler clampToEdgeNearSampler      [[id(0)]];
    texture2d<float> DepthPassTexture   [[id(1)]];
};

fragment float4 stageMain(
    Fragment_Shader::VSOutput input [[stage_in]],
    constant FSData& fsData         [[buffer(UPDATE_FREQ_NONE)]]
)
{
    Fragment_Shader::VSOutput input0;
    input0.Position = float4(input.Position.xyz, 1.0 / input.Position.w);
    input0.UV = input.UV;
    input0.MiscData = input.MiscData;
    Fragment_Shader main(fsData.clampToEdgeNearSampler, fsData.DepthPassTexture);
    return main.main(input0);
}
