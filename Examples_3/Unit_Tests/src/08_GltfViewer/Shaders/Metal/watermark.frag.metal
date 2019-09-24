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

struct Fragment_Shader
{
    texture2d<float> sceneTexture;
    sampler clampMiplessLinearSampler;
    struct VSOutput
    {
        float4 Position [[position]];
        float2 TexCoord;
    };
    float4 main(VSOutput input)
    {
        return sceneTexture.sample(clampMiplessLinearSampler, (input).TexCoord);
    };

    Fragment_Shader(
texture2d<float> sceneTexture,sampler clampMiplessLinearSampler) :
sceneTexture(sceneTexture),clampMiplessLinearSampler(clampMiplessLinearSampler) {}
};

struct SceneTexture
{
	texture2d<float> sceneTexture [[id(0)]];
	sampler clampMiplessLinearSampler [[id(1)]];
};

fragment float4 stageMain(
    Fragment_Shader::VSOutput input [[stage_in]],
	constant SceneTexture& argBufferStatic [[buffer(UPDATE_FREQ_NONE)]])
{
    Fragment_Shader::VSOutput input0;
    input0.Position = float4(input.Position.xyz, 1.0 / input.Position.w);
    input0.TexCoord = input.TexCoord;
    Fragment_Shader main(
    argBufferStatic.sceneTexture,
    argBufferStatic.clampMiplessLinearSampler);
    return main.main(input0);
}
