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



#include <metal_stdlib>
using namespace metal;

struct Fragment_Shader
{
    struct PsIn
    {
        float4 position [[position]];
        float2 texcoord;
    };
    struct Uniforms_uRootConstants
    {
        packed_float4 color;
        packed_float2 scaleBias;
    };
    constant Uniforms_uRootConstants & uRootConstants;
    texture2d<float> uTex;
    sampler uSampler;
    float4 main(PsIn input)
    {
        return uTex.sample(uSampler, (input).texcoord) * uRootConstants.color;
    };

    Fragment_Shader(constant Uniforms_uRootConstants & uRootConstants,texture2d<float> uTex,sampler uSampler)
        : uRootConstants(uRootConstants)
        , uTex(uTex)
        , uSampler(uSampler)
    {
    }
};

fragment float4 stageMain(
    Fragment_Shader::PsIn input                                        [[stage_in]],
    constant Fragment_Shader::Uniforms_uRootConstants& uRootConstants  [[buffer(0)]],
    texture2d<float> uTex                                              [[texture(0)]],
    sampler uSampler                                                   [[sampler(0)]]
)
{
    Fragment_Shader::PsIn input0;
    input0.position = float4(input.position.xyz, 1.0 / input.position.w);
    input0.texcoord = input.texcoord;
    Fragment_Shader main(uRootConstants, uTex, uSampler);
    return main.main(input0);
}
