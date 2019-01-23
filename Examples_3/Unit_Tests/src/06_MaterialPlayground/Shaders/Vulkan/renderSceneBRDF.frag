#version 450 core

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
	vec3 pos;
	float radius;
	vec3 col;
	float intensity;
};

struct DirectionalLight
{
	vec3 direction;
	int shadowMap;
	vec3 color;
	float intensity;
	float shadowRange;
	float _pad0;
	float _pad1;
	float _pad2;
};

layout(set = 0, binding = 0) uniform cbCamera 
{
	mat4 projView;
	mat4 invProjView;
	vec3 camPos;
	float _dumm;
	int bUseEnvironmentLight;
	float fAmbientIntensity;
	int renderMode;
};

layout (set = 0, binding = 1) uniform cbObject 
{
	mat4 worldMat;
	vec3 albedo;
	float roughness;
	vec2 tiling;
	float metalness;
	int textureConfig;
};

layout (set=0, binding=2) uniform cbPointLights 
{
	PointLight PointLights[MAX_NUM_POINT_LIGHTS];
	uint NumPointLights;
};

layout(set = 0, binding = 13) uniform cbDirectionalLights
{
	DirectionalLight DirectionalLights[MAX_NUM_DIRECTIONAL_LIGHTS];
	uint NumDirectionalLights;
};

const float PI = 3.14159265359;


vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
	return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
}

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
	return F0 + (vec3(1.0f) - F0) * pow(1.0 - cosTheta, 5.0);
}

float distributionGGX(vec3 N, vec3 H, float roughness)
{
	float a = roughness*roughness;
	float a2 = a*a;
	float NdotH = max(dot(N,H), 0.0);
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

	return nom/denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
	float NdotV = max(dot(N, V), 0.0);
	float NdotL = max(dot(N, L), 0.0);
	float ggx2 = GeometrySchlickGGX(NdotV, roughness);
	float ggx1 = GeometrySchlickGGX(NdotL, roughness);

	return ggx1 * ggx2;
}


layout(location = 0) in vec3 normal;
layout(location = 1) in vec3 pos;
layout(location = 2) in vec2 uv;

layout(location = 0) out vec4 outColor;


layout(set = 0, binding = 3) uniform texture2D brdfIntegrationMap;
layout(set = 0, binding = 4) uniform textureCube irradianceMap;
layout(set = 0, binding = 5) uniform textureCube  specularMap;

// material parameters
layout(set = 0, binding = 7)  uniform texture2D albedoMap;
layout(set = 0, binding = 8)  uniform texture2D normalMap;
layout(set = 0, binding = 9)  uniform texture2D metallicMap;
layout(set = 0, binding = 10) uniform texture2D roughnessMap;
layout(set = 0, binding = 11) uniform texture2D aoMap;

layout(set = 0, binding = 12) uniform sampler bilinearSampler;
layout(set = 0, binding = 6) uniform sampler bilinearClampedSampler;

bool HasDiffuseTexture(int textureConfig) { return (textureConfig & (1 << 0)) != 0; }
bool HasNormalTexture(int textureConfig) { return (textureConfig & (1 << 1)) != 0; }
bool HasMetallicTexture(int textureConfig) { return (textureConfig & (1 << 2)) != 0; }
bool HasRoughnessTexture(int textureConfig) { return (textureConfig & (1 << 3)) != 0; }
bool HasAOTexture(int textureConfig) { return (textureConfig & (1 << 4)) != 0; }

bool IsOrenNayarDiffuse(int textureConfig) { return (textureConfig & (1 << 5)) != 0; }

vec3 LambertDiffuse(vec3 albedo, vec3 kD)
{
	return kD * albedo / PI;
}

float OrenNayarDiffuse(vec3 L, vec3 V, vec3 N, float roughness)
{
	// src: https://www.gamasutra.com/view/feature/131269/implementing_modular_hlsl_with_.php?page=3

	const float NdotL = max(dot(N, L), 0.0f);
	const float NdotV = max(dot(N, V), 0.0f);

	// gamma: the azimuth (circular angle) between L and V
	//
	const float gamma = max( 0.0f, dot(normalize(V - N * NdotV), normalize(L - N * NdotL)) );

	// alpha and beta represent the polar angles in spherical coord system
	//
	// alpha = min(theta_L, theta_V)
	// beta  = max(theta_L, theta_V)
	//
	// where theta_L is the angle between N and L
	//   and theta_V is the angle between N and V
	//
	const float alpha = max(acos(NdotV), acos(NdotL));
	const float beta  = min(acos(NdotV), acos(NdotL));

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


vec3 getNormalFromMap()
{
	vec3 tangentNormal = texture(sampler2D(normalMap, bilinearSampler),uv).xyz * 2.0 - 1.0;

	vec3 Q1  = dFdx(pos);
	vec3 Q2  = dFdy(pos);
	vec2 st1 = dFdx(uv);
	vec2 st2 = dFdy(uv);

	vec3 N   = normalize(normal);
	vec3 T  = normalize(Q1*st2.t - Q2*st1.t);
	vec3 B  = -normalize(cross(N, T));
	mat3 TBN = mat3(T, B, N);

	return normalize(TBN * tangentNormal);
}

vec3 CalculateLightContribution(vec3 lightDirection, vec3 worldNormal, vec3 viewDirection, vec3 halfwayVec, vec3 radiance, vec3 albedo, float roughness, float metalness, vec3 F0, int isOrenNayar)
{
	float NDF = distributionGGX(worldNormal, halfwayVec, roughness);
	float G = GeometrySmith(worldNormal, viewDirection, lightDirection, roughness);
	vec3 F = fresnelSchlick(max(dot(worldNormal, halfwayVec), 0.0f), F0);

	vec3 nominator = NDF * G * F;
	float denominator = 4.0f * max(dot(worldNormal, viewDirection), 0.0f) * max(dot(worldNormal, lightDirection), 0.0f) + 0.001f;
	vec3 specular = nominator / denominator;

	vec3 kS = F;

	vec3 kD = vec3(1.0f, 1.0f, 1.0f) - kS;

	kD *= 1.0f - metalness;

	float NdotL = max(dot(worldNormal, lightDirection), 0.0f);

	if (isOrenNayar != 0)
		return (OrenNayarDiffuse(lightDirection, viewDirection, worldNormal, roughness) * kD * albedo / PI + specular) * radiance * NdotL;
	else
		return (LambertDiffuse(kD, albedo) + specular) * radiance * NdotL;
}

vec3 CalculateDirectionalLightContribution(uint lightIndex, vec3 worldNormal, vec3 viewDirection, vec3 albedo, float roughness, float metalness, vec3 F0, int isOrenNayar)
{
	const DirectionalLight light = DirectionalLights[lightIndex];
	vec3 H = normalize(viewDirection + light.direction);
	vec3 radiance = light.color * light.intensity;
	return CalculateLightContribution(normalize(light.direction), worldNormal, viewDirection, H, radiance, albedo, roughness, metalness, F0, isOrenNayar);
}

vec3 CalculatePointLightContribution(uint lightIndex, vec3 worldPosition, vec3 worldNormal, vec3 viewDirection, vec3 albedo, float roughness, float metalness, vec3 F0)
{
	const PointLight light = PointLights[lightIndex];
	vec3 L = normalize(light.pos - worldPosition);
	vec3 H = normalize(viewDirection + L);

	float distance = length(light.pos - worldPosition);
	float distanceByRadius = 1.0f - pow((distance / light.radius), 4);
	float clamped = pow(clamp(distanceByRadius, 0.0f, 1.0f), 2.0f);
	float attenuation = clamped / (distance * distance + 1.0f);
	vec3 radiance = light.col.rgb * attenuation * light.intensity;

	return CalculateLightContribution(L, worldNormal, viewDirection, H, radiance, albedo, roughness, metalness, F0, 0);
}

void main()
{
	// Read material surface properties
	//
	vec3 _albedo = HasDiffuseTexture(textureConfig)
		? texture(sampler2D(albedoMap, bilinearSampler), uv).rgb
		: albedo.rgb;

	const float _roughness = HasRoughnessTexture(textureConfig)
		? texture(sampler2D(roughnessMap, bilinearSampler), uv).r
		: roughness;

	const float _metalness = HasMetallicTexture(textureConfig)
		? texture(sampler2D(metallicMap, bilinearSampler), uv).r
		: metalness;

	const float _ao = HasAOTexture(textureConfig)
		? texture(sampler2D(aoMap, bilinearSampler), uv).r
		: 1.0f;

	const vec3 N = HasNormalTexture(textureConfig)
		? getNormalFromMap()
		: normalize(normal);

	_albedo = pow(_albedo, vec3(2.2f, 2.2f, 2.2f));

	const int isOrenNayar = IsOrenNayarDiffuse(textureConfig) ? 1 : 0;

	const vec3 V = normalize(camPos - pos);
	const vec3 R = reflect(-V, N);


	// F0 represents the base reflectivity (calculated using IOR: index of refraction)
	vec3 F0 = vec3(0.04f, 0.04f, 0.04f);
	F0 = mix(F0, _albedo, _metalness);

	// Lo = outgoing radiance
	vec3 Lo = vec3(0.0f, 0.0f, 0.0f);

	for (int i = 0; i < NumPointLights; ++i)
		Lo += CalculatePointLightContribution(i, pos.xyz, N, V, _albedo, _roughness, _metalness, F0);

	for (int i = 0; i < NumDirectionalLights; ++i)
		Lo += CalculateDirectionalLightContribution(i, N, V, _albedo, _roughness, _metalness, F0, isOrenNayar);

	vec3 F = FresnelSchlickRoughness(max(dot(N, V), 0.0f), F0, _roughness);
	vec3 kS = F;
	vec3 kD = vec3(1.0f, 1.0f, 1.0f) - kS;
	kD *= 1.0f - _metalness;

	vec3 color = Lo;
	if (bUseEnvironmentLight != 0)
	{
		vec3 irradiance = texture(samplerCube(irradianceMap, bilinearSampler), N).rgb;
		vec3 diffuse = kD * irradiance * _albedo;

		vec3 specularColor = textureLod(samplerCube(specularMap, bilinearSampler), R, _roughness * 4).rgb;

		vec2 maxNVRough = vec2(max(dot(N, V), 0.0), _roughness);
		vec2 brdf = texture(sampler2D(brdfIntegrationMap, bilinearClampedSampler), maxNVRough).rg;

		vec3 specular = specularColor * (F * brdf.x + brdf.y);

		vec3 ambient = vec3(diffuse + specular) * _ao;

		color += ambient;
	}
	else
	{
		vec3 ambient = _albedo * _ao * fAmbientIntensity;
		color += ambient;
	}

	color = color / (color + vec3(1.0f, 1.0f, 1.0f));

	float gammaCorr = 1.0f / 2.2f;
	color = pow(color, vec3(gammaCorr));

	switch (renderMode)
	{
	default:
	case 0: break;
	case 1: color = _albedo; break;
	case 2: color = N; break;
	case 3: color = vec3(_roughness); break;
	case 4: color = vec3(_metalness); break;
	case 5: color = vec3(_ao); break;
	}
	outColor = vec4(color, 1.0f);
	
}
