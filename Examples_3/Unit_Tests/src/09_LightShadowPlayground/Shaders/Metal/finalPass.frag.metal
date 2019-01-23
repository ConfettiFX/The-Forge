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

#define RENDER_OUTPUT_SCENE 0
#define RENDER_OUTPUT_SDF_MAP 1
#define RENDER_OUTPUT_ALBEDO 2
#define RENDER_OUTPUT_NORMAL 3
#define RENDER_OUTPUT_POSITION 4
#define RENDER_OUTPUT_DEPTH 5
#define RENDER_OUTPUT_ESM_MAP 6

#define SHADOW_TYPE_NONE 0
#define SHADOW_TYPE_ESM 1
#define SHADOW_TYPE_SDF 2

#define SPECULAR_EXP 10.0f

struct VSOutput
{
    float4 position [[position]];
};

struct LightUniform
{
    float4x4 lightViewProj;
    float4 lightDirection;
    float4 lightColor;
};

struct RenderSettingUniform
{
    float4 WindowDimension;
    int RenderOutput;
    int ShadowType;
};
struct ESMInput
{
    float2 ScreenDimension;
    float2 NearFarDist;
    float Exponent;
    uint BlurWidth;
    int IfHorizontalBlur;
    int padding1;
};
struct CameraUniformBlock
{
    float4 CameraPosition;
};


float map_01(float x, float v0, float v1)
{
    return (x - v0) / (v1 - v0);
}

// Pixel shader
fragment float4 stageMain(VSOutput input                                         [[stage_in]],
                          sampler depthSampler                                   [[sampler(0)]],
                          sampler textureSampler                                 [[sampler(1)]],
                          
                          texture2d<float,access::sample> gBufferColor           [[texture(0)]],
                          texture2d<float,access::read> gBufferNormal            [[texture(1)]],
                          texture2d<float,access::read> gBufferPosition          [[texture(2)]],
                          texture2d<float,access::read> gBufferDepth             [[texture(3)]],
                          texture2d<float,access::sample> shadowMap              [[texture(7)]],
                          texture2d<float,access::sample> skyboxTex              [[texture(8)]],
                          texture2d<float,access::sample> sdfScene               [[texture(9)]],
                         
                         constant LightUniform& lightUniformBlock                [[buffer(4)]],
                         constant RenderSettingUniform& renderSettingUniformBlock[[buffer(5)]],
                         constant ESMInput& ESMInputConstants                    [[buffer(6)]],
                         constant CameraUniformBlock& cameraUniform              [[buffer(10)]])
{
    uint2 u2uv = uint2(input.position.xy);
    float2 f2uv = (float2(u2uv) + 0.5) / renderSettingUniformBlock.WindowDimension.xy;

    float4 normalData   =   gBufferNormal.read(u2uv);
    float4 position     = gBufferPosition.read(u2uv);
    float depth         =    gBufferDepth.read(u2uv).r;
    float4 colorData    =    gBufferColor.sample(textureSampler, f2uv);
    float shadowMapVal  =       shadowMap.read(u2uv).r;

    float3 normal = normalData.xyz * 2.0f - 1.0f;
    
    float shadowFactor = 1.0f;
    if (renderSettingUniformBlock.ShadowType == SHADOW_TYPE_ESM)
    {
        ///////////////////////////////////////////////////////////////
        ////////// ESMshadow begins here //////////////////
        const float c = ESMInputConstants.Exponent;
        float4 shadowCoord = lightUniformBlock.lightViewProj * position;
        float2 shadowIndex;
        shadowIndex.x = ((shadowCoord.x / shadowCoord.w) / 2.0f) + 0.5f;
        shadowIndex.y = (-(shadowCoord.y / shadowCoord.w) / 2.0f) + 0.5f;
        float lightMappedExpDepth = shadowMap.sample(depthSampler, shadowIndex).r;
        float pixelDepth = shadowCoord.w;
        float pixelMappedDepth = map_01(pixelDepth, ESMInputConstants.NearFarDist.x, ESMInputConstants.NearFarDist.y);
        shadowFactor = lightMappedExpDepth * exp(-c * pixelMappedDepth);
        shadowFactor = clamp(shadowFactor, 0.0f, 1.0f);
        ////////// ESM shadow ends here  //////////////////
        ///////////////////////////////////////////////////////////////
    }
    else if (renderSettingUniformBlock.ShadowType == SHADOW_TYPE_SDF)
    {
        shadowFactor = sdfScene.sample(textureSampler, f2uv).x;
    }

    if (renderSettingUniformBlock.RenderOutput == RENDER_OUTPUT_ESM_MAP) {
        float shadowMapVal01 = log(shadowMapVal) / ESMInputConstants.Exponent;
        return float4(float3(1, 1, 1)*shadowMapVal01, 1);
    }
	else if (renderSettingUniformBlock.RenderOutput == RENDER_OUTPUT_SDF_MAP) {
		return sdfScene.sample(textureSampler, f2uv);
	}
    if (length(normalData.xyz)<0.01){
        return skyboxTex.sample(textureSampler, f2uv);
    }
    if (renderSettingUniformBlock.RenderOutput == RENDER_OUTPUT_SCENE){
        float3 lightVec = -normalize(lightUniformBlock.lightDirection.xyz);
        float3 viewVec = normalize(position.xyz - cameraUniform.CameraPosition.xyz);
        float dotP = dot(normal, lightVec.xyz);
        if (dotP < 0.05f)
          dotP = 0.05f;//set as ambient color
        float3 diffuse = lightUniformBlock.lightColor.xyz * colorData.xyz * dotP;
        float3 specular = lightUniformBlock.lightColor.xyz * pow(clamp(dot(reflect(lightVec, normal), viewVec), 0.0f, 1.0f), SPECULAR_EXP);
        float3 finalColor = clamp(diffuse+ specular*0.5f, 0.0f, 1.0f);
        finalColor *= shadowFactor;
        return float4((finalColor).xyz, 1);
    }
    else if (renderSettingUniformBlock.RenderOutput == RENDER_OUTPUT_ALBEDO) {
        return float4(colorData.xyz, 1);
    }
    else if (renderSettingUniformBlock.RenderOutput == RENDER_OUTPUT_NORMAL) {
        return float4(normalData.xyz, 1);
    }
    else if (renderSettingUniformBlock.RenderOutput == RENDER_OUTPUT_POSITION) {
        return float4(position.xyz, 1);
    }
    else if (renderSettingUniformBlock.RenderOutput == RENDER_OUTPUT_DEPTH) {
        return float4(float3(1, 1, 1)*depth, 1);
    }
    return float4(1,0,0,1);//should not reach here
}
