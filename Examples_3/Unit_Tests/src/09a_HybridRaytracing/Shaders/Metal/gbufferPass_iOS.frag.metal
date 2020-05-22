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

    sampler samplerLinear;	
	
	texture2d<float, access::sample> albedoMap;
	texture2d<float, access::sample> normalMap;
	texture2d<float, access::sample> metallicMap;
	texture2d<float, access::sample> roughnessMap;
	texture2d<float, access::sample> aoMap;

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

        float3 albedo = ( albedoMap.sample(samplerLinear, input.uv)).rgb;

        float3 N = normalize(input.normal);

        Out.albedo = float4(albedo, 1);
        Out.normal = float4(N, 0);

        return Out;
    };

    Fragment_Shader(constant Uniforms_cbPerPass & cbPerPass,
    constant Uniforms_cbPerProp & cbPerProp,
    sampler samplerLinear,
    texture2d<float, access::sample> albedoMapIn) : cbPerPass(cbPerPass),
    cbPerProp(cbPerProp),
    samplerLinear(samplerLinear),
    albedoMap(albedoMapIn) {}
};

struct FSData
{
    constant Fragment_Shader::Uniforms_cbPerProp& cbPerProp [[id(0)]];
    sampler samplerLinear [[id(1)]];
};

struct FSDataPerFrame
{
    constant Fragment_Shader::Uniforms_cbPerPass & cbPerPass [[id(0)]];
};

struct FSDataPerDraw
{
	//    texture2d<float> textureMaps[5];
    texture2d<float, access::sample> albedoMap;
    texture2d<float, access::sample> normalMap;
    texture2d<float, access::sample> metallicMap;
    texture2d<float, access::sample> roughnessMap;
    texture2d<float, access::sample> aoMap;
};

fragment Fragment_Shader::PSOut stageMain( Fragment_Shader::PsIn 			input [[stage_in]],
                        constant FSData& fsData                     [[buffer(UPDATE_FREQ_NONE)]],
                        constant FSDataPerFrame& fsDataPerFrame     [[buffer(UPDATE_FREQ_PER_FRAME)]],
						constant FSDataPerDraw& fsDataPerDraw       [[buffer(UPDATE_FREQ_PER_DRAW)]]
)
{
/*
    texture2d<float, access::sample> albedoMap = textureMaps[0];
    texture2d<float, access::sample> normalMap = textureMaps[1];
    texture2d<float, access::sample> metallicMap = textureMaps[2];
    texture2d<float, access::sample> roughnessMap = textureMaps[3];
    texture2d<float, access::sample> aoMap = textureMaps[4];
*/
    Fragment_Shader::PsIn input0;
    input0.normal = input.normal;
    input0.pos = input.pos;
    input0.uv = input.uv;
    Fragment_Shader main(fsDataPerFrame.cbPerPass, fsData.cbPerProp, fsData.samplerLinear, fsDataPerDraw.albedoMap);
    return main.main(input0);
}
