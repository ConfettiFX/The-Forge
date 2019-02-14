/*
* Copyright (c) 2018 Confetti Interactive Inc.
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
#define PI 3.14159274
#define PI_2 3.14159274*2.0
#define SHADOW_TYPE_NONE 0
#define SHADOW_TYPE_ESM 1
#define SHADOW_TYPE_SDF 2
    struct Uniforms_ESMInputConstants
    {
        float Near;
        float Far;
        float FarNearDiff;
        float Near_Over_FarNearDiff;
        float Exponent;
    };
    constant Uniforms_ESMInputConstants & ESMInputConstants;
    struct Uniforms_lightUniformBlock
    {
        float4x4 lightViewProj;
        float4 lightPosition;
        float4 lightColor;
    };
    constant Uniforms_lightUniformBlock & lightUniformBlock;
    struct Uniforms_renderSettingUniformBlock
    {
        float4 WindowDimension;
        int ShadowType;
    };
    constant Uniforms_renderSettingUniformBlock & renderSettingUniformBlock;
    struct Uniforms_cameraUniformBlock
    {
        float4x4 View;
        float4x4 Project;
        float4x4 ViewProject;
        float4x4 InvView;
    };
    constant Uniforms_cameraUniformBlock & cameraUniformBlock;
    texture2d<float> shadowTexture;
    sampler clampMiplessSampler;
    texture2d<float> SphereTex;
    sampler textureSampler;
    texture2d<float> PlaneTex;
    struct PsIn
    {
        float3 WorldPositionIn;
        float3 ColorIn;
        float3 NormalIn;
        int IfPlaneIn[[flat]];
        float4 Position [[position]];
    };
    struct PsOut
    {
        float4 FinalColor [[color(0)]];
    };
    float map_01(float depth)
    {
        return ((depth - ESMInputConstants.Near) / ESMInputConstants.FarNearDiff);
    };
    float calcESMShadowFactor(float3 worldPos)
    {
        float4 shadowCoord = lightUniformBlock.lightViewProj * float4(worldPos, 1.0);
        float2 shadowUV = (((shadowCoord).xy / (float2((shadowCoord).w, (-(shadowCoord).w)) * (const float2)(2.0))) + float2(0.5, 0.5));
        float lightMappedExpDepth = shadowTexture.sample(clampMiplessSampler, shadowUV).x;
        float pixelMappedDepth = map_01((shadowCoord).w);
        float shadowFactor = (lightMappedExpDepth * exp2(((-ESMInputConstants.Exponent) * pixelMappedDepth)));
        return clamp(shadowFactor, 0.0, 1.0);
    };
    float3 calcSphereTexColor(float3 worldNormal)
    {
        float2 uv = float2(0.0);
        ((uv).x = ((asin((worldNormal).x) / PI) + 0.5));
        ((uv).y = ((asin((worldNormal).y) / PI) + 0.5));
        return SphereTex.sample(textureSampler, uv).xyz;
    };
    PsOut main(PsIn input)
    {
        float shadowFactor = 1.0;
        if ((renderSettingUniformBlock.ShadowType == SHADOW_TYPE_ESM))
        {
            (shadowFactor = calcESMShadowFactor((input).WorldPositionIn));
        }
        else if ((renderSettingUniformBlock.ShadowType == SHADOW_TYPE_SDF))
        {
            (shadowFactor = (float)(shadowTexture.read(uint3(uint2(input.Position.xy), 0).xy, 0).x));
        }
        float3 albedo = input.ColorIn;
        float3 normal = normalize(input.NormalIn);
        if (input.IfPlaneIn == 0)
        {
            albedo *= calcSphereTexColor(normal);
        }
        else
        {
            albedo *= (float3)(PlaneTex.sample(textureSampler, ((input).WorldPositionIn).xz).xyz);
        }
        float3 lightVec = normalize(lightUniformBlock.lightPosition.xyz - input.WorldPositionIn);
        float3 viewVec = normalize(input.WorldPositionIn - cameraUniformBlock.InvView[3].xyz);
        float dotP = max(dot(normal, lightVec), 0.0);
        float3 diffuse = (albedo * (float3)(max(dotP, 0.05)));
        const float specularExp = 10.0;
        float specular = pow((max(dot(reflect(lightVec, normal), viewVec), 0.0) * dotP), specularExp);
        float3 finalColor = clamp(diffuse + (float3)(specular * 0.5), 0, 1);
        finalColor *= (lightUniformBlock.lightColor.xyz * (float3)(shadowFactor));
        PsOut output;
        ((output).FinalColor = float4(finalColor, 1.0));
        return output;
    };

    Fragment_Shader(
constant Uniforms_ESMInputConstants & ESMInputConstants,constant Uniforms_lightUniformBlock & lightUniformBlock,constant Uniforms_renderSettingUniformBlock & renderSettingUniformBlock,constant Uniforms_cameraUniformBlock & cameraUniformBlock,texture2d<float> shadowTexture,sampler clampMiplessSampler,texture2d<float> SphereTex,sampler textureSampler,texture2d<float> PlaneTex) :
ESMInputConstants(ESMInputConstants),lightUniformBlock(lightUniformBlock),renderSettingUniformBlock(renderSettingUniformBlock),cameraUniformBlock(cameraUniformBlock),shadowTexture(shadowTexture),clampMiplessSampler(clampMiplessSampler),SphereTex(SphereTex),textureSampler(textureSampler),PlaneTex(PlaneTex) {}
};


fragment Fragment_Shader::PsOut stageMain(
    Fragment_Shader::PsIn input [[stage_in]],
    constant Fragment_Shader::Uniforms_ESMInputConstants & ESMInputConstants [[buffer(2)]],
    constant Fragment_Shader::Uniforms_lightUniformBlock & lightUniformBlock [[buffer(3)]],
    constant Fragment_Shader::Uniforms_renderSettingUniformBlock & renderSettingUniformBlock [[buffer(4)]],
    constant Fragment_Shader::Uniforms_cameraUniformBlock & cameraUniformBlock [[buffer(5)]],
    texture2d<float> shadowTexture [[texture(0)]],
    sampler clampMiplessSampler [[sampler(0)]],
    texture2d<float> SphereTex [[texture(1)]],
    sampler textureSampler [[sampler(1)]],
    texture2d<float> PlaneTex [[texture(2)]])
{
    Fragment_Shader::PsIn input0;
    input0.WorldPositionIn = input.WorldPositionIn;
    input0.ColorIn = input.ColorIn;
    input0.NormalIn = input.NormalIn;
    input0.IfPlaneIn = input.IfPlaneIn;
    input0.Position = float4(input.Position.xyz, 1.0 / input.Position.w);
    Fragment_Shader main(
    ESMInputConstants,
    lightUniformBlock,
    renderSettingUniformBlock,
    cameraUniformBlock,
    shadowTexture,
    clampMiplessSampler,
    SphereTex,
    textureSampler,
    PlaneTex);
    return main.main(input0);
}
