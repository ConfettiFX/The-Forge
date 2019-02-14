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
    struct Uniforms_sdfUniformBlock
    {
        float mShadowHardness;
        float mShadowHardnessRcp;
        uint mMaxIteration;
    };
    constant Uniforms_sdfUniformBlock & sdfUniformBlock;
    struct Uniforms_cameraUniformBlock
    {
        float4x4 View;
        float4x4 Project;
        float4x4 ViewProject;
        float4x4 InvView;
        float4x4 InvProj;
        float4x4 InvViewProject;
        float4 NDCConversionConstants[2];
        float Near;
        float FarNearDiff;
        float FarNear;
    };
    constant Uniforms_cameraUniformBlock & cameraUniformBlock;
    struct Uniforms_renderSettingUniformBlock
    {
        float4 WindowDimension;
    };
    constant Uniforms_renderSettingUniformBlock & renderSettingUniformBlock;
    struct Uniforms_lightUniformBlock
    {
        float4x4 lightViewProj;
        float4 lightPosition;
        float4 lightColor;
        float4 lightUpVec;
        float lightRange;
    };
    constant Uniforms_lightUniformBlock & lightUniformBlock;
    texture2d<float> DepthBufferCopy;
    sampler clampMiplessSampler;
    struct PsIn
    {
        float4 WorldObjectCenterAndRadius [[flat]];
        float4 Position [[position]];
        bool FrontFacing [[front_facing]];
    };
    struct PsOut
    {
        float4 ShadowFactorOut [[color(0)]];
    };
    float4 WorldObjectCenterAndRadius;
    float linearizeZValue(float nonLinearValue)
    {
        return (cameraUniformBlock.FarNear / (cameraUniformBlock.Near + (nonLinearValue * cameraUniformBlock.FarNearDiff)));
    };
    float3 reconstructPosition(float3 windowCoords)
    {
        float linearZ = linearizeZValue(windowCoords.z);
        float4 quasiPostProj = ((cameraUniformBlock.NDCConversionConstants[0] + float4(((windowCoords).xy * float2(2.0, (-2.0))), 0.0, 0.0)) * (float4)(linearZ));
        return (cameraUniformBlock.InvViewProject * quasiPostProj).xyz + (cameraUniformBlock.NDCConversionConstants[1]).xyz;
    };
    float sdSphereLocal(float3 pos, float radius)
    {
        return (length(pos) - radius);
    };
    const float stepEpsilon = 0.0040000000;
    float closestDistLocal(float3 pos)
    {
        return sdSphereLocal(pos - WorldObjectCenterAndRadius.xyz, WorldObjectCenterAndRadius.w);
    };
    float calcSdfShadowFactor(float3 rayStart, float3 rayDir, float tMin, float tMax)
    {
        float minDist = 1.000000e+10;
        float t = tMin;
        for (uint i = 0; (i < sdfUniformBlock.mMaxIteration); (i++))
        {
            float3 param = (rayStart + (rayDir * (float3)(t)));
            float h = closestDistLocal(param);
            if ((h < minDist))
            {
                (minDist = h);
            }
            (t += h);
            if (((minDist < stepEpsilon) || (t > tMax)))
            {
                break;
            }
        }
        return max(((minDist - stepEpsilon) * sdfUniformBlock.mShadowHardness), 0.0);
    };
    PsOut main(PsIn input)
    {
        if (input.FrontFacing)
        {
            discard_fragment();
        }
        WorldObjectCenterAndRadius = input.WorldObjectCenterAndRadius;
        int2 i2uv = int2(input.Position.xy);
        float depth = DepthBufferCopy.read(uint2(i2uv), 0).x;
        float2 f2uv = float2(i2uv) / renderSettingUniformBlock.WindowDimension.xy;
        float3 position = reconstructPosition(float3(f2uv, depth));
        float3 lightVec = normalize(((lightUniformBlock.lightPosition).xyz - position));
        float dif = calcSdfShadowFactor(position, lightVec, sdfUniformBlock.mShadowHardnessRcp, lightUniformBlock.lightRange);
        PsOut output;
        output.ShadowFactorOut = (float)(4.0 / 3.0) * dif;
        return output;
    };

    Fragment_Shader(
					constant Uniforms_sdfUniformBlock & sdfUniformBlock,constant Uniforms_cameraUniformBlock & cameraUniformBlock,constant Uniforms_renderSettingUniformBlock & renderSettingUniformBlock,constant Uniforms_lightUniformBlock & lightUniformBlock,texture2d<float> DepthBufferCopy,sampler clampMiplessSampler) :
sdfUniformBlock(sdfUniformBlock),cameraUniformBlock(cameraUniformBlock),renderSettingUniformBlock(renderSettingUniformBlock),lightUniformBlock(lightUniformBlock),DepthBufferCopy(DepthBufferCopy),clampMiplessSampler(clampMiplessSampler) {}
};


fragment Fragment_Shader::PsOut stageMain(
    Fragment_Shader::PsIn input [[stage_in]],
	constant Fragment_Shader::Uniforms_lightUniformBlock & lightUniformBlock [[buffer(1)]],
	constant Fragment_Shader::Uniforms_cameraUniformBlock & cameraUniformBlock [[buffer(2)]],
    constant Fragment_Shader::Uniforms_sdfUniformBlock & sdfUniformBlock [[buffer(3)]],
    constant Fragment_Shader::Uniforms_renderSettingUniformBlock & renderSettingUniformBlock [[buffer(4)]],
    texture2d<float> DepthBufferCopy [[texture(0)]],
    sampler clampMiplessSampler [[sampler(0)]])
{
    Fragment_Shader::PsIn input0;
    input0.WorldObjectCenterAndRadius = input.WorldObjectCenterAndRadius;
    input0.FrontFacing = input.FrontFacing;
	input0.Position = input.Position;
    Fragment_Shader main(
    sdfUniformBlock,
    cameraUniformBlock,
    renderSettingUniformBlock,
    lightUniformBlock,
    DepthBufferCopy,
    clampMiplessSampler);
    return main.main(input0);
}
