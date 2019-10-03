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

constant const float NUM_SHADOW_SAMPLES_INV = 0.03125f;
constant const float shadowSamples[(32 * 2)] = { (-0.17466460), (-0.79131840), (-0.129792), (-0.44771160), 0.08863912, (-0.8981690), (-0.58914988), (-0.678163), 0.17484090, (-0.5252063), 0.6483325, (-0.752117), 0.45293192, (-0.384986), 0.09757467, (-0.1166954), 0.3857658, (-0.9096935), 0.56130584, (-0.1283066), 0.768011, (-0.4906538), 0.8499438, (-0.220937), 0.6946555, 0.16058660, 0.9614297, 0.0597522, 0.7986544, 0.53259124, 0.45139648, 0.5592551, 0.2847693, 0.2293397, (-0.2118996), (-0.1609127), (-0.4357893), (-0.3808875), (-0.4662672), (-0.05288446), (-0.139129), 0.23940650, 0.1781853, 0.5254948, 0.4287854, 0.899425, 0.12893490, 0.8724155, (-0.6924323), (-0.2203967), (-0.48997), 0.2795907, (-0.26117242), 0.7359962, (-0.7704172), 0.42331340, (-0.8501040), 0.12639350, (-0.83452672), (-0.499136), (-0.5380967), 0.6264234, (-0.9769312), (-0.15505689) };

struct Fragment_Shader
{
    struct Uniforms_ShadowUniformBuffer
    {
        float4x4 LightViewProj;
    };
    struct Uniforms_cbPerFrame
    {
        float4x4 worldMat;
        float4x4 projViewMat;
        float4 screenSize;
    };
    constant Uniforms_ShadowUniformBuffer & ShadowUniformBuffer;
    depth2d<float> ShadowTexture;
    sampler clampMiplessLinearSampler;
    struct VSOutput
    {
        float4 Position [[position]];
        float3 WorldPos;
        float2 TexCoord;
    };
    float random(float3 seed, float3 freq)
    {
        float dt = dot(floor((seed * freq)), float3((float)53.12149811, (float)21.1352005, (float)9.13220024));
        return fract((sin(dt) * (float)(2105.23535156)));
    };
    float CalcPCFShadowFactor(float3 worldPos)
    {
        float4 posLS = ((ShadowUniformBuffer.LightViewProj)*(float4((worldPos).xyz, (float)1.0)));
        (posLS /= (float4)((posLS).w));
        ((posLS).y *= (float)((-1)));
        ((posLS).xy = (((posLS).xy * (float2)(0.5)) + float2((float)0.5, (float)0.5)));
        float2 HalfGaps = float2((float)0.00048828124, (float)0.00048828124);
        //float2 Gaps = float2((float)0.0009765625, (float)0.0009765625);
        ((posLS).xy += HalfGaps);
        float shadowFactor = (float)(1.0);
        float shadowFilterSize = (float)(0.0016000000);
        float angle = random(worldPos, 20.0);
        float s = sin(angle);
        float c = cos(angle);
        for (int i = 0; (i < 32); (i++))
        {
            float2 offset = float2(shadowSamples[(i * 2)], shadowSamples[((i * 2) + 1)]);
            (offset = float2((((offset).x * c) + ((offset).y * s)), (((offset).x * (-s)) + ((offset).y * c))));
            (offset *= (float2)(shadowFilterSize));
            float shadowMapValue = (float)(ShadowTexture.sample(clampMiplessLinearSampler, ((posLS).xy + offset), level(0)));
            (shadowFactor += ((((shadowMapValue - 0.0020000000) > (posLS).z))?(0.0):(1.0)));
        }
        (shadowFactor *= NUM_SHADOW_SAMPLES_INV);
        return shadowFactor;
    };
    float4 main(VSOutput input)
    {
        float3 color = float3((float)1.0, (float)1.0, (float)1.0);
        (color *= (float3)(CalcPCFShadowFactor((input).WorldPos)));
        float i = 1.0f - sqrt(input.TexCoord.x * input.TexCoord.x + input.TexCoord.y * input.TexCoord.y);
        i = pow(saturate(i), 1.2);
        return float4((color).rgb, i);   
    };

    Fragment_Shader(
constant Uniforms_ShadowUniformBuffer & ShadowUniformBuffer,depth2d<float> ShadowTexture,sampler clampMiplessLinearSampler) :
ShadowUniformBuffer(ShadowUniformBuffer),ShadowTexture(ShadowTexture),clampMiplessLinearSampler(clampMiplessLinearSampler) {}
};

struct Scene
{
	depth2d<float> ShadowTexture [[id(0)]];
	sampler clampMiplessLinearSampler [[id(1)]];
};

struct PerFrame
{
	constant Fragment_Shader::Uniforms_ShadowUniformBuffer& ShadowUniformBuffer [[id(0)]];
	constant Fragment_Shader::Uniforms_cbPerFrame& cbPerFrame [[id(1)]];
};

fragment float4 stageMain(
    Fragment_Shader::VSOutput input [[stage_in]],
    constant Scene& argBufferStatic [[buffer(UPDATE_FREQ_NONE)]],
    constant PerFrame& argBufferPerFrame [[buffer(UPDATE_FREQ_PER_FRAME)]])
{
    Fragment_Shader::VSOutput input0;
    input0.Position = float4(input.Position.xyz, 1.0 / input.Position.w);
    input0.WorldPos = input.WorldPos;
    input0.TexCoord = input.TexCoord;
    Fragment_Shader main(
    argBufferPerFrame.ShadowUniformBuffer,
    argBufferStatic.ShadowTexture,
    argBufferStatic.clampMiplessLinearSampler);
    return main.main(input0);
}
