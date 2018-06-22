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

cbuffer cbCamera : register(b0) {
	float4x4 projView;
	float3 camPos;
}

cbuffer cbObject : register(b1) {
	float4x4 worldMat;
	float roughness;
	float metalness;
	int objectId;
}

cbuffer cbLights : register(b2) {
	Light lights[16];
	int currAmountOflights;
}

Texture2D<float2> brdfIntegrationMap : register(t3);
TextureCube<float4> irradianceMap : register(t4);
TextureCube<float4> specularMap : register(t5);
SamplerState envSampler : register(s6);


Texture2D albedoMap : register(t7);
Texture2D normalMap : register(t8);
Texture2D metallicMap : register(t9);
Texture2D roughnessMap : register(t10);
Texture2D aoMap : register(t11);

SamplerState defaultSampler : register(s12);

struct VSOutput {
	float4 Position : SV_POSITION;
	float3 pos : POSITION;
	float3 normal : NORMAL;
	float2 uv :   TEXCOORD0;
};

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



float3 getNormalFromMap(float2 uv,float3 pos,float3 normal)
{

    float3 tangentNormal =  normalMap.Sample(defaultSampler,uv).rgb * 2.0 - 1.0;

    float3 Q1  = ddx(pos);
    float3 Q2  = ddy(pos);
    float2 st1 = ddx(uv);
    float2 st2 = ddy(uv);

    float3 N   = normalize(normal);
    float3 T  = normalize(Q1*st2.g - Q2*st1.g);
    float3 B  = -normalize(cross(N, T));
    float3x3 TBN = float3x3(T, B, N);

    float3 res = mul(tangentNormal,TBN);
    return res;//normalize();
}



float4 main(VSOutput input) : SV_TARGET
{
	// Fixed albedo for now
	float3 albedo = float3(0.5, 0.0, 0.0);
	float _roughness = roughness;
	float _metalness = metalness;
	float ao = 1.0f;

	float3 N = normalize(input.normal);

	if(objectId!=-1) {

		 N = getNormalFromMap(input.uv ,input.pos,input.normal);
		 float3 val = albedoMap.Sample(defaultSampler,input.uv).rgb;
		albedo = float3(pow(val.x,2.2f),pow(val.y,2.2f),pow(val.z,2.2f));
		_metalness   = metallicMap.Sample(defaultSampler, input.uv).r;
		_roughness = roughnessMap.Sample( defaultSampler, input.uv).r;
		ao =   aoMap.Sample(defaultSampler, input.uv).r;

		if(objectId==2) {
	
			albedo  = float3(0.1f, 0.1f, 0.1f);
			_roughness = 2.0f; 
		}else if(objectId==3) {
			albedo  = float3(0.8f, 0.1f, 0.1f);
		}
	}

	

	float3 V = normalize(camPos - input.pos);
	float3 R = reflect(-V, N);


	// 0.04 is the index of refraction for metal
	float3 F0 = float3(0.04f, 0.04f, 0.04f);
	F0 = lerp(F0, albedo, _metalness);

	// Lo = outgoing radiance
	float3 Lo = float3(0.0f, 0.0f, 0.0f);

	for(int i = 0; i < currAmountOflights; ++i)
	{
		float3 L = normalize(lights[i].pos.rgb - input.pos);

		float3 H = normalize(V + L);
		
		float distance = length(lights[i].pos.xyz - input.pos);

		float distanceByRadius = 1.0f - pow((distance / lights[i].radius), 4);
		float clamped = pow(saturate(distanceByRadius), 2.0f);
		float attenuation = clamped / (distance * distance + 1.0f);

		float3 radiance = lights[i].col.xyz * attenuation * lights[i].intensity;

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

		Lo += (kD * albedo / PI + specular) * radiance * NdotL;
	}

	float3 F = FresnelSchlickRoughness(max(dot(N, V), 0.0), F0, _roughness);
	float3 kS = F;
	float3 kD = float3(1.0, 1.0, 1.0) - kS;
	kD *= 1.0 - _metalness;

	float3 irradiance = irradianceMap.Sample(envSampler, N).rgb;
	float3 diffuse = kD * irradiance * albedo;

	float3 specularColor = specularMap.SampleLevel(envSampler, R, _roughness * 4).rgb;

	float2 maxNVRough = max(dot(N, V), _roughness);
	float2 brdf = brdfIntegrationMap.Sample(defaultSampler, maxNVRough);
	
	float3 specular = specularColor * (F * brdf.x + brdf.y);

	float3 ambient = float3(diffuse + specular)*float3(ao,ao,ao);
	
	float3 color = Lo + ambient;

	color = color / (color + float3(1.0f, 1.0f, 1.0f));

	float gammaCorr = 1.0f / 2.2f;

	color.r = pow(color.r, gammaCorr);
	color.g = pow(color.g, gammaCorr);
	color.b = pow(color.b, gammaCorr);

    return float4(color.r, color.g, color.b, 1.0f);
}
