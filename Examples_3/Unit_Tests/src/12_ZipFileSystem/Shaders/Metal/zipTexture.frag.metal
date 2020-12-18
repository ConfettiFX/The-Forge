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
        float4 Position;
        float2 texcoords;
    };
    sampler uSamplerZip;
    texture2d<float> ZipTexture;
    float4 main(VSOutput input)
    {
        return ZipTexture.sample(uSamplerZip, (input).texcoords);
    };

    Fragment_Shader(
sampler uSamplerZip,texture2d<float> ZipTexture) :
uSamplerZip(uSamplerZip),ZipTexture(ZipTexture) {}
};

struct main_input
{
    float4 SV_POSITION [[position]];
    float2 TEXCOORD;
};

fragment float4 stageMain(
	main_input inputData [[stage_in]],
	texture2d<float,access::sample> ZipTexture  [[texture(6)]],
    sampler uSampler0                           [[sampler(0)]]
)
{
    Fragment_Shader::VSOutput input0;
    input0.Position = inputData.SV_POSITION;
    input0.texcoords = inputData.TEXCOORD;
    Fragment_Shader main(
    uSampler0,
    ZipTexture);
    return main.main(input0);
}
