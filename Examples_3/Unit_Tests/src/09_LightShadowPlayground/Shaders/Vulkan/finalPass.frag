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
#version 450 core

#define RENDER_OUTPUT_SCENE 0
#define RENDER_OUTPUT_SDF_MAP 1
#define RENDER_OUTPUT_ALBEDO 2
#define RENDER_OUTPUT_NORMAL 3
#define RENDER_OUTPUT_POSITION 4
#define RENDER_OUTPUT_DEPTH 5
#define RENDER_OUTPUT_SSAO_MAP 6
#define RENDER_OUTPUT_ESM_MAP 7

#define SHADOW_TYPE_NONE 0
#define SHADOW_TYPE_ESM 1
#define SHADOW_TYPE_SDF 2

#define SPECULAR_EXP 10.0f

layout(location = 0) out vec4 FinalColor;



layout(set = 0, binding = 0) uniform sampler depthSampler;
layout(set = 0, binding = 1) uniform sampler textureSampler;

layout(set = 0, binding = 2) uniform texture2D gBufferColor;
layout(set = 0, binding = 3) uniform texture2D gBufferNormal;
layout(set = 0, binding = 4) uniform texture2D gBufferPosition;
layout(set = 0, binding = 5) uniform texture2D gBufferDepth;
layout(set = 0, binding = 6) uniform texture2D shadowMap;
layout(set = 0, binding = 7) uniform texture2D skyboxTex;
layout(set = 0, binding = 8) uniform texture2D sdfScene;

layout(set = 0, binding = 9) uniform lightUniformBlock
{
    mat4 lightViewProj;
    vec4 lightDirection;
    vec4 lightColor;
};


layout(set = 0, binding = 10) uniform renderSettingUniformBlock
{
    vec4 WindowDimension;
    int RenderOutput;
    int ShadowType;
};

layout(set = 0, binding = 11) uniform ESMInputConstants
{
    vec2 ScreenDimension;
    vec2 NearFarDist;
    float Exponent;
    uint BlurWidth;
    int IfHorizontalBlur;
    int padding1;
};

layout(set = 0, binding = 12) uniform cameraUniform
{
    vec4 CameraPosition;
};

float map_01(float x, float v0, float v1)
{
    return (x - v0) / (v1 - v0);
}
float calcShadowFactor(vec4 worldPos)
{
    const float c = Exponent;
    vec4 shadowCoord = (lightViewProj)*worldPos;
    vec2 shadowIndex;
    shadowIndex.x = ((shadowCoord.x / shadowCoord.w) /  2.0f) + 0.5f;
    shadowIndex.y = (-(shadowCoord.y / shadowCoord.w) / 2.0f) + 0.5f;
    float lightMappedExpDepth = texture(sampler2D(shadowMap, depthSampler), shadowIndex).r;
    float pixelDepth = shadowCoord.w;
    float pixelMappedDepth = map_01(pixelDepth, NearFarDist.x, NearFarDist.y);
    float shadowFactor = lightMappedExpDepth * exp(-c * pixelMappedDepth);

    return clamp(shadowFactor, 0, 1);
}

// Pixel shader
void main(void)
{
    ivec2 i2uv = ivec2(gl_FragCoord.xy);
    vec2 f2uv = vec2(i2uv.xy) / WindowDimension.xy;
    
    vec4 normalData = texelFetch(sampler2D(gBufferNormal  ,textureSampler), i2uv, 0);
    vec4 position = texelFetch(sampler2D(gBufferPosition  ,textureSampler), i2uv, 0);
    float depth = texelFetch(sampler2D(gBufferDepth  ,textureSampler), i2uv, 0).r;
    vec4 colorData = texelFetch(sampler2D(gBufferColor  ,textureSampler), i2uv, 0);
    float shadowMapVal = texelFetch(sampler2D(shadowMap  ,textureSampler), i2uv, 0).r;
    
    vec3 normal = normalData.xyz * 2.0f - 1.0f;
    float shadowFactor = 1.0f;
    if (ShadowType == SHADOW_TYPE_ESM)
      shadowFactor = calcShadowFactor(position);
    else if (ShadowType == SHADOW_TYPE_SDF)
      shadowFactor = texelFetch(sampler2D(sdfScene  ,textureSampler), i2uv, 0).x;
    if (RenderOutput == RENDER_OUTPUT_ESM_MAP) {
        float shadowMapVal01 = log(shadowMapVal) / Exponent;
        FinalColor =  vec4(vec3(1, 1, 1)*shadowMapVal01, 1);
        return;
    }
    else if (RenderOutput == RENDER_OUTPUT_SDF_MAP) {
        float sdfFactor =  texelFetch(sampler2D(sdfScene  ,textureSampler), i2uv, 0).x;
        FinalColor =  vec4(vec3(1, 1, 1)*sdfFactor, 1);
        return;
    }
    if (length(normalData.xyz)<0.01) {
        FinalColor = texture(sampler2D(skyboxTex, textureSampler), f2uv);
        return;
    }
    if (RenderOutput == RENDER_OUTPUT_SCENE){
        vec3 lightVec = -normalize(lightDirection.xyz);
        vec3 viewVec = normalize(position.xyz - CameraPosition.xyz);
        float dotP = dot(normal, lightVec.xyz);
        if (dotP < 0.05f)
          dotP = 0.05f;//set as ambient color
        vec3 diffuse = lightColor.xyz * colorData.xyz * dotP;
        vec3 specular = lightColor.xyz * pow(clamp(dot(reflect(lightVec, normal), viewVec),0,1), SPECULAR_EXP);
        vec3 finalColor = clamp(diffuse+ specular*0.5f,0,1);
        finalColor *= shadowFactor;
        
        FinalColor = vec4((finalColor).xyz, 1);
        return;
    }
    else if (RenderOutput == RENDER_OUTPUT_ALBEDO) {
        FinalColor = vec4(colorData.xyz, 1);
        return;
    }
    else if (RenderOutput == RENDER_OUTPUT_NORMAL) {
        FinalColor = vec4(normalData.xyz, 1);
        return;
    }
    else if (RenderOutput == RENDER_OUTPUT_POSITION) {
        FinalColor = vec4(position.xyz, 1);
        return;
    }
    else if (RenderOutput == RENDER_OUTPUT_DEPTH) {
        FinalColor = vec4(vec3(1, 1, 1)*depth, 1);
        return;
    }
    //should not reach here
    FinalColor = vec4(1,0,0,1);
}
