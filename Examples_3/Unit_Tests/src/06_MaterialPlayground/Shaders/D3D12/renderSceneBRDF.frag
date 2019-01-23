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

struct PointLight
{
	float3 pos;
	float radius;
	float3 col;
	float intensity;
};


struct DirectionalLight
{
	float3 direction;
	int shadowMap;
	float3 color;
	float intensity;
	float shadowRange;
	float _pad0;
	float _pad1;
	float _pad2;
};

static const float PI = 3.14159265359;


//
// SHADER INTERFACE
//
cbuffer cbCamera : register(b0) 
{
	float4x4 projView;
	float4x4 invProjView;
	float3 camPos;
	float __dumm;

	int bUseEnvironmentLight;
	float fAmbientIntensity;
	int renderMode;
}

cbuffer cbObject : register(b1) 
{
	float4x4 worldMat;
	float3 albedo;
	float roughness;
	float2 tiling;
	float metalness;

	// specifies which texture maps are to be used 
	// instead of the constant buffer values above.
	int textureConfig;
}

cbuffer cbPointLights : register(b2) 
{
	PointLight PointLights[MAX_NUM_POINT_LIGHTS];
	uint NumPointLights;
}

cbuffer cbDirectionalLights : register(b3)
{
	DirectionalLight DirectionalLights[MAX_NUM_DIRECTIONAL_LIGHTS];
	uint NumDirectionalLights;
}

Texture2D<float2> brdfIntegrationMap : register(t3);
TextureCube<float4> irradianceMap : register(t4);
TextureCube<float4> specularMap : register(t5);

Texture2D albedoMap : register(t7);
Texture2D normalMap : register(t8);
Texture2D metallicMap : register(t9);
Texture2D roughnessMap : register(t10);
Texture2D aoMap : register(t11);

SamplerState bilinearSampler : register(s12);
SamplerState bilinearClampedSampler : register(s6);

struct VSOutput
{
	float4 Position : SV_POSITION;
	float3 pos : POSITION;
	float3 normal : NORMAL;
	float2 uv : TEXCOORD0;
};




//
// UTILITY FUNCTIONS
//
inline bool HasDiffuseTexture(int textureConfig)   { return (textureConfig & (1 << 0)) != 0; }
inline bool HasNormalTexture(int textureConfig)    { return (textureConfig & (1 << 1)) != 0; }
inline bool HasMetallicTexture(int textureConfig)  { return (textureConfig & (1 << 2)) != 0; }
inline bool HasRoughnessTexture(int textureConfig) { return (textureConfig & (1 << 3)) != 0; }
inline bool HasAOTexture(int textureConfig)        { return (textureConfig & (1 << 4)) != 0; }

inline bool IsOrenNayarDiffuse(int textureConfig)  { return (textureConfig & (1 << 5)) != 0; }

inline float3 UnpackNormals(float2 uv, float3 pos, in Texture2D normalMap, in SamplerState samplerState, float3 normal)
{
	float3 tangentNormal = normalMap.Sample(samplerState, uv).rgb * 2.0 - 1.0;

	float3 Q1 = ddx(pos);
	float3 Q2 = ddy(pos);
	float2 st1 = ddx(uv);
	float2 st2 = ddy(uv);

	float3 N = normalize(normal);
	float3 T = normalize(Q1*st2.g - Q2 * st1.g);
	float3 B = normalize(cross(T, N));
	float3x3 TBN = float3x3(T, B, N);

	float3 res = mul(tangentNormal, TBN);
	return res;//normalize();
}

// in case we have Tangent vectors in the vertex data
inline float3 UnpackNormals(float2 uv, in Texture2D normalMap, in SamplerState samplerState, float3 vertexNormal, float3 vertexTangent)
{
	// uncompressed normal in tangent space
	const float3 SampledNormal = normalMap.Sample(samplerState, uv).xyz * 2.0f - 1.0f;

	const float3 T = normalize(vertexTangent - dot(vertexNormal, vertexTangent) * vertexNormal);
	const float3 N = normalize(vertexNormal);
	const float3 B = normalize(cross(T, N));
	const float3x3 TBN = float3x3(T, B, N);
	return mul(SampledNormal, TBN);
}




//
// LIGHTING FUNCTIONS
//
float3 FresnelSchlickRoughness(float cosTheta, float3 F0, float roughness)
{
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

float3 LambertDiffuse(float3 albedo, float3 kD)
{
	return kD * albedo / PI;
}

float OrenNayarDiffuse(float3 L, float3 V, float3 N, float roughness)
{
	// src: https://www.gamasutra.com/view/feature/131269/implementing_modular_hlsl_with_.php?page=3

	const float NdotL = max(dot(N, L), 0.0f);
	const float NdotV = max(dot(N, V), 0.0f);

	// gamma: the azimuth (circular angle) between L and V
	//
	const float gamma = max(0.0f, dot( normalize(V - N * NdotV), normalize(L - N * NdotL) ));

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


struct LightingParams
{
	float3 position;
	float3 normal;
	float3 view;

	float3 albedo;
	float roughness;
	float metalness;
	float ao;

	int isOrenNayar;
};

float3 CalculateLightContribution(float3 lightDirection, float3 worldNormal, float3 viewDirection, float3 halfwayVec, float3 radiance, float3 albedo, float roughness, float metalness, float3 F0, int isOrenNayar)
{
	float NDF = distributionGGX(worldNormal, halfwayVec, roughness);
	float G = GeometrySmith(worldNormal, viewDirection, lightDirection, roughness);
	float3 F = fresnelSchlick(max(dot(worldNormal, halfwayVec), 0.0f), F0);

	float3 nominator = NDF * G * F;
	float denominator = 4.0f * max(dot(worldNormal, viewDirection), 0.0f) * max(dot(worldNormal, lightDirection), 0.0f) + 0.001f;
	//if (isOrenNayar)
	//{
	//	nominator = NDF * G;
	//	denominator = 4.0f * max(dot(worldNormal, lightDirection), 0.0f) + 0.001f;
	//}
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

float3 CalculateDirectionalLightContribution(uint lightIndex, float3 worldNormal, float3 viewDirection, float3 albedo, float roughness, float metalness, float3 F0, int isOrenNayar)
{
	const DirectionalLight light = DirectionalLights[lightIndex];
	float3 H = normalize(viewDirection + light.direction);
	float3 radiance = light.color * light.intensity;
	return CalculateLightContribution(light.direction, worldNormal, viewDirection, H, radiance, albedo, roughness, metalness, F0, isOrenNayar);
}

float3 CalculatePointLightContribution(uint lightIndex, float3 worldPosition, float3 worldNormal, float3 viewDirection, float3 albedo, float roughness, float metalness, float3 F0)
{
	const PointLight light = PointLights[lightIndex];
	float3 L = normalize(light.pos - worldPosition);
	float3 H = normalize(viewDirection + L);

	float distance = length(light.pos - worldPosition);
	float distanceByRadius = 1.0f - pow((distance / light.radius), 4);
	float clamped = pow(saturate(distanceByRadius), 2.0f);
	float attenuation = clamped / (distance * distance + 1.0f);
	float3 radiance = light.col.rgb * attenuation * light.intensity;

	return CalculateLightContribution(L, worldNormal, viewDirection, H, radiance, albedo, roughness, metalness, F0, 0);
}


float4 main(VSOutput input) : SV_TARGET
{
	// Read material surface properties
	//
	const float2 uv = input.uv * tiling;
	float3 _albedo = HasDiffuseTexture(textureConfig)
		? albedoMap.Sample(bilinearSampler, uv).rgb
		: albedo;

	float _roughness = HasRoughnessTexture(textureConfig)
		? roughnessMap.Sample(bilinearSampler, uv).r
		: roughness;
	if (_roughness < 0.04) _roughness = 0.04;

	const float _metalness = HasMetallicTexture(textureConfig) 
		? metallicMap.Sample(bilinearSampler, uv).r
		: metalness;

	const float _ao = HasAOTexture(textureConfig) 
		? aoMap.Sample(bilinearSampler, uv).r
		: 1.0f;

	const float3 N = HasNormalTexture(textureConfig)
		? UnpackNormals(uv, input.pos, normalMap, bilinearSampler, input.normal)
		: normalize(input.normal);

	_albedo = pow(_albedo, 2.2f);
	
	const int isOrenNayar = IsOrenNayarDiffuse(textureConfig) ? 1 : 0;

	const float3 V = normalize(camPos - input.pos);
	const float3 R = reflect(-V, N);


	// F0 represents the base reflectivity (calculated using IOR: index of refraction)
	float3 F0 = float3(0.04f, 0.04f, 0.04f);
	F0 = lerp(F0, _albedo, _metalness);

	// Lo = outgoing radiance
	float3 Lo = float3(0.0f, 0.0f, 0.0f);

	uint i;
	for(i = 0; i < NumPointLights; ++i)
		Lo += CalculatePointLightContribution(i, input.pos.xyz, N, V, _albedo, _roughness, _metalness, F0);

	for (i = 0; i < NumDirectionalLights; ++i)
		Lo += CalculateDirectionalLightContribution(i, N, V, _albedo, _roughness, _metalness, F0, isOrenNayar);

	float3 F = FresnelSchlickRoughness(max(dot(N, V), 0.0), F0, _roughness);
	float3 kS = F;
	float3 kD = float3(1.0, 1.0, 1.0) - kS;
	kD *= 1.0 - _metalness;


	float3 color = Lo;
	if (bUseEnvironmentLight != 0)
	{
		float3 irradiance = irradianceMap.Sample(bilinearSampler, N).rgb;
		float3 diffuse = kD * irradiance * _albedo;

		float3 specularColor = specularMap.SampleLevel(bilinearSampler, R, _roughness * 4).rgb;

		float2 maxNVRough = float2(max(dot(N, V), 0.0), _roughness);
		float2 brdf = brdfIntegrationMap.Sample(bilinearClampedSampler, maxNVRough).rg;

		float3 specular = specularColor * (F * brdf.x + brdf.y);

		float3 ambient = float3(diffuse + specular) * float3(_ao, _ao, _ao);

		color += ambient;
	}
	else
	{
		float3 ambient = float3(_albedo) * _ao * fAmbientIntensity;
		color += ambient;
	}

	color = color / (color + float3(1.0f, 1.0f, 1.0f));
	float gammaCorr = 1.0f / 2.2f;

	color.r = pow(color.r, gammaCorr);
	color.g = pow(color.g, gammaCorr);
	color.b = pow(color.b, gammaCorr);


	switch (renderMode)
	{
	default:
	case 0: break;
	case 1: color = _albedo; break;
	case 2: color = N; break;
	case 3: color = _roughness; break;
	case 4: color = _metalness; break;
	case 5: color = _ao; break;
	}
	return float4(color.r, color.g, color.b, 1.0f);
}
