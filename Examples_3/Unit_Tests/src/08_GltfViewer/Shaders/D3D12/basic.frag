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

#define NUM_SHADOW_SAMPLES 32

#if NUM_SHADOW_SAMPLES == 16
static const float NUM_SHADOW_SAMPLES_INV = 0.0625;
static const float shadowSamples[NUM_SHADOW_SAMPLES * 2] =
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
#else
static const float NUM_SHADOW_SAMPLES_INV = 0.03125;
static const float shadowSamples[NUM_SHADOW_SAMPLES * 2] =
{
	-0.1746646, -0.7913184,
	-0.129792, -0.4477116,
	0.08863912, -0.898169,
	-0.5891499, -0.6781639,
	0.1748409, -0.5252063,
	0.6483325, -0.752117,
	0.4529319, -0.384986,
	0.09757467, -0.1166954,
	0.3857658, -0.9096935,
	0.5613058, -0.1283066,
	0.768011, -0.4906538,
	0.8499438, -0.220937,
	0.6946555, 0.1605866,
	0.9614297, 0.05975229,
	0.7986544, 0.5325912,
	0.4513965, 0.5592551,
	0.2847693, 0.2293397,
	-0.2118996, -0.1609127,
	-0.4357893, -0.3808875,
	-0.4662672, -0.05288446,
	-0.139129, 0.2394065,
	0.1781853, 0.5254948,
	0.4287854, 0.899425,
	0.1289349, 0.8724155,
	-0.6924323, -0.2203967,
	-0.48997, 0.2795907,
	-0.2611724, 0.7359962,
	-0.7704172, 0.4233134,
	-0.850104, 0.1263935,
	-0.8345267, -0.4991361,
	-0.5380967, 0.6264234,
	-0.9769312, -0.1550569
};
#endif

cbuffer cbPerPass : register(b0, UPDATE_FREQ_PER_FRAME)
{
	float4x4	projView;
	float4x4	shadowLightViewProj;
	float4      camPos;
	float4      lightColor[4];
	float4      lightDirection[3];
}

struct GLTFTextureProperties
{
    uint mTextureSamplerIndex;
	int mUVStreamIndex;
	float mRotation;
	float mValueScale;
	float2 mOffset;
	float2 mScale;
};

struct GLTFMaterialData
{
	uint mAlphaMode;
	float mAlphaCutoff;
	float2 mEmissiveGBScale;
	
    float4 mBaseColorFactor;
    float4 mMetallicRoughnessFactors; // RG, or specular RGB + A glossiness
	
	GLTFTextureProperties mBaseColorProperties;
	GLTFTextureProperties mMetallicRoughnessProperties;
	
	GLTFTextureProperties mNormalTextureProperties;
	GLTFTextureProperties mOcclusionTextureProperties;
	GLTFTextureProperties mEmissiveTextureProperties;
};

cbuffer cbMaterialData : register(b0, UPDATE_FREQ_PER_DRAW) {
	GLTFMaterialData materialData;
};

Texture2D baseColorMap			: register(t0, UPDATE_FREQ_PER_DRAW);
Texture2D normalMap				: register(t1, UPDATE_FREQ_PER_DRAW);
Texture2D metallicRoughnessMap	: register(t2, UPDATE_FREQ_PER_DRAW);
Texture2D occlusionMap			: register(t3, UPDATE_FREQ_PER_DRAW);
Texture2D emissiveMap			: register(t4, UPDATE_FREQ_PER_DRAW);
Texture2D ShadowTexture		    : register(t14, UPDATE_FREQ_NONE);

SamplerState baseColorSampler			: register(s0, UPDATE_FREQ_PER_DRAW);
SamplerState normalMapSampler			: register(s1, UPDATE_FREQ_PER_DRAW);
SamplerState metallicRoughnessSampler	: register(s2, UPDATE_FREQ_PER_DRAW);
SamplerState occlusionMapSampler		: register(s3, UPDATE_FREQ_PER_DRAW);
SamplerState emissiveMapSampler			: register(s4, UPDATE_FREQ_PER_DRAW);
SamplerState clampMiplessLinearSampler : register(s7);

struct PsIn
{    
    float3 pos               : POSITION;
	float3 normal	         : NORMAL;
	float2 texCoord          : TEXCOORD0;
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
	float Fc = pow(1.0f - cosTheta, 5.0f);
	return F0 + (1.0f - F0) * Fc;
}

float4 sampleTexture(GLTFTextureProperties textureProperties, 
					Texture2D tex, SamplerState s, 
					float4 scaleFactor,
					float2 uv)
{
	uint textureIndex = textureProperties.mTextureSamplerIndex & 0xFFFF;
	uint samplerIndex = textureProperties.mTextureSamplerIndex >> 16;
	if (textureIndex == 0xFFFF)
	{
		return scaleFactor;
	}

	float2 texCoord = uv * textureProperties.mScale + textureProperties.mOffset;
	if (textureProperties.mRotation)
	{
		float s, c;
		sincos(textureProperties.mRotation, s, c);
		texCoord = float2(c * texCoord.x - s * texCoord.y, s * texCoord.x + c * texCoord.y);
	}

	return tex.Sample(s, texCoord) * textureProperties.mValueScale * scaleFactor;
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

// Appoximation of joint Smith term for GGX
// [Heitz 2014, "Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs"]
float Vis_SmithJointApprox(float a, float NoV, float NoL)
{	
	float Vis_SmithV = NoL * (NoV * (1.0f - a) + a);
	float Vis_SmithL = NoV * (NoL * (1.0f - a) + a);
	return 0.5 * rcp(max(Vis_SmithV + Vis_SmithL, 0.001));
}

float3 reconstructNormal(in float4 sampleNormal)
{
	float3 tangentNormal;
	tangentNormal.xy = sampleNormal.rg * 2 - 1;
	tangentNormal.z = sqrt(1.0 - saturate(dot(tangentNormal.xy, tangentNormal.xy)));
	return normalize(tangentNormal);
}

float3 getNormalFromMap(float3 normal, float3 pos, float2 uv)
{
	uint textureIndex = materialData.mNormalTextureProperties.mTextureSamplerIndex & 0xFFFF;
	if (textureIndex == 0xFFFF) 
	{
		return normalize(normal);
	}

	float3 tangentNormal = reconstructNormal(normalMap.Sample(normalMapSampler, uv));

	float3 Q1 = ddx(pos);
	float3 Q2 = ddy(pos);
	float2 st1 = ddx(uv);
	float2 st2 = ddy(uv);

	float3 N = normalize(normal);
	float3 T = Q1*st2.g - Q2 * st1.g;
	T = normalize(T);

	if(isnan(T.x) ||isnan(T.y) || isnan(T.z))
	{
		float3 UpVec = abs(N.y) < 0.999 ? float3(0.0, 1.0, 0.0) : float3(0.0, 0.0, 1.0);
		T = normalize(cross(N, UpVec));
	}

	float3 B = normalize(cross(T, N));
	float3x3 TBN = float3x3(T, B, N);

	float3 res = mul(tangentNormal, TBN);
	return normalize(res);
}

float3 ComputeLight(float3 albedo, float3 lightColor,
float3 metalness, float roughness,
float3 N, float3 L, float3 V, float3 H, float NoL, float NoV)
{
	float a  = roughness * roughness;
	// 0.04 is the index of refraction for metal
	float3 F0 = float3(0.04f, 0.04f, 0.04f);
	float3 diffuse = (1.0 - metalness) * albedo;
	float NDF = distributionGGX(N, H, roughness);
	float G = Vis_SmithJointApprox(a, NoV, NoL);
	float3 F = fresnelSchlick(max(dot(N, H), 0.0f), lerp(F0, albedo, metalness));	
	float3 specular = NDF * G * F;
	
	// To intensify Fresnel
	float3 F2 = fresnelSchlick(max(dot(N, V), 0.0f), F0);
	specular += F2;

	float3 irradiance = float3(lightColor.r,lightColor.g,lightColor.b) * float3(1.0, 1.0, 1.0);
	float3 result = (diffuse + specular) * NoL * irradiance;

	return result;
}

float CalcESMShadowFactor(float3 worldPos)
{
	float4 posLS = mul(shadowLightViewProj, float4(worldPos.xyz, 1.0));
	posLS /= posLS.w;
	posLS.y *= -1;
	posLS.xy = posLS.xy * 0.5 + float2(0.5, 0.5);


	float2 HalfGaps = float2(0.00048828125, 0.00048828125);
	float2 Gaps = float2(0.0009765625, 0.0009765625);

	posLS.xy += HalfGaps;

	float shadowFactor = 1.0;

	float4 shadowDepthSample = float4(0, 0, 0, 0);
	shadowDepthSample.x = ShadowTexture.SampleLevel(clampMiplessLinearSampler, posLS.xy, 0).r;
	shadowDepthSample.y = ShadowTexture.SampleLevel(clampMiplessLinearSampler, posLS.xy, 0, int2(1, 0)).r;
	shadowDepthSample.z = ShadowTexture.SampleLevel(clampMiplessLinearSampler, posLS.xy, 0, int2(0, 1)).r;
	shadowDepthSample.w = ShadowTexture.SampleLevel(clampMiplessLinearSampler, posLS.xy, 0, int2(1, 1)).r;
	float avgShadowDepthSample = (shadowDepthSample.x + shadowDepthSample.y + shadowDepthSample.z + shadowDepthSample.w) * 0.25f;
	shadowFactor = saturate(2.0 - exp((posLS.z - avgShadowDepthSample) * 100.0f ));
	return shadowFactor;
}

float random(float3 seed, float3 freq)
{
	// project seed on random constant vector
	float dt = dot(floor(seed * freq), float3(53.1215, 21.1352, 9.1322));
	// return only the fractional part
	return frac(sin(dt) * 2105.2354);
}

float CalcPCFShadowFactor(float3 worldPos)
{
	float4 posLS = mul(shadowLightViewProj, float4(worldPos.xyz, 1.0));
	posLS /= posLS.w;
	posLS.y *= -1;
	posLS.xy = posLS.xy * 0.5 + float2(0.5, 0.5);


	float2 HalfGaps = float2(0.00048828125, 0.00048828125);
	float2 Gaps = float2(0.0009765625, 0.0009765625);

	posLS.xy += HalfGaps;

	float shadowFactor = 1.0;

	float shadowFilterSize = 0.0016;
	float angle = random(worldPos, 20.0);
	float s = sin(angle);
	float c = cos(angle);

	for (int i = 0; i < NUM_SHADOW_SAMPLES; i++)
	{
		float2 offset = float2(shadowSamples[i * 2], shadowSamples[i * 2 + 1]);
		offset = float2(offset.x * c + offset.y * s, offset.x * -s + offset.y * c);
		offset *= shadowFilterSize;
		float shadowMapValue = ShadowTexture.SampleLevel(clampMiplessLinearSampler, posLS.xy + offset, 0);
		shadowFactor += (shadowMapValue - 0.002f > posLS.z ? 0.0f : 1.0f);
	}
	shadowFactor *= NUM_SHADOW_SAMPLES_INV;
	return shadowFactor;
}

float CalculateShadow(float3 worldPos)
{
	float4 NDC = mul(shadowLightViewProj, float4(worldPos, 1.0));
	NDC /= NDC.w;
	float Depth = NDC.z;
	float2 ShadowCoord = float2((NDC.x + 1.0)*0.5, (1.0 - NDC.y)*0.5);
	float ShadowDepth = ShadowTexture.Sample(clampMiplessLinearSampler, ShadowCoord).r;
	

	if(ShadowDepth - 0.002f > Depth)
		return 0.0f;
	else
		return 1.0f;
}


PSOut main(PsIn input) : SV_TARGET
{	
	PSOut Out = (PSOut) 0;

	// TODO: multiply factors (e.g. mBaseColorFactor) by vertex colours.

	float4 baseColor = sampleTexture(materialData.mBaseColorProperties, 
							baseColorMap, baseColorSampler, 
							materialData.mBaseColorFactor,
							input.texCoord);

	float4 metallicRoughness = sampleTexture(materialData.mMetallicRoughnessProperties, 
									metallicRoughnessMap, metallicRoughnessSampler, 
									materialData.mMetallicRoughnessFactors,
									input.texCoord);

	float ao = sampleTexture(materialData.mOcclusionTextureProperties, 
					occlusionMap, occlusionMapSampler, 
					1.0,
					input.texCoord).x;

	float3 emissive = sampleTexture(materialData.mEmissiveTextureProperties, 
							emissiveMap, emissiveMapSampler, 
							1.0,
							input.texCoord).rgb;
	emissive *= materialData.mEmissiveTextureProperties.mValueScale;
	emissive.gb *= materialData.mEmissiveGBScale;

	float3 normal = getNormalFromMap(input.normal, input.pos, input.texCoord);

	float3 metalness = float3(metallicRoughness.b, metallicRoughness.b, metallicRoughness.b);
	float roughness = metallicRoughness.g;
	
	if (materialData.mAlphaMode == 1 && materialData.mAlphaCutoff < 1.f && baseColor.a < materialData.mAlphaCutoff) { discard; }

	roughness = clamp(0.02f, 1.0f, roughness);

	// Compute Direction light
	float3 N = normal;	
	float3 V = normalize(camPos.xyz - input.pos);
	float NoV = max(dot(N,V), 0.0);	

	float3 result = float3(0.0, 0.0, 0.0);		

	[unroll]
	for(uint i=0; i<1; ++i)
	{
		float3 L = normalize(lightDirection[i].xyz);
		float3 H = normalize(V + L);	
		float NoL = max(dot(N,L), 0.0);

		result += ComputeLight(baseColor.rgb, lightColor[i].rgb, metalness, roughness, N, L, V, H, NoL, NoV) * lightColor[i].a;
	}

	// AO
	result *= ao;

	result *= CalcPCFShadowFactor(input.pos);

	// Ambeint Light
	result += baseColor.rgb * lightColor[3].rgb * lightColor[3].a;
	result += emissive;
	
	// Tonemap and gamma correct
	// result = result/(result+float3(1.0, 1.0, 1.0));

	Out.outColor = float4(result.r, result.g, result.b, baseColor.a);
	return Out;
}