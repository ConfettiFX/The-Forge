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
 * "License") you may not use this file except in compliance
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
#include <metal_matrix>
using namespace metal;

struct Light
{
	float4 pos;
	float4 col;
	float radius;
	float intensity;
	float _pad0;
	float _pad1;
};

static constant float PI = 3.14159265359;

struct CameraData
{
	float4x4 viewMat;
	float4x4 projMat;
	float4x4 viewProjMat;
	float4x4 InvViewProjMat;
	
	float4 cameraWorldPos;
	float4 viewPortSize;
};

struct ObjectData
{
	float4x4 worldMat;
	float roughness;
	float metalness;
	int pbrMaterials;
};

struct LightData
{
	Light lights[16];
	int currAmountOflights;
};

struct DirectionalLight
{
	float4 mPos;
	float4 mCol; //alpha is the intesity
	float4 mDir;
};


struct DLightData {
	DirectionalLight dlights[16];
	int currAmountOfDLights;
};


struct VSOutput {
	float4 position [[position]];
	float2 uv;
};

float3 FresnelSchlickRoughness(float cosTheta, float3 F0, float roughness)
{
	return F0 + (max(float3(1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
}

float3 fresnelSchlick(float cosTheta, float3 F0)
{
	return F0 + (1.0f - F0) * pow(1.0 - cosTheta, 5.0);
}

float distributionGGX(float3 N, float3 H, float roughness)
{
	float a = roughness * roughness;
	float a2 = a * a;
	float NdotH = max(dot(N, H), 0.0);
	float NdotH2 = NdotH * NdotH;
	float nom = a2;
	float denom = (NdotH2 * (a2 - 1.0) + 1.0);
	denom = PI * denom * denom;
	
	return nom / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
	float r = (roughness + 1.0f);
	float k = (r * r) / 8.0f;
	
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

float3 getNormalFromMap(texture2d<float> normalMap, sampler defaultSampler, float2 uv, float3 pos, float3 normal)
{
	
	float3 tangentNormal = normalMap.sample(defaultSampler, uv).rgb * 2.0 - 1.0;
	
	float3 Q1 = dfdx(pos);
	float3 Q2 = dfdy(pos);
	float2 st1 = dfdx(uv);
	float2 st2 = dfdy(uv);
	
	float3 N = normalize(normal);
	float3 T = normalize(Q1 * st2.g - Q2 * st1.g);
	float3 B = -normalize(cross(N, T));
	float3x3 TBN = float3x3(T, B, N);
	float3 res = TBN * tangentNormal;
	return res;
}

float3 getWorldPositionFromDepth(float2 ndc, float sceneDepth, CameraData cbExtendCamera )
{
	float4 worldPos = cbExtendCamera.InvViewProjMat * float4(ndc, sceneDepth, 1.0);
	worldPos /= worldPos.w;
	
	return float3(worldPos.xyz);
}

fragment float4 stageMain(VSOutput input [[stage_in]],
						  constant CameraData& cbExtendCamera [[buffer(1)]],
						  constant LightData& cbLights  [[buffer(2)]],
						  constant DLightData& cbDLights  [[buffer(3)]],
						  texture2d<float, access::sample> brdfIntegrationMap[[texture(0)]],
						  texturecube<float, access::sample> irradianceMap[[texture(1)]],
						  texture2d<float> AlbedoTexture[[texture(2)]],
						  texture2d<float> NormalTexture[[texture(3)]],
						  texture2d<float> RoughnessTexture[[texture(4)]],
						  depth2d<float, access::sample> DepthTexture [[texture(5)]],
						  texturecube<float, access::sample> specularMap[[texture(6)]],
						  
						  sampler envSampler[[sampler(0)]],
						  sampler defaultSampler[[sampler(1)]])
{
	
	// default albedo
	float3 albedo = AlbedoTexture.sample(defaultSampler, input.uv, 0).rgb;
	
	float4 normalColor = NormalTexture.sample(defaultSampler, input.uv, 0);
	float4 roughnessColor = RoughnessTexture.sample(defaultSampler, input.uv, 0);
	
	float _roughness = roughnessColor.r;
	float _metalness = normalColor.a;
	float ao = 1.0f;
	
	float depth = DepthTexture.sample(defaultSampler, input.uv);
	
	if(depth > 0.99999)
	{
		//Skybox
		return float4(albedo, 1.0);
	}
	
	float3 N = normalize(normalColor.rgb);
	
	float2 ndc = float2(input.uv.x * 2.0 - 1.0, (1.0 - input.uv.y) * 2.0 - 1.0);
	
	float3 worldPos = getWorldPositionFromDepth(ndc, depth, cbExtendCamera);
	
	float3 V = normalize(cbExtendCamera.cameraWorldPos.xyz - worldPos);
	float3 R = normalize(reflect(-V, N));
	
	// 0.04 is the index of refraction for metal
	float3 F0 = float3(0.04f, 0.04f, 0.04f);
	F0 = mix(F0, albedo.rgb, _metalness);
	
	// Lo = outgoing radiance
	float3 Lo = float3(0.0f, 0.0f, 0.0f);
	
	//Directional Lights
	for(int i = 0; i < cbDLights.currAmountOfDLights; ++i)
	{
		// Vec from world pos to light pos
		float3 L =  -normalize(float3(cbDLights.dlights[i].mDir.xyz));
		
		// halfway vec
		float3 H = normalize(V + L);
		
		float3 radiance = float3(cbDLights.dlights[i].mCol.rgb) * cbDLights.dlights[i].mCol.a;
		
		float NDF = distributionGGX(N, H, _roughness);
		float G = GeometrySmith(N, V, L, _roughness);
		float3 F = fresnelSchlick(max(dot(N,H), 0.0), F0);
		
		float3 nominator = NDF * G * F;
		float denominator = 4.0f * max(dot(N,V), 0.0) * max(dot(N, L), 0.0) + 0.001;
		float3 specular = nominator / denominator;
		
		float3 kS = F;
		
		float3 kD = float3(1.0, 1.0, 1.0) - kS;
		
		kD *= 1.0f - _metalness;
		
		float NdotL = max(dot(N, L), 0.0);
		
		if(NdotL>0.0f)
		{
			Lo +=  (kD * albedo / PI + specular) * radiance * NdotL;
		}
		else
		{
			Lo += float3(0.0f, 0.0f, 0.0f);
		}
	}
	
	//Point Lights
	for(int i = 0; i < cbLights.currAmountOflights; ++i)
	{
		float3 L = normalize(cbLights.lights[i].pos.rgb - worldPos);
		
		float3 H = normalize(V + L);
		
		float distance = length(cbLights.lights[i].pos.xyz - worldPos);
		
		float distanceByRadius = 1.0f - pow((distance / cbLights.lights[i].radius), 4);
		float clamped = pow(saturate(distanceByRadius), 2.0f);
		float attenuation = clamped / (distance * distance + 1.0f);
		
		float3 radiance = cbLights.lights[i].col.rgb * attenuation * cbLights.lights[i].intensity;
		
		float NDF = distributionGGX(N, H, _roughness);
		float G = GeometrySmith(N, V, L, _roughness);
		float3 F = fresnelSchlick(max(dot(N, H), 0.0), F0);
		
		float3 nominator = NDF * G * F;
		float denominator = 4.0f * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.001;
		float3 specular = nominator / denominator;
		
		float3 kS = F;
		
		float3 kD = float3(1.0, 1.0f, 1.0f) - kS;
		
		kD *= 1.0f - _metalness;
		
		float NdotL = max(dot(N, L), 0.0);
		
		Lo += (kD * albedo.rgb / PI + specular) * radiance * NdotL;
	}
	
	// Ambient-term.
	float3 F = FresnelSchlickRoughness(max(dot(N, V), 0.0), F0, _roughness);
	float3 kS = F;
	float3 kD = float3(1.0) - kS;
	kD *= 1.0 - _metalness;
	
	float3 irradiance = irradianceMap.sample(envSampler, N).rgb;
	float3 diffuse = kD * irradiance * albedo.rgb;
	
	
	uint mipIndex = (uint)(_roughness * 4.0f);
	float3 specularColor = specularMap.sample(envSampler, R, level(mipIndex)).rgb;
	
	float maxNVRough = float(max(dot(N, V), _roughness));
	float2 brdf  = brdfIntegrationMap.sample(defaultSampler, float2(max(dot(N, V), 0.0), maxNVRough)).rg;
	float3 specular = specularColor * (F * brdf.x + brdf.y);
	
	float3 ambient = (diffuse + specular) * float3(ao);
	float3 color = ambient* 0.3 + Lo;
	color = color / (color + float3(1.0f));
	
	float gammaCorr = 1.0f / 2.2f;
	
	color.r = pow(color.r, gammaCorr);
	color.g = pow(color.g, gammaCorr);
	color.b = pow(color.b, gammaCorr);
	
	return float4(color.r, color.g, color.b, 1.0f);
}
