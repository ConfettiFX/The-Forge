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

struct Light
{
	float4 pos;
	float4 col;
	float radius;
	float intensity;
};

static constant float PI = 3.14159265359;

struct CameraData {
    float4x4 projView;
    float3 camPos;
};

struct ObjectData {
    float4x4 worldMat;
    float roughness;
    float metalness;
};

struct LightData {
    int currAmountOflights;
    int pad0;
    int pad1;
    int pad2;
    Light lights[16];
};

struct VSInput
{
    float4 Position [[attribute(0)]];
    float4 Normal   [[attribute(1)]];
};

struct VSOutput {
    float4 Position [[position]];
    float3 pos;
    float3 normal;
};

float3 fresnelSchlick(float cosTheta, float3 F0)
{
	return F0 + (1.0f - F0) * pow(1.0 - cosTheta, 5.0);
}

float distributionGGX(float3 N, float3 H, float roughness)
{
	float a = roughness*roughness;
	float a2 = a*a;
	float NdotH = max(dot(N, H), 0.0);
	float NdotH2 = NdotH*NdotH;
	float nom = a2;
	float denom = (NdotH2 * (a2 - 1.0) + 1.0);
	denom = PI * denom * denom;

	return nom / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
	float r = (roughness + 1.0f);
	float k = (r*r) / 8.0f;

	float nom = NdotV;
	float denom = NdotV * (1.0 - k) + k;

	return nom / denom;
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
	float NdotV = max(dot(N, V), 0.0);
	float NdotL = max(dot(N, L), 0.0);
	float ggx2 = GeometrySchlickGGX(NdotV, roughness);
	float ggx1 = GeometrySchlickGGX(NdotL, roughness);

	return ggx1 * ggx2;
}

vertex VSOutput VSMain(VSInput In [[stage_in]],
                       constant CameraData& cbCamera [[buffer(0)]],
                       constant ObjectData& cbObject [[buffer(1)]])
{
    VSOutput result;
    float4x4 tempMat = cbCamera.projView * cbObject.worldMat;
    result.Position = tempMat * In.Position;

    result.pos = (cbObject.worldMat * In.Position).xyz;
    result.normal = (cbObject.worldMat * float4(In.Normal.xyz, 0.0f)).xyz;

    return result;
}
                                                      
fragment float4 PSMain(VSOutput In [[stage_in]],
                       constant CameraData& cbCamera [[buffer(0)]],
                       constant ObjectData& cbObject [[buffer(1)]],
                       constant LightData& cbLights  [[buffer(2)]])
{
    float3 N = normalize(In.normal);
    float3 V = normalize(cbCamera.camPos - In.pos);
    
    // Fixed albedo for now
    float3 albedo = float3(0.9568f, 0.8745f, 0.8588f);
    
    // 0.04 is the index of refraction for metal
    float3 F0 = float3(0.04f, 0.04f, 0.04f);
    F0 = mix(F0, albedo, cbObject.metalness);
    
    // Lo = outgoing radiance
    float3 Lo = float3(0.0f, 0.0f, 0.0f);
    for(int i = 0; i < cbLights.currAmountOflights; ++i)
    {
        float3 L = normalize(cbLights.lights[i].pos.rgb - In.pos);
        
        float3 H = normalize(V + L);
        
        float distance = length(cbLights.lights[i].pos.xyz - In.pos);
        
        float distanceByRadius = 1.0f - pow((distance / cbLights.lights[i].radius), 4);
        float clamped = pow(saturate(distanceByRadius), 2.0f);
        float attenuation = clamped / (distance * distance + 1.0f);
        
        float3 radiance = cbLights.lights[i].col.rgb * attenuation * cbLights.lights[i].intensity;
        
        float NDF = distributionGGX(N, H, cbObject.roughness);
        float G = GeometrySmith(N, V, L, cbObject.roughness);
        float3 F = fresnelSchlick(max(dot(N, H), 0.0), F0);
        
        
        float3 nominator = NDF * G * F;
        float denominator = 4.0f * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.001;
        float3 specular = nominator / denominator;
        
        
        float3 kS = F;
        
        float3 kD = float3(1.0, 1.0f, 1.0f) - kS;
        
        kD *= 1.0f - cbObject.metalness;
        
        float NdotL = max(dot(N, L), 0.0);
        
        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }
    
    float3 ambient = float3(0.03f, 0.03f, 0.03f) * albedo;
    float3 color = Lo + ambient;
    color = color / (color + float3(1.0f, 1.0f, 1.0f));
    
    float gammaCorr = 1.0f / 2.2f;
    
    color.r = pow(color.r, gammaCorr);
    color.g = pow(color.g, gammaCorr);
    color.b = pow(color.b, gammaCorr);
    
    return float4(color.r, color.g, color.b, 1.0f);
}
