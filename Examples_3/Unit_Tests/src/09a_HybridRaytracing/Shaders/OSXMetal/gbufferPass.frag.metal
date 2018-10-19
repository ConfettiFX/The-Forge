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
#include <metal_atomic>
using namespace metal;

inline float3x3 matrix_ctor(float4x4 m) {
        return float3x3(m[0].xyz, m[1].xyz, m[2].xyz);
}
struct Fragment_Shader
{

    struct Uniforms_cbPerPass {

        float4x4 projView;
    };
    constant Uniforms_cbPerPass & cbPerPass;
    struct Uniforms_cbPerProp {

        float4x4 world;
        float roughness;
        float metallic;
        int pbrMaterials;
        float pad;
    };
    constant Uniforms_cbPerProp & cbPerProp;
    struct Uniforms_cbTextureRootConstants {

        uint albedoMap;
        uint normalMap;
        uint metallicMap;
        uint roughnessMap;
        uint aoMap;
    };
    constant Uniforms_cbTextureRootConstants & cbTextureRootConstants;
    sampler samplerLinear;

    struct Uniforms_textureMaps {
         array<texture2d<float>, 128> Textures;
    };
    constant Uniforms_textureMaps & textureMaps;

    struct PsIn
    {
        float3 normal;
        float3 pos;
        float2 uv;
    };

    struct PSOut
    {
        float4 albedo [[color(0)]];
        float4 normal [[color(1)]];
    };

    PSOut main(PsIn input)
    {
		Fragment_Shader::PSOut Out;

        float3 albedo = ( textureMaps.Textures[cbTextureRootConstants.albedoMap].sample(samplerLinear, input.uv)).rgb;

        float3 N = normalize(input.normal);

        Out.albedo = float4(albedo, 1);
        Out.normal = float4(N, 0);

        return Out;
    };

    Fragment_Shader(constant Uniforms_cbPerPass & cbPerPass,
    constant Uniforms_cbPerProp & cbPerProp,
    constant Uniforms_cbTextureRootConstants & cbTextureRootConstants,
    sampler samplerLinear,
    constant Uniforms_textureMaps & textureMaps) : cbPerPass(cbPerPass),
    cbPerProp(cbPerProp),
    cbTextureRootConstants(cbTextureRootConstants),
    samplerLinear(samplerLinear),
    textureMaps(textureMaps) {}
};


fragment Fragment_Shader::PSOut stageMain(Fragment_Shader::PsIn input [[stage_in]],
  sampler samplerLinear [[sampler(0)]],
constant     Fragment_Shader::Uniforms_cbPerPass & cbPerPass [[buffer(1)]],
constant     Fragment_Shader::Uniforms_cbPerProp & cbPerProp [[buffer(2)]],
constant     Fragment_Shader::Uniforms_cbTextureRootConstants & cbTextureRootConstants [[buffer(3)]],
constant     Fragment_Shader::Uniforms_textureMaps & textureMaps [[buffer(4)]]) {
    Fragment_Shader::PsIn input0;
    input0.normal = input.normal;
    input0.pos = input.pos;
    input0.uv = input.uv;
    Fragment_Shader main(cbPerPass, cbPerProp, cbTextureRootConstants, samplerLinear, textureMaps);
        return main.main(input0);
}
