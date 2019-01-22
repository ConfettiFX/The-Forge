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

struct Fragment_Shader
{
    struct VSOutput
    {
        float4 Position [[position]];
        float4 UV;
    };
    struct Uniforms_RootConstant
    {
        float axis;
    };
    constant Uniforms_RootConstant & RootConstant;
    texture2d<float> Source;
    sampler LinearSampler;
    float4 main(VSOutput input)
    {
        const int StepCount = 2;
        const float Weights[StepCount] = { 0.44908, 0.05092 };
        const float Offsets[StepCount] = { 0.53805, 2.0627799 };
        uint2 dim;
        dim[0] = Source.get_width();
        dim[1] = Source.get_height();
        float2 stepSize = float2(((1.0 - RootConstant.axis) / (float)(dim[0])), (RootConstant.axis / (float)(dim[1])));
        float4 output = 0.0;
        for (int i = 0; (i < StepCount); (++i))
        {
            float2 offset = ((float2)(Offsets[i]) * stepSize);
            (output += (float4)(Source.sample(LinearSampler, (input.UV.xy + offset)) * (float4)(Weights[i])));
            (output += (float4)(Source.sample(LinearSampler, (input.UV.xy - offset)) * (float4)(Weights[i])));
        }
        return output;
    };

    Fragment_Shader(
constant Uniforms_RootConstant & RootConstant,texture2d<float> Source,sampler LinearSampler) :
RootConstant(RootConstant),Source(Source),LinearSampler(LinearSampler) {}
};


fragment float4 stageMain(
    Fragment_Shader::VSOutput input [[stage_in]],
    constant Fragment_Shader::Uniforms_RootConstant & RootConstant [[buffer(0)]],
    texture2d<float> Source [[texture(0)]],
    sampler LinearSampler [[sampler(0)]])
{
    Fragment_Shader::VSOutput input0;
    input0.Position = float4(input.Position.xyz, 1.0 / input.Position.w);
    input0.UV = input.UV;
    Fragment_Shader main(
    RootConstant,
    Source,
    LinearSampler);
    return main.main(input0);
}
