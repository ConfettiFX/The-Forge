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


layout(early_fragment_tests) in;
#define PI 3.1415926289793f
#define PI_2 3.1415926289793f*2.0f

#define SHADOW_TYPE_NONE 0
#define SHADOW_TYPE_ESM 1
#define SHADOW_TYPE_SDF 2


layout (location = 0) in vec3 WorldPositionIn;
layout (location = 1) in vec3 ColorIn;
layout (location = 2) in vec3 NormalIn;
layout (location = 3) in flat int IfPlaneIn;

layout(location = 0) out vec4 FinalColor;

layout(set = 3, binding = 1) uniform texture2D SphereTex;
layout(set = 3, binding = 2) uniform texture2D PlaneTex;
layout(set = 0, binding = 0) uniform sampler textureSampler;
layout(set = 3, binding = 4) uniform texture2D shadowTexture;
layout(set = 0, binding = 1) uniform sampler clampMiplessSampler;

layout(set = 0, binding = 2) uniform renderSettingUniformBlock
{
    vec4 WindowDimension;
    int ShadowType;
};
layout(set = 1, binding = 0) uniform cameraUniformBlock
{
    mat4 View;
    mat4 Project;
    mat4 ViewProject;
    mat4 InvView;
};
layout(set = 1, binding = 1) uniform lightUniformBlock
{
    mat4 lightViewProj;
    vec4 lightPosition;
    vec4 lightColor;
};
layout(set = 1, binding = 2) uniform ESMInputConstants
{
    float Near;
    float Far;
    float FarNearDiff;
    float Near_Over_FarNearDiff;
    float Exponent;
};

float map_01(float depth)
{
    return (depth-Near)/FarNearDiff;
}
float calcESMShadowFactor(vec3 worldPos)
{
    vec4 shadowCoord = (lightViewProj)*vec4(worldPos,1.0);
    vec2 shadowUV = shadowCoord.xy/(vec2(shadowCoord.w,-shadowCoord.w)*2.0)+vec2(0.5);
    float lightMappedExpDepth = texture(sampler2D(shadowTexture, clampMiplessSampler), shadowUV).r;
    
    float pixelMappedDepth = map_01(shadowCoord.w);
    float shadowFactor = lightMappedExpDepth * exp2(-Exponent * pixelMappedDepth);

    return clamp(shadowFactor, 0, 1);
}
vec3 calcSphereTexColor(vec3 worldNormal)
{
  vec2 uv = vec2(0,0);
  uv.x = asin(worldNormal.x)/PI+0.5f;
  uv.y = asin(worldNormal.y)/PI+0.5f;  
  return texture(sampler2D(SphereTex, textureSampler), uv).rgb;
}
void main()
{
    // calculate shadow
    float shadowFactor = 1.0f;
    if (ShadowType == SHADOW_TYPE_ESM)
      shadowFactor = calcESMShadowFactor(WorldPositionIn);
    else if (ShadowType == SHADOW_TYPE_SDF)
      shadowFactor = texelFetch(sampler2D(shadowTexture  ,clampMiplessSampler), ivec2(gl_FragCoord.xy), 0).x;
      
    // calculate material parameters
    vec3 albedo = ColorIn;
    vec3 normal = normalize(NormalIn);
    if (IfPlaneIn == 0){//spheres
        albedo *= calcSphereTexColor(normal);
    }
    else {//plane
        albedo *= texture(sampler2D(PlaneTex, textureSampler), WorldPositionIn.xz).rgb;
    }
    
    vec3 lightVec = normalize(lightPosition.xyz-WorldPositionIn);
    vec3 viewVec = normalize(WorldPositionIn - InvView[3].xyz);
    float dotP = max(dot(normal, lightVec.xyz),0.0);
    vec3 diffuse = albedo * max(dotP,0.05);// 0.05 set as ambient color
    const float specularExp = 10.0;
    float specular = pow(max(dot(reflect(lightVec, normal), viewVec),0.0)*dotP, specularExp); // TODO: use half-vector not relfection, and normalize the phong factor
    vec3 finalColor = clamp(diffuse+ specular*0.5f,0,1); // TODO: use fresnel to blend between diffuse and specular
    finalColor *= lightColor.xyz * shadowFactor;
    
    FinalColor = vec4((finalColor).xyz, 1);
}