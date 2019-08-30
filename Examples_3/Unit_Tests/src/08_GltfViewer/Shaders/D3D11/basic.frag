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

#define PI 3.141592654f

cbuffer cbPerPass : register(b0) 
{
	float4x4	projView;
	float4      camPos;
	float4      lightColor;
}

cbuffer cbPerProp : register(b1)
{
	float4x4	world;
	int         unlit;
	int         hasAlbedoMap;
	int         hasNormalMap;
	int         hasMetallicRoughnessMap;
	int         hasAOMap;
	int         hasEmissiveMap;
	float4		posOffset;
	float2		uvOffset;
	float2		uvScale;
	float2		padding00;
}

Texture2D albedoMap            : register(t0);
Texture2D normalMap            : register(t1);
Texture2D metallicRoughnessMap : register(t2);
Texture2D aoMap                : register(t3);
Texture2D emissiveMap          : register(t4);

SamplerState samplerAlbedo   : register(s0);
SamplerState samplerNormal   : register(s1);
SamplerState samplerMR       : register(s2);
SamplerState samplerAO       : register(s3);
SamplerState samplerEmissive : register(s4);

struct PsIn
{    
    float3 pos               : POSITION;
	float3 normal	         : NORMAL;
	float2 texCoord          : TEXCOORD0;
	float3 baseColor         : COLOR;
	float2 metallicRoughness : TEXCOORD1;
	float2 alphaSettings     : TEXCOORD2;
};

struct PSOut
{
    float4 outColor : SV_Target0;
    //float4 normal : SV_Target1;
};

float3 FresnelSchlickRoughness(float cosTheta, float3 F0, float roughness)
{
	return F0 + (max(float3(1.0 - roughness, 1.0 - roughness, 1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
}

float3 fresnelSchlick(float cosTheta, float3 F0)
{
	return F0 + (1.0f - F0) * pow(1.0 - cosTheta, 5.0);
}

float distributionGGX(float3 N, float3 H, float roughness)
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

float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
	float NdotV = max(dot(N, V), 0.0);
	float NdotL = max(dot(N, L), 0.0);
	float ggx2 = GeometrySchlickGGX(NdotV, roughness);
	float ggx1 = GeometrySchlickGGX(NdotL, roughness);

	return ggx1 * ggx2;
}

float3 reconstructNormal(in float4 sampleNormal)
{
	float3 tangentNormal;
	tangentNormal.xy = sampleNormal.rg * 2 - 1;
	tangentNormal.z = sqrt(1 - saturate(dot(tangentNormal.xy, tangentNormal.xy)));
	return tangentNormal;
}

float3 getNormalFromMap(float3 normal, float3 pos, float2 uv)
{
	float3 tangentNormal = reconstructNormal(normalMap.Sample(samplerNormal, uv));

    float3 Q1  = ddx(pos);
    float3 Q2  = ddy(pos);
    float2 st1 = ddx(uv);
    float2 st2 = ddy(uv);

    float3 N   = normalize(normal);
    float3 T  = normalize(Q1*st2.y - Q2*st1.y);
    float3 B  = -normalize(cross(N, T));
    float3x3 TBN = float3x3(T, B, N);

    float3 res = mul(tangentNormal,TBN);
    return res;
}

PSOut main(PsIn input) : SV_TARGET
{	
	PSOut Out = (PSOut) 0;

	//load albedo
	float3 albedo = albedoMap.Sample(samplerAlbedo, input.texCoord).rgb;
	float alpha = albedoMap.Sample(samplerAlbedo, input.texCoord).a;
	float3 metallicRoughness = metallicRoughnessMap.Sample(samplerMR, input.texCoord).rgb;
	float ao = aoMap.Sample(samplerAO, input.texCoord).r;
	float3 emissive = emissiveMap.Sample(samplerEmissive, input.texCoord).rgb;

	float3 normal = getNormalFromMap(input.normal, input.pos, input.texCoord);

	float3 metalness = float3(metallicRoughness.b, metallicRoughness.b, metallicRoughness.b);
	float roughness = input.metallicRoughness.g;
	
	// Apply alpha mask
	uint alphaMode = uint(input.alphaSettings.x);
	float alphaCutoff = input.alphaSettings.y;
	if (alphaMode == 0)
		alpha = 1.0f;
	if (alphaMode == 1 && alpha < alphaCutoff)
		discard;

	albedo = albedo * input.baseColor.rgb;
	metalness = metalness * input.metallicRoughness.x;
	roughness = roughness * input.metallicRoughness.y;

	if (hasAlbedoMap == 0)
	{
		albedo = input.baseColor.rgb;
	}
	if (hasNormalMap == 0)
	{
		normal = input.normal;
	}
	if (hasMetallicRoughnessMap == 0)
	{
		metalness = float3(input.metallicRoughness.x, input.metallicRoughness.x, input.metallicRoughness.x);
		roughness = input.metallicRoughness.y;
	}

	// Compute Direction light
	float3 N = normal;
	float3 L = normalize(float3(.5,1.0,.5));
	float3 V = normalize(camPos.xyz - input.pos);
	float3 H = normalize(V + L);
	
	float NL = max(dot(N,L),0.0);
	float NV = max(dot(N,V), 0.0);

	// 0.04 is the index of refraction for metal
	float3 F0 = float3(0.04f, 0.04f, 0.04f);
	F0 = lerp(F0, albedo, metalness);

	float3 radiance = float3(lightColor.r,lightColor.g,lightColor.b) * 3.0f;

	float NDF = distributionGGX(N, H, roughness);
	float G = GeometrySmith(N, V, L, roughness);
	float3 F = fresnelSchlick(max(dot(N,H), 0.0), F0);
		
	float3 nominator = NDF * G * F;
	float denominator = 4.0f * NV * NL + 0.001;
	float3 specular = nominator / denominator;

	float3 kS = F;
	float3 kD = float3(1.0f,1.0f,1.0f) - kS;
	kD *= float3(1.0f,1.0f,1.0f) - metalness;

	float3 Lo = (kD * albedo / PI + specular) * radiance * NL;

	// Compute ambient / environment light
	F = FresnelSchlickRoughness(NV, F0, roughness);
	kS = F;
	kD = float3(1.0f,1.0f,1.0f) - kS;
	kD *= float3(1.0f,1.0f,1.0f) - metalness;

	float3 irradiance = float3(1.0, 1.0, 1.0);
	float3 diffuse = kD * irradiance * albedo;
	//float3 specular = specularColor * (F * brdf.x + brdf.y);
	float3 ambient = diffuse * ao;

	float3 color = emissive + Lo + ambient * 0.3;

	// Do not Light alpha blended materials
	if (alphaMode != 0 || unlit != 0)
		color = albedo;
	
	// Tonemap and gamma correct
	color = color/(color+float3(1.0, 1.0, 1.0));
	color = pow(color, float3(1.0/2.2, 1.0/2.2, 1.0/2.2));

	//color = albedo;
	//metallicRoughness.r = 0;
	//color = normal;
	Out.outColor = float4(color.r, color.g, color.b, alpha);

	return Out;
}