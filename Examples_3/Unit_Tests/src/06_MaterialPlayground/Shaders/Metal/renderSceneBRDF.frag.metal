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
using namespace metal;

struct PointLight
{
	float4 positionAndRadius;
	float4 colorAndIntensity;
};

struct DirectionalLight
{
    float4 directionAndShadowMap;
    float4 colorAndIntensity;
    float shadowRange;
};

static constant float PI = 3.14159265359;

struct CameraData
{
    float4x4 projView;
    float4x4 invProjView;
    float3 camPos;

	int bUseEnvironmentLight;
	float fAmbientIntensity;
	int renderMode;
};

struct ObjectData
{
	float4x4 worldMat;
	float4 albedoAndRoughness;
	float2 tiling;
	float metalness;
	int textureConfig;
};

struct PointLightData
{
	PointLight PointLights[MAX_NUM_POINT_LIGHTS];
	int NumPointLights;
};

struct DirectionalLightData
{
	DirectionalLight DirectionalLights[MAX_NUM_DIRECTIONAL_LIGHTS];
	int NumDirectionalLights;
};

struct VSOutput
{
	float4 position[[position]];
	float3 pos;
	float3 normal;
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

bool HasDiffuseTexture(int textureConfig) { return (textureConfig & (1 << 0)) != 0; }
bool HasNormalTexture(int textureConfig) { return (textureConfig & (1 << 1)) != 0; }
bool HasMetallicTexture(int textureConfig) { return (textureConfig & (1 << 2)) != 0; }
bool HasRoughnessTexture(int textureConfig) { return (textureConfig & (1 << 3)) != 0; }
bool HasAOTexture(int textureConfig) { return (textureConfig & (1 << 4)) != 0; }
bool IsOrenNayarDiffuse(int textureConfig)  { return (textureConfig & (1 << 5)) != 0; }

float3 getNormalFromMap(texture2d<float> normalMap, sampler bilinearSampler, float2 uv, float3 pos, float3 normal)
{
	float3 tangentNormal = normalMap.sample(bilinearSampler, uv).rgb * 2.0 - 1.0;

	float3 Q1 = dfdx(pos);
	float3 Q2 = dfdy(pos);
	float2 st1 = dfdx(uv);
	float2 st2 = dfdy(uv);

	float3 N = normalize(normal);
	float3 T = normalize(Q1 * st2.g - Q2 * st1.g);
	float3 B = -normalize(cross(N, T));
	float3x3 TBN = float3x3(T, B, N);
	float3 res = TBN * tangentNormal;
	return normalize(res);
}


float3 LambertDiffuse(float3 albedo, float3 kD)
{
	return kD * albedo / PI;
}

float3 OrenNayarDiffuse(float3 L, float3 V, float3 N, float roughness)
{
	// src: https://www.gamasutra.com/view/feature/131269/implementing_modular_hlsl_with_.php?page=3

	const float NdotL = max(dot(N, L), 0.0f);
	const float NdotV = max(dot(N, V), 0.0f);

	// gamma: the azimuth (circular angle) between L and V
	//
	const float gamma = dot( normalize(V - N * NdotV), normalize(L - N * NdotL) );

	// alpha and beta represent the polar angles in spherical coord system
	//
	// alpha = min(theta_L, theta_V)
	// beta  = max(theta_L, theta_V)
	//
	// where theta_L is the angle between N and L
	//   and theta_V is the angle between N and V
	//
	const float alpha = max( acos(NdotV), acos(NdotL) );
	const float beta  = min( acos(NdotV), acos(NdotL) );

	const float sigma2 = roughness * roughness;

	// Oren-Nayar constants
	const float A = 1.0f - 0.5f * sigma2 / (sigma2 + 0.33f);
	float B = 0.45f * sigma2 / (sigma2 + 0.09f);
	if (gamma >= 0)
		B *= sin(alpha) * tan(beta);
	else
		B = 0.0f;

	return (A + B);
}

float3 CalculateLightContribution(float3 lightDirection, float3 worldNormal, float3 viewDirection, float3 halfwayVec, float3 radiance, float3 albedo, float roughness, float metalness, float3 F0, int isOrenNayar)
{
	float NDF = distributionGGX(worldNormal, halfwayVec, roughness);
	float G = GeometrySmith(worldNormal, viewDirection, lightDirection, roughness);
	float3 F = fresnelSchlick(max(dot(worldNormal, halfwayVec), 0.0f), F0);

	float3 nominator = NDF * G * F;
	float denominator = 4.0f * max(dot(worldNormal, viewDirection), 0.0f) * max(dot(worldNormal, 	lightDirection), 0.0f) + 0.001f;
	float3 specular = nominator / denominator;

	float3 kS = F;

	float3 kD = float3(1.0f, 1.0f, 1.0f) - kS;

	kD *= 1.0f - metalness;

	float NdotL = max(dot(worldNormal, lightDirection), 0.0f);

	if (isOrenNayar)
		return (OrenNayarDiffuse(lightDirection, viewDirection, worldNormal, roughness) * kD * albedo / PI + specular) * radiance * NdotL;
	else
		return (LambertDiffuse(kD, albedo) + specular) * radiance * NdotL;
}

float3 CalculateDirectionalLightContribution(constant DirectionalLight& light, float3 worldNormal, float3 viewDirection, float3 albedo, float roughness, float metalness, float3 F0, int isOrenNayar)
{
	float3 H = normalize(viewDirection + light.directionAndShadowMap.xyz);
	float3 radiance = light.colorAndIntensity.rgb * light.colorAndIntensity.a;
	return CalculateLightContribution(light.directionAndShadowMap.xyz, worldNormal, viewDirection, H, radiance, albedo, roughness, metalness, F0, isOrenNayar);
}

float3 CalculatePointLightContribution(constant PointLight& light, float3 worldPosition, float3 worldNormal, float3 viewDirection, float3 albedo, float roughness, float metalness, float3 F0)
{
	float3 L = normalize(light.positionAndRadius.xyz - worldPosition);
	float3 H = normalize(viewDirection + L);

	float distance = length(light.positionAndRadius.xyz - worldPosition);
	float distanceByRadius = 1.0f - pow((distance / light.positionAndRadius.w), 4);
	float clamped = pow(saturate(distanceByRadius), 2.0f);
	float attenuation = clamped / (distance * distance + 1.0f);
	float3 radiance = light.colorAndIntensity.rgb * attenuation * light.colorAndIntensity.a;

	return CalculateLightContribution(L, worldNormal, viewDirection, H, radiance, albedo, roughness, metalness, F0, 0);
}

fragment float4 stageMain(VSOutput In[[stage_in]],
						  constant CameraData &cbCamera[[buffer(1)]],
						  constant ObjectData &cbObject[[buffer(2)]],
						  constant PointLightData &cbPointLights[[buffer(3)]],
						  constant DirectionalLightData &cbDirectionalLights[[buffer(4)]],
						  texture2d<float, access::sample> brdfIntegrationMap[[texture(0)]],
						  texturecube<float, access::sample> irradianceMap[[texture(1)]],
						  texture2d<float> albedoMap[[texture(2)]],
						  texture2d<float, access::sample> normalMap[[texture(3)]],
						  texture2d<float, access::sample> metallicMap[[texture(4)]],
						  texture2d<float, access::sample> roughnessMap[[texture(5)]],
						  texture2d<float, access::sample> aoMap[[texture(6)]],
						  texturecube<float, access::sample> specularMap[[texture(7)]],
						  sampler bilinearSampler[[sampler(0)]],
						  sampler bilinearClampedSampler[[sampler(1)]])
{
	// Read material surface properties
	//
	float2 uv = In.uv * cbObject.tiling;
	float3 _albedo = HasDiffuseTexture(cbObject.textureConfig)
		? albedoMap.sample(bilinearSampler, uv).rgb
		: cbObject.albedoAndRoughness.rgb;

	float _roughness = HasRoughnessTexture(cbObject.textureConfig)
		? roughnessMap.sample(bilinearSampler, uv).r
		: cbObject.albedoAndRoughness.a;
	if (_roughness < 0.04) _roughness = 0.04;

	const float _metalness = HasMetallicTexture(cbObject.textureConfig)
		? metallicMap.sample(bilinearSampler, uv).r
		: cbObject.metalness;

	const float _ao = HasAOTexture(cbObject.textureConfig)
		? aoMap.sample(bilinearSampler, uv).r
		: 1.0f;

	const float3 N = HasNormalTexture(cbObject.textureConfig)
		? getNormalFromMap(normalMap, bilinearSampler, uv, In.pos, In.normal)
		: normalize(In.normal);

	_albedo = pow(_albedo, 2.2f);

	const int isOrenNayar = IsOrenNayarDiffuse(cbObject.textureConfig) ? 1 : 0;

	const float3 V = normalize(cbCamera.camPos - In.pos);
	const float3 R = reflect(-V, N);

	// F0 represents the base reflectivity (calculated using IOR: index of refraction)
	float3 F0 = float3(0.04f, 0.04f, 0.04f);
	F0 = mix(F0, _albedo, _metalness);

	// Lo = outgoing radiance
	float3 Lo = float3(0.0f, 0.0f, 0.0f);

	for(int i = 0; i < cbPointLights.NumPointLights; ++i)
		Lo += CalculatePointLightContribution(cbPointLights.PointLights[i], In.pos.xyz, N, V, _albedo, _roughness, _metalness, F0);

	for (int i = 0; i < cbDirectionalLights.NumDirectionalLights; ++i)
		Lo += CalculateDirectionalLightContribution(cbDirectionalLights.DirectionalLights[i], N, V, _albedo, _roughness, _metalness, F0, isOrenNayar);

	float3 F = FresnelSchlickRoughness(max(dot(N, V), 0.0), F0, _roughness);
	float3 kS = F;
	float3 kD = float3(1.0, 1.0, 1.0) - kS;
	kD *= 1.0 - _metalness;

	float3 color = Lo;
	if (cbCamera.bUseEnvironmentLight != 0)
	{
		float3 irradiance = irradianceMap.sample(bilinearSampler, N).rgb;
		float3 diffuse = kD * irradiance * _albedo;

		float3 specularColor = specularMap.sample(bilinearSampler, R, level(_roughness * 4)).rgb;

		float2 maxNVRough = float2(max(dot(N, V), 0.0), _roughness);
		float2 brdf = brdfIntegrationMap.sample(bilinearClampedSampler, maxNVRough).rg;

		float3 specular = specularColor * (F * brdf.x + brdf.y);

		float3 ambient = float3(diffuse + specular) * float3(_ao, _ao, _ao);

		color += ambient;
	}
	else
	{
		float3 ambient = float3(_albedo) * _ao * cbCamera.fAmbientIntensity;
		color += ambient;
	}

	color = color / (color + float3(1.0f, 1.0f, 1.0f));

	float gammaCorr = 1.0f / 2.2f;

	color = pow(color, gammaCorr);
	
	switch (cbCamera.renderMode)
	{
	default:
	case 0: break;
	case 1: color = _albedo; break;
	case 2: color = N; break;
	case 3: color = _roughness; break;
	case 4: color = _metalness; break;
	case 5: color = _ao; break;
	}
	return float4(color, 1.0f);
}
