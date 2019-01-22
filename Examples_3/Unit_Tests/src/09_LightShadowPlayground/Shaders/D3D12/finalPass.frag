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
    float4 position : SV_Position;
};

SamplerState depthSampler : register(s0);
SamplerState textureSampler : register(s1);

Texture2D gBufferColor : register(t0);
Texture2D gBufferNormal : register(t1);
Texture2D gBufferPosition : register(t2);
Texture2D gBufferDepth : register(t3);
Texture2D shadowMap : register(t4);
Texture2D skyboxTex : register(t5);
Texture2D sdfScene : register(t6);

cbuffer lightUniformBlock : register(b0)
{
    float4x4 lightViewProj;
    float4 lightDirection;
    float4 lightColor;
};

cbuffer renderSettingUniformBlock : register(b1)
{
    float4 WindowDimension;
    int RenderOutput;
    int ShadowType;
};
cbuffer ESMInputConstants : register(b2)
{
    float2 ScreenDimension;
    float2 NearFarDist;
    float Exponent;
    uint BlurWidth;
    int IfHorizontalBlur;
    int padding1;
};
cbuffer cameraUniform : register(b3)
{
    float4 CameraPosition;
};


float map_01(float x, float v0, float v1)
{
    return (x - v0) / (v1 - v0);
}
float calcShadowFactor(float4 worldPos)
{
    const float c = Exponent;
    float4 shadowCoord = mul((lightViewProj), worldPos);
    float2 shadowIndex;
    shadowIndex.x = ((shadowCoord.x / shadowCoord.w) / 2.0f) + 0.5f;
    shadowIndex.y = (-(shadowCoord.y / shadowCoord.w) / 2.0f) + 0.5f;
    float lightMappedExpDepth = shadowMap.Sample(depthSampler, shadowIndex).r;
    float pixelDepth = shadowCoord.w;
    float pixelMappedDepth = map_01(pixelDepth, NearFarDist.x, NearFarDist.y);
    float shadowFactor = lightMappedExpDepth * exp(-c * pixelMappedDepth);

    return saturate(shadowFactor);
}


// Pixel shader
float4 main(VSOutput input, uint i : SV_SampleIndex) : SV_Target
{
    uint3 u3uv = uint3(input.position.xy, 0);
    float2 f2uv = float2(u3uv.xy + 0.5f) / WindowDimension.xy;

    float4 normalData   =   gBufferNormal.Load(u3uv);
    float4 position     = gBufferPosition.Load(u3uv);
    float depth         =    gBufferDepth.Load(u3uv).r;
    float4 colorData    =    gBufferColor.Sample(textureSampler, f2uv);
    float shadowMapVal  =       shadowMap.Load(u3uv).r;

    float3 normal = normalData.xyz * 2.0f - 1.0f;

    float shadowFactor = 1.0f;
    if (ShadowType == SHADOW_TYPE_ESM)
      shadowFactor = calcShadowFactor(position);
    else if (ShadowType == SHADOW_TYPE_SDF)
      shadowFactor = sdfScene.Load(u3uv).x;

    if (RenderOutput == RENDER_OUTPUT_ESM_MAP) {
        float shadowMapVal01 = log(shadowMapVal) / Exponent;
        return float4(float3(1, 1, 1)*shadowMapVal01, 1);
    }
    else if (RenderOutput == RENDER_OUTPUT_SDF_MAP) {
      return sdfScene.Load(u3uv);
    }
    if (length(normalData.xyz)<0.01){
        return skyboxTex.Sample(textureSampler, f2uv);
    }
    if (RenderOutput == RENDER_OUTPUT_SCENE){
        float3 lightVec = -normalize(lightDirection.xyz);
        float3 viewVec = normalize(position.xyz - CameraPosition.xyz);
        float dotP = dot(normal, lightVec.xyz);
        if (dotP < 0.05f)
          dotP = 0.05f;//set as ambient color
        float3 diffuse = lightColor.xyz * colorData.xyz * dotP;
        float3 specular = lightColor.xyz * pow(saturate(dot(reflect(lightVec, normal), viewVec)), SPECULAR_EXP);
        float3 finalColor = saturate(diffuse+ specular*0.5f);
        finalColor *= shadowFactor;
        return float4((finalColor).xyz, 1);
    }
    else if (RenderOutput == RENDER_OUTPUT_ALBEDO) {
        return float4(colorData.xyz, 1);
    }
    else if (RenderOutput == RENDER_OUTPUT_NORMAL) {
        return float4(normalData.xyz, 1);
    }
    else if (RenderOutput == RENDER_OUTPUT_POSITION) {
        return float4(position.xyz, 1);
    }
    else if (RenderOutput == RENDER_OUTPUT_DEPTH) {
        return float4(float3(1, 1, 1)*depth, 1);
    }
    return float4(1,0,0,1);//should not reach here
}
