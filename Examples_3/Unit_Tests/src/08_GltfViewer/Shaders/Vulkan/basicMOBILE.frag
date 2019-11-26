/*
 * Copyright (c) 2018 Kostas Anagnostou (https://twitter.com/KostasAAA).
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

#define PI 3.141592654f


#define NUM_SHADOW_SAMPLES 4

const float NUM_SHADOW_SAMPLES_INV = 0.125;
const float shadowSamples[NUM_SHADOW_SAMPLES * 8] =
{
	-0.1746646, -0.7913184,
	0.08863912, -0.898169,
	0.1748409, -0.5252063,
	0.4529319, -0.384986,
	0.3857658, -0.9096935,
	0.768011, -0.4906538,
	0.6946555, 0.1605866,
	0.7986544, 0.5325912,
	0.2847693, 0.2293397,
	-0.4357893, -0.3808875,
	-0.139129, 0.2394065,
	0.4287854, 0.899425,
	-0.6924323, -0.2203967,
	-0.2611724, 0.7359962,
	-0.850104, 0.1263935,
	-0.5380967, 0.6264234
};

layout(std140, UPDATE_FREQ_PER_FRAME, binding = 0) uniform cbPerPass
{
	uniform mat4    projView;
	uniform vec4    camPos;
    uniform vec4    lightColor[4];
    uniform vec4    lightDirection[3];
	uniform ivec4   quantizationParams;
};

layout(std140, UPDATE_FREQ_PER_DRAW, binding = 0) uniform cbPerProp
{
	uniform mat4  world;
	uniform mat4  InvTranspose;
	uniform int   unlit;
	uniform int   hasAlbedoMap;
	uniform int   hasNormalMap;
	uniform int   hasMetallicRoughnessMap;
	uniform int   hasAOMap;
	uniform int   hasEmissiveMap;
	uniform vec4  centerOffset;
	uniform vec4  posOffset;
	uniform vec2  uvOffset;
	uniform vec2  uvScale;
  uniform float posScale;
  uniform uint  textureMapInfo;

	uniform uint  sparseTextureMapInfo;
  uniform float padding00;
  uniform float padding01;
  uniform float padding02;
};

layout(std140, UPDATE_FREQ_PER_FRAME, binding = 2) uniform ShadowUniformBuffer
{
	uniform mat4    LightViewProj;
};

layout(UPDATE_FREQ_PER_DRAW, binding = 1) uniform texture2D albedoMap;
layout(UPDATE_FREQ_PER_DRAW, binding = 2) uniform texture2D normalMap;
layout(UPDATE_FREQ_PER_DRAW, binding = 3) uniform texture2D metallicRoughnessMap;
layout(UPDATE_FREQ_PER_DRAW, binding = 4) uniform texture2D aoMap;
layout(UPDATE_FREQ_PER_DRAW, binding = 5) uniform texture2D emissiveMap;

layout(UPDATE_FREQ_PER_DRAW, binding = 6)  uniform sampler samplerAlbedo;
layout(UPDATE_FREQ_PER_DRAW, binding = 7)  uniform sampler samplerNormal;
layout(UPDATE_FREQ_PER_DRAW, binding = 8)  uniform sampler samplerMR;
layout(UPDATE_FREQ_PER_DRAW, binding = 9)  uniform sampler samplerAO;
layout(UPDATE_FREQ_PER_DRAW, binding = 10) uniform sampler samplerEmissive;

layout(UPDATE_FREQ_NONE, binding = 14) uniform texture2D ShadowTexture;
layout(UPDATE_FREQ_NONE, binding = 16) uniform sampler clampMiplessLinearSampler;

layout(location = 0) in vec3 worldPosition;
layout(location = 1) in vec3 worldNormal;
layout(location = 2) in vec2 InUV;
layout(location = 3) in vec4 InBaseColor;
layout(location = 4) in vec2 InMetallicRoughness;
layout(location = 5) in vec2 InAlphaSettings;

layout(location = 0) out vec4 OutColor;

vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
	return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
}

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
	return F0 + (1.0f - F0) * pow(1.0 - cosTheta, 5.0);
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

// Appoximation of joint Smith term for GGX
// [Heitz 2014, "Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs"]
float Vis_SmithJointApprox(float a, float NoV, float NoL)
{	
	float Vis_SmithV = NoL * (NoV * (1.0f - a) + a);
	float Vis_SmithL = NoV * (NoL * (1.0f - a) + a);
	return 0.5 * (1.0 / max(Vis_SmithV + Vis_SmithL, 0.001));
}

vec3 reconstructNormal(in vec4 sampleNormal)
{
	vec3 tangentNormal;
	tangentNormal.xy = sampleNormal.rg * 2 - 1;
	tangentNormal.z = sqrt(1.0f - clamp(dot(tangentNormal.xy, tangentNormal.xy), 0.0f, 1.0f));
	return tangentNormal;
}

vec3 getNormalFromMap()
{
    vec3 tangentNormal = reconstructNormal(texture(sampler2D(normalMap, samplerNormal),InUV));

    vec3 Q1  = dFdx(worldPosition);
    vec3 Q2  = dFdy(worldPosition);
    vec2 st1 = dFdx(InUV);
    vec2 st2 = dFdy(InUV);

    vec3 N   = normalize(worldNormal);
    vec3 T  = normalize(Q1*st2.t - Q2*st1.t);
	T = normalize(T);

	if(isnan(T.x) ||isnan(T.y) || isnan(T.z))
	{
		vec3 UpVec = abs(N.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(0.0, 0.0, 1.0);
		T = normalize(cross(N, UpVec));
	}

    vec3 B  = normalize(cross(T, N));
    mat3 TBN = mat3(T, B, N);

    return normalize(TBN * tangentNormal);
}

vec3 ComputeLight(vec3 albedo, vec3 lightColor,
vec3 metalness, float roughness,
vec3 N, vec3 L, vec3 V, vec3 H, float NoL, float NoV,
uint alphaMode)
{
	float a  = roughness * roughness;

	// 0.04 is the index of refraction for metal
	vec3 F0 = vec3(0.04f, 0.04f, 0.04f);
	vec3 diffuse = (1.0 - metalness) * albedo;

	float NDF = distributionGGX(N, H, roughness);
	float G = Vis_SmithJointApprox(a, NoV, NoL);
	vec3 F = fresnelSchlick(max(dot(N, H), 0.0f), mix(F0, albedo, metalness));	
	vec3 specular = NDF * G * F;
	
	// To intensify Fresnel
	vec3 F2 = fresnelSchlick(max(dot(N, V), 0.0f), F0);
	specular += F2;

	vec3 irradiance = vec3(lightColor.r,lightColor.g,lightColor.b) * vec3(1.0, 1.0, 1.0);
	vec3 result = (diffuse + specular) * NoL * irradiance;	

	// Do not Light alpha blended materials
	if (alphaMode != 0 || unlit != 0)
		result = albedo;

	return result;
}

float random(vec3 seed, vec3 freq)
{
	// project seed on random constant vector
	float dt = dot(floor(seed * freq), vec3(53.1215, 21.1352, 9.1322));
	// return only the fractional part
	return fract(sin(dt) * 2105.2354);
}

float CalcPCFShadowFactor(vec3 worldPos)
{
	vec4 posLS = LightViewProj * vec4(worldPos.xyz, 1.0);
	posLS /= posLS.w;
	posLS.y *= -1;
	posLS.xy = posLS.xy * 0.5 + vec2(0.5, 0.5);


	vec2 HalfGaps = vec2(0.00048828125, 0.00048828125);
	vec2 Gaps = vec2(0.0009765625, 0.0009765625);

	posLS.xy += HalfGaps;

	float shadowFactor = 1.0;

		float shadowFilterSize = 0.0016;
		float angle = random(worldPos, vec3(20.0));
		float s = sin(angle);
		float c = cos(angle);

		for (int i = 0; i < NUM_SHADOW_SAMPLES; i++)
		{
			vec2 offset = vec2(shadowSamples[i * 2], shadowSamples[i * 2 + 1]);
			offset = vec2(offset.x * c + offset.y * s, offset.x * -s + offset.y * c);
			offset *= shadowFilterSize;			

			float shadowMapValue = texture(sampler2D(ShadowTexture, clampMiplessLinearSampler), posLS.xy + offset, 0).r;
			shadowFactor += (shadowMapValue - 0.002f > posLS.z ? 0.0f : 1.0f);
		}
		shadowFactor *= NUM_SHADOW_SAMPLES_INV;
		return shadowFactor;
}

void main()
{	
	//load albedo
	vec4 albedoInfo = texture(sampler2D(albedoMap, samplerAlbedo), InUV);
	vec3 albedo = albedoInfo.rgb;
	float alpha = albedoInfo.a;
	vec3 metallicRoughness = texture(sampler2D(metallicRoughnessMap, samplerMR), InUV).rgb;
	float ao = texture(sampler2D(aoMap, samplerAO), InUV).r;
	vec3 emissive = texture(sampler2D(emissiveMap, samplerEmissive), InUV).rgb;
	vec3 normal = getNormalFromMap();

	vec3 metalness = vec3(metallicRoughness.b);
	float roughness = metallicRoughness.g;
	
	// Apply alpha mask
	uint alphaMode = uint(InAlphaSettings.x);
	
	if (alphaMode == 1)
	{
		float alphaCutoff = InAlphaSettings.y;
		if(alpha < alphaCutoff)
			discard;
		else
			alpha = 1.0f;
	}
	else
	{
		alpha = mix(InBaseColor.a, alpha * InBaseColor.a, float(hasAlbedoMap));
	}
		
	albedo = albedo * InBaseColor.rgb;
	metalness = metalness * InMetallicRoughness.x;
	roughness = roughness * InMetallicRoughness.y;

	albedo = mix(InBaseColor.rgb, albedo, float(hasAlbedoMap));
	normal = mix(worldNormal, normal, float(hasNormalMap));
	metalness = mix(vec3(InMetallicRoughness.x), metalness, float(hasMetallicRoughnessMap));
	roughness = mix(InMetallicRoughness.y, roughness, float(hasMetallicRoughnessMap));
	emissive = mix(vec3(0.0f,0.0f,0.0f), emissive, float(hasEmissiveMap));
	ao = mix(1.0f - metallicRoughness.r, ao, float(hasAOMap));

	roughness = clamp(0.05f, 1.0f, roughness);

	// Compute Direction light
	vec3 N = normal;
	vec3 V = normalize(camPos.xyz - worldPosition);
	float NoV = max(dot(N,V), 0.0);

	vec3 result = vec3(0.0, 0.0, 0.0);		

	for(uint i=0; i<3; ++i)
	{
		vec3 L = normalize(lightDirection[i].xyz);
		vec3 H = normalize(V + L);	
		float NoL = max(dot(N,L), 0.0);

		result += ComputeLight(albedo, lightColor[i].rgb, metalness, roughness, N, L, V, H, NoL, NoV, alphaMode) * lightColor[i].a ;
	}

	result *= CalcPCFShadowFactor(worldPosition);
	// AO
	result *= ao;

	// Ambeint Light
	result += albedo * lightColor[3].rgb * lightColor[3].a;
	result += emissive;

	// Tonemap and gamma correct
	//color = color/(color+vec3(1.0));
	//result = pow(result, vec3(1.0/2.2));

	//color =vec3(InUV, 0.0f);
	//metallicRoughness.r = 0;
	//color = vec3(InUV, 0.0f);//metallicRoughness;
	//color = -normal;
	OutColor = vec4(result, alpha);
}
