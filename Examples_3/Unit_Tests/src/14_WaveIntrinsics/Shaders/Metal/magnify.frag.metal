/*
* Copyright (c) 2018-2020 The Forge Interactive Inc.
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
    struct Uniforms_SceneConstantBuffer
    {
        float4x4 orthProjMatrix;
        float2 mousePosition;
        float2 resolution;
        float time;
        uint renderMode;
        uint laneSize;
        uint padding;
    };
    constant Uniforms_SceneConstantBuffer & SceneConstantBuffer;
    struct PSInput
    {
        float4 position [[position]];
        float2 uv;
    };
    texture2d<float> g_texture;
    sampler g_sampler;
    float4 main(PSInput input)
    {
        float aspectRatio = (SceneConstantBuffer.resolution.x / SceneConstantBuffer.resolution.y);
        float magnifiedFactor = 6.0;
        float magnifiedAreaSize = 0.05;
        float magnifiedAreaBorder = 0.0050000000;
        float2 normalizedPixelPos = input.uv;
        float2 normalizedMousePos = (SceneConstantBuffer.mousePosition / SceneConstantBuffer.resolution);
        float2 diff = abs((normalizedPixelPos - normalizedMousePos));
        float4 color = g_texture.sample(g_sampler, normalizedPixelPos);
        if (((diff.x < (magnifiedAreaSize + magnifiedAreaBorder)) && (diff.y < ((magnifiedAreaSize + magnifiedAreaBorder) * aspectRatio))))
        {
            (color = float4(0.0, 1.0, 1.0, 1.0));
        }
        if (((diff.x < magnifiedAreaSize) && (diff.y < (magnifiedAreaSize * aspectRatio))))
        {
            (color = (float4)(g_texture.sample(g_sampler, (normalizedMousePos + ((normalizedPixelPos - normalizedMousePos) / (float2)(magnifiedFactor))))));
        }
        return color;
    };

    Fragment_Shader(
constant Uniforms_SceneConstantBuffer & SceneConstantBuffer,texture2d<float> g_texture,sampler g_sampler) :
SceneConstantBuffer(SceneConstantBuffer),g_texture(g_texture),g_sampler(g_sampler) {}
};

struct FSData {
    texture2d<float> g_texture [[id(0)]];
    sampler g_sampler [[id(1)]];
};

struct FSDataPerFrame {
    constant Fragment_Shader::Uniforms_SceneConstantBuffer& SceneConstantBuffer [[id(0)]];
};

fragment float4 stageMain(
    Fragment_Shader::PSInput input              [[stage_in]],
    constant FSData& fsData                     [[buffer(UPDATE_FREQ_NONE)]],
    constant FSDataPerFrame& fsDataPerFrame     [[buffer(UPDATE_FREQ_PER_FRAME)]]
)
{
    Fragment_Shader::PSInput input0;
    input0.position = float4(input.position.xyz, 1.0 / input.position.w);
    input0.uv = input.uv;
    Fragment_Shader main(fsDataPerFrame.SceneConstantBuffer, fsData.g_texture, fsData.g_sampler);
    return main.main(input0);
}
