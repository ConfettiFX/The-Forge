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
	float _pad0;
	float _pad1;
	int shadowMapDimensions;
	float4x4 viewProj;
};

static constant float PI = 3.14159265359;
static constant float PI_DIV2 = 1.57079632679;

struct CameraData
{
    float4x4 projView;
    float4x4 invProjView;
    float3 camPos;

	float fAmbientLightIntensity;
	int bUseEnvironmentLight;
	float fEnvironmentLightIntensity;
	float fAOIntensity;

	int renderMode;
	float fNormalMapIntensity;
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

float3 FresnelSchlick(float cosTheta, float3 F0)
{
	return F0 + (1.0f - F0) * pow(1.0 - cosTheta, 5.0);
}

float DistributionGGX(float3 N, float3 H, float roughness)
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

float3 getNormalFromMap(texture2d<float> normalMap, sampler bilinearSampler, float2 uv, float3 pos, float3 normal, float intensity)
{
	float3 tangentNormal = normalMap.sample(bilinearSampler, uv).rgb * 2.0 - 1.0;

	tangentNormal.xy *= intensity;
	tangentNormal.z = sqrt(1.0f - saturate(dot(tangentNormal.xy, tangentNormal.xy)));

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

float3 OrenNayarDiffuse(float3 L, float3 V, float3 N, float roughness, float3 albedo, float3 kD)
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
		B *= sin(alpha) * clamp(tan(beta), -PI_DIV2, PI_DIV2);
	else
		B = 0.0f;

	return (A + B) * albedo * kD / PI;
}

float3 BRDF(float3 N, float3 V, float3 L, float3 albedo, float roughness, float metalness, int isOrenNayar)
{	
	const float3 H = normalize(V + L);

	// F0 represents the base reflectivity (calculated using IOR: index of refraction)
	float3 F0 = float3(0.04f, 0.04f, 0.04f);
	F0 = mix(F0, albedo, metalness);

	float NDF = DistributionGGX(N, H, roughness);
	float G = GeometrySmith(N, V, L, roughness);
	float3 F = FresnelSchlick(max(dot(N, H), 0.0f), F0);

	float3 kS = F;
	float3 kD = (float3(1.0f, 1.0f, 1.0f) - kS) * (1.0f - metalness);

	// Id & Is: diffuse & specular illumination
	float3 Is = NDF * G * F / (4.0f * max(dot(N, V), 0.0f) * max(dot(N, L), 0.0f) + 0.001f);
	float3 Id = float3(0,0,0);
	if (isOrenNayar)
		//Id = OrenNayarDiffuse(L, V, N, roughness, albedo, kD);
		return OrenNayarDiffuse(L, V, N, roughness, albedo, kD);
	else
		Id = LambertDiffuse(albedo, kD);

	return Id + Is;
}

float3 EnvironmentBRDF(texturecube<float> irradianceMap, texturecube<float> specularMap, texture2d<float> brdfIntegrationMap, sampler bilinearSampler, sampler bilinearClampedSampler, float3 N, float3 V, float3 albedo, float roughness, float metalness)
{
	const float3 R = reflect(-V, N);

	// F0 represents the base reflectivity (calculated using IOR: index of refraction)
	float3 F0 = float3(0.04f, 0.04f, 0.04f);
	F0 = mix(F0, albedo, metalness);

	float3 F = FresnelSchlickRoughness(max(dot(N, V), 0.0f), F0, roughness);

	float3 kS = F;
	float3 kD = (float3(1.0f, 1.0f, 1.0f) - kS) * (1.0f - metalness);

	float3 irradiance = irradianceMap.sample(bilinearSampler, N).rgb;
	float3 specular = specularMap.sample(bilinearSampler, R, level(roughness * 4)).rgb;

	float2 maxNVRough = float2(max(dot(N, V), 0.0), roughness);
	float2 brdf = brdfIntegrationMap.sample(bilinearClampedSampler, maxNVRough).rg;

	// Id & Is: diffuse & specular illumination
	float3 Is = specular * (F * brdf.x + brdf.y);
	float3 Id = kD * irradiance * albedo;
	
	//if (isOrenNayar)
	//	Id = OrenNayarDiffuse(L, V, N, roughness, albedo, kD);
	//else
	//	Id = LambertDiffuse(albedo, kD);

	return (Id + Is);
}

float ShadowTest(texture2d<float> shadowMap, sampler bilinearSampler, float4 Pl, float2 shadowMapDimensions)
{
	// homogenous position after perspective divide
	const float3 projLSpaceCoords = Pl.xyz / Pl.w;

	// light frustum check
	if (projLSpaceCoords.x < -1.0f || projLSpaceCoords.x > 1.0f ||
		projLSpaceCoords.y < -1.0f || projLSpaceCoords.y > 1.0f ||
		projLSpaceCoords.z <  0.0f || projLSpaceCoords.z > 1.0f
		)
	{
		return 1.0f;
	}

	const float2 texelSize = 1.0f / (shadowMapDimensions);
	
	// clip space [-1, 1] --> texture space [0, 1]
	const float2 shadowTexCoords = float2(0.5f, 0.5f) + projLSpaceCoords.xy * float2(0.5f, -0.5f);	// invert Y

	const float BIAS = 0.00001f;
	const float pxDepthInLSpace = projLSpaceCoords.z;

	float shadow = 0.0f;
	const int rowHalfSize = 2;

	// PCF
	for (int x = -rowHalfSize; x <= rowHalfSize; ++x)
	{
		for (int y = -rowHalfSize; y <= rowHalfSize; ++y)
		{
			float2 texelOffset = float2(x, y) * texelSize;
			float closestDepthInLSpace = shadowMap.sample(bilinearSampler, shadowTexCoords + texelOffset).x;	// TODO point sampler

			// depth check
			shadow += (pxDepthInLSpace - BIAS> closestDepthInLSpace) ? 1.0f : 0.0f;
		}
	}

	shadow /= (rowHalfSize * 2 + 1) * (rowHalfSize * 2 + 1);
	return 1.0 - shadow;
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
						  texture2d<float, access::sample> shadowMap[[texture(7)]],
						  texturecube<float, access::sample> specularMap[[texture(8)]],
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
		? getNormalFromMap(normalMap, bilinearSampler, uv, In.pos, In.normal, cbCamera.fNormalMapIntensity)
		: normalize(In.normal);

	_albedo = pow(_albedo, 2.2f);

	const int isOrenNayar = IsOrenNayarDiffuse(cbObject.textureConfig) ? 1 : 0;

	const float3 P = In.pos.xyz;
	const float3 V = normalize(cbCamera.camPos - P);
	
	float3 Lo = float3(0.0f, 0.0f, 0.0f);	// outgoing radiance

	// Direct Lights
	int i;
	for (i = 0; i < cbPointLights.NumPointLights; ++i)
	{
		const float3 Pl = cbPointLights.PointLights[i].positionAndRadius.xyz;
		const float3 L = normalize(Pl - P);
		const float NdotL = max(dot(N, L), 0.0f);
		const float distance = length(Pl - P);
		const float distanceByRadius = 1.0f - pow((distance / cbPointLights.PointLights[i].positionAndRadius.w), 4);
		const float clamped = pow(saturate(distanceByRadius), 2.0f);
		const float attenuation = clamped / (distance * distance + 1.0f);
		const float3 radiance = cbPointLights.PointLights[i].colorAndIntensity.rgb * cbPointLights.PointLights[i].colorAndIntensity.w * attenuation;

		Lo += BRDF(N, V, L, _albedo, _roughness, _metalness, isOrenNayar) * radiance * NdotL;
	}

	for (i = 0; i < cbDirectionalLights.NumDirectionalLights; ++i)
	{
		const float3 L = cbDirectionalLights.DirectionalLights[i].directionAndShadowMap.xyz;
		const float4 P_lightSpace =  cbDirectionalLights.DirectionalLights[i].viewProj * float4(P, 1.0f);
		const float NdotL = max(dot(N, L), 0.0f);
		const float3 radiance = cbDirectionalLights.DirectionalLights[i].colorAndIntensity.rgb * cbDirectionalLights.DirectionalLights[i].colorAndIntensity.w;
		const float shadowing = ShadowTest(shadowMap, bilinearSampler, P_lightSpace, cbDirectionalLights.DirectionalLights[i].shadowMapDimensions);

		Lo += BRDF(N, V, L, _albedo, _roughness, _metalness, isOrenNayar) * radiance * NdotL * shadowing;
	}


	// Environment Lighting
	if (cbCamera.bUseEnvironmentLight != 0)
	{
		Lo += EnvironmentBRDF(irradianceMap, specularMap, brdfIntegrationMap, bilinearSampler, bilinearClampedSampler, N, V, _albedo, _roughness, _metalness) * float3(_ao, _ao, _ao) * cbCamera.fEnvironmentLightIntensity;
	}
	else
	{
		if(HasAOTexture(cbObject.textureConfig))
			Lo += _albedo * (_ao  * cbCamera.fAOIntensity + (1.0f - cbCamera.fAOIntensity)) * cbCamera.fAmbientLightIntensity;
	}
	
	// Gamma correction
	float3 color = Lo;
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
	case 5: color = (_ao  * cbCamera.fAOIntensity + (1.0f - cbCamera.fAOIntensity)); break;
	}
	return float4(color, 1.0f);
}
