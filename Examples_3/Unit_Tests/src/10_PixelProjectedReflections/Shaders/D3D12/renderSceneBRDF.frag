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


struct Light
{
	float4 pos;
	float4 col;
	float radius;
	float intensity;
	float _pad0;
	float _pad1;
};

static const float PI = 3.14159265359;


cbuffer cbExtendCamera : register(b0) {
	float4x4 viewMat;
	float4x4 projMat;
	float4x4 viewProjMat;
	float4x4 InvViewProjMat;

	float4 cameraWorldPos;
	float4 viewPortSize;
}


cbuffer cbLights : register(b1) {
	Light lights[16];
	int currAmountOflights;
}

struct DirectionalLight
{
	float4 mPos;
	float4 mCol; //alpha is the intesity
	float4 mDir;
};

cbuffer cbDLights : register(b2)  {
	DirectionalLight dlights[16];
	int currAmountOfDLights;
};


Texture2D<float2> brdfIntegrationMap : register(t3);
TextureCube<float4> irradianceMap : register(t4);
TextureCube<float4> specularMap : register(t5);

Texture2D AlbedoTexture : register(t6);
Texture2D NormalTexture : register(t7);
Texture2D RoughnessTexture : register(t8);
Texture2D DepthTexture : register(t9);

SamplerState envSampler : register(s10);
SamplerState defaultSampler : register(s11);


float3 FresnelSchlickRoughness(float cosTheta, float3 F0, float roughness)
{
	//return F0 + (max(float3(1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);


	float3 ret = float3(0.0, 0.0, 0.0);
	float powTheta = pow(1.0 - cosTheta, 5.0);
	float invRough = float(1.0 - roughness);

	ret.x = F0.x + (max(invRough, F0.x) - F0.x) * powTheta;
	ret.y = F0.y + (max(invRough, F0.y) - F0.y) * powTheta;
	ret.z = F0.z + (max(invRough, F0.z) - F0.z) * powTheta;

	return ret;
}

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

float3 getWorldPositionFromDepth(float2 ndc, float sceneDepth )
{
	float4 worldPos = mul( InvViewProjMat, float4(ndc, sceneDepth, 1.0));
	worldPos /= worldPos.w;

	return float3(worldPos.xyz);
}


struct VSOutput {
	float4 Position : SV_POSITION;	
	float2 uv:    TEXCOORD0;
};

float4 main(VSOutput input) : SV_TARGET
{

	// default albedo 
	float3 albedo = AlbedoTexture.Sample(defaultSampler, input.uv).rgb;

	float4 normalColor = NormalTexture.Sample(defaultSampler, input.uv);
	float4 roughnessColor = RoughnessTexture.Sample(defaultSampler, input.uv);

	float _roughness = roughnessColor.r;
	float _metalness = normalColor.a;
	float ao = 1.0f;

	float depth = DepthTexture.Sample(defaultSampler, input.uv).r;
	
	if(depth >= 1.0)
	{
		//Skybox
		return float4(albedo, 1.0);
	}

	float3 N = normalize(normalColor.rgb);
	
	float2 ndc = float2(input.uv.x * 2.0 - 1.0, (1.0 - input.uv.y) * 2.0 - 1.0);
	
	float3 worldPos = getWorldPositionFromDepth(ndc, depth);

	float3 V = normalize(cameraWorldPos.xyz - worldPos);
	float3 R = reflect(-V, N);

	// 0.04 is the index of refraction for metal
	float3 F0 = float3(0.04f, 0.04f, 0.04f);
	F0 = lerp(F0, albedo, _metalness);

	float3 Lo = float3(0.0, 0.0, 0.0);

	//Directional Lights
	for(int i = 0; i < currAmountOfDLights; ++i)
	{
		 // Vec from world pos to light pos
		float3 L =  -normalize(float3(dlights[i].mDir.xyz));

		// halfway vec
		float3 H = normalize(V + L);

		float3 radiance = float3(dlights[i].mCol.rgb) * dlights[i].mCol.a;

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
	for(int i = 0; i < currAmountOflights; ++i)
	{
		 // Vec from world pos to light pos
		float3 L =  normalize(float3(lights[i].pos.xyz) - worldPos);

		// halfway vec
		float3 H = normalize(V + L);
		
		// get distance
		float distance = length(float3(lights[i].pos.xyz) - worldPos);

		// Distance attenuation from Epic Games' paper 
		float distanceByRadius = 1.0f - pow((distance / lights[i].radius), 4);
		float clamped = pow(clamp(distanceByRadius, 0.0f, 1.0f), 2.0f);
		float attenuation = clamped / (distance * distance + 1.0f);

		//float attenuation = 1.0f;
		// Radiance is color mul with attenuation mul intensity 
		float3 radiance = float3(lights[i].col.rgb) * attenuation * lights[i].intensity;

		float NDF = distributionGGX(N, H, _roughness);
		float G = GeometrySmith(N, V, L, _roughness);
		float3 F = fresnelSchlick(max(dot(N, H), 0.0), F0);

		float3 nominator = NDF * G * F;
		float denominator = 4.0f * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.001;
		float3 specular = nominator / denominator;

		float3 kS = F;

		float3 kD = float3(1.0, 1.0, 1.0) - kS;

		kD *= 1.0f - _metalness;

		float NdotL = max(dot(N, L), 0.0);

		if(NdotL>0.0f) {

		Lo += (kD * albedo / PI + specular) * radiance * NdotL;
		}else {


			Lo+= float3(0.0f, 0.0f, 0.0f);
		}		
	}


	float3 F = FresnelSchlickRoughness(max(dot(N, V), 0.0), F0, _roughness);
	float3 kS = F;
	float3 kD = float3(1.0, 1.0, 1.0) - kS;
	kD *= 1.0 - _metalness;

	float3 irradiance = irradianceMap.Sample(envSampler, N).rgb;
	float3 diffuse = kD * irradiance * albedo;

	float3 specularColor = specularMap.SampleLevel(envSampler, R, _roughness * 4).rgb;
		
	float2 maxNVRough = float2(max(dot(N, V), 0.0), _roughness);
	float2 brdf = brdfIntegrationMap.Sample(defaultSampler, maxNVRough).rg;
	
	float3 specular = specularColor * (F * brdf.x + brdf.y);

	float3 ambient = float3(diffuse + specular)*float3(ao,ao,ao);
	
	float3 color = Lo + ambient * 0.2;

	// Tonemap and gamma correct
	color = color / (color + float3(1.0f, 1.0f, 1.0f));
	float gammaCorr = 1.0f / 2.2f;

	color.r = pow(color.r, gammaCorr);
	color.g = pow(color.g, gammaCorr);
	color.b = pow(color.b, gammaCorr);

    return float4(color.r, color.g, color.b, 1.0f);
}