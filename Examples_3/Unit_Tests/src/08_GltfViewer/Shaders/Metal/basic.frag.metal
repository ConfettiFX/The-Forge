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

#include <metal_stdlib>
#include <metal_atomic>
using namespace metal;

inline float3x3 matrix_ctor(float4x4 m) {
    return float3x3(m[0].xyz, m[1].xyz, m[2].xyz);
}

static constant float PI = 3.14159265359;

#define NUM_SHADOW_SAMPLES 32

#if NUM_SHADOW_SAMPLES == 16
static constant float NUM_SHADOW_SAMPLES_INV = 0.0625;
static constant float shadowSamples[NUM_SHADOW_SAMPLES * 2] =
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
static constant float NUM_SHADOW_SAMPLES_INV = 0.03125;
static constant float shadowSamples[NUM_SHADOW_SAMPLES * 2] =
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

float3 FresnelSchlickRoughness(float cosTheta, float3 F0, float roughness)
{
    return F0 + (max(float3(1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
}

float3 fresnelSchlick(float cosTheta, float3 F0)
{
	float Fc = pow(1.0f - cosTheta, 5.0f);
	return F0 + (1.0f - F0) * Fc;
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

// Appoximation of joint Smith term for GGX
// [Heitz 2014, "Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs"]
float Vis_SmithJointApprox(float a, float NoV, float NoL)
{	
	float Vis_SmithV = NoL * (NoV * (1.0f - a) + a);
	float Vis_SmithL = NoV * (NoL * (1.0f - a) + a);
	return 0.5 * (1.0f / max(Vis_SmithV + Vis_SmithL, 0.001));
}

float3 reconstructNormal(float4 sampleNormal)
{
	float3 tangentNormal;

#ifdef TARGET_IOS
	// ASTC texture stores normal map components in red and alpha channels
	// Once MTLTextureSwizzle becomes part of final API, we can do this in the renderer side so the shader can treat ASTC normal_psnr like a BC5 texture
	tangentNormal.xy = (sampleNormal.ra * 2 - 1);
#else
	tangentNormal.xy = (sampleNormal.rg * 2 - 1);
#endif
	tangentNormal.z = sqrt(1 - saturate(dot(tangentNormal.xy, tangentNormal.xy)));
	return tangentNormal;
}

float3 getNormalFromMap( texture2d<float> normalMap, sampler defaultSampler, float2 uv, float3 pos, float3 normal)
{
    float3 tangentNormal = reconstructNormal(normalMap.sample(defaultSampler,uv));

    float3 Q1 = dfdx(pos);
    float3 Q2 = dfdy(pos);
    float2 st1 = dfdx(uv);
    float2 st2 = dfdy(uv);


    float3 N = normalize(normal);
    float3 T = normalize(Q1*st2.g - Q2*st1.g);
	T = normalize(T);

	if(isnan(T.x) ||isnan(T.y) || isnan(T.z))
	{
		float3 UpVec = abs(N.y) < 0.999 ? float3(0.0, 1.0, 0.0) : float3(0.0, 0.0, 1.0);
		T = normalize(cross(N, UpVec));
	}

    float3 B = normalize(cross(T, N));
    float3x3 TBN = float3x3(T, B, N);

    float3 res = TBN * tangentNormal;
    return normalize(res);
};

float3 ComputeLight(float3 albedo, float3 lightColor,
float3 metalness, float roughness,
float3 N, float3 L, float3 V, float3 H, float NoL, float NoV,
uint alphaMode, uint unlit)
{
    float a  = roughness * roughness;
    // 0.04 is the index of refraction for metal
    float3 F0 = float3(0.04f, 0.04f, 0.04f);
    float3 diffuse = (1.0 - metalness) * albedo;
    float NDF = distributionGGX(N, H, roughness);
    float G = Vis_SmithJointApprox(a, NoV, NoL);
    float3 F = fresnelSchlick(max(dot(N, H), 0.0f), mix(F0, albedo, metalness));
    float3 specular = NDF * G * F;
    
    // To intensify Fresnel
    float3 F2 = fresnelSchlick(max(dot(N, V), 0.0f), F0);
    specular += F2;

    float3 irradiance = float3(lightColor.r,lightColor.g,lightColor.b) * float3(1.0, 1.0, 1.0);
    float3 result = (diffuse + specular) * NoL * irradiance;
    // Do not Light alpha blended materials
    if (alphaMode != 0 || unlit != 0)
        result = albedo;

    return result;
}

float CalcESMShadowFactor(float4x4 LightViewProj, float3 worldPos, texture2d<float, access::sample> shadowTexture, sampler shadowSampler)
{
    float4 posLS = LightViewProj * float4(worldPos.xyz, 1.0);
    posLS /= posLS.w;
    posLS.y *= -1;
    posLS.xy = posLS.xy * 0.5 + float2(0.5, 0.5);


    float2 HalfGaps = float2(0.00048828125, 0.00048828125);
    //float2 Gaps = float2(0.0009765625, 0.0009765625);

    posLS.xy += HalfGaps;

    float shadowFactor = 1.0;

    float4 shadowDepthSample = float4(0, 0, 0, 0);
    shadowDepthSample.x = shadowTexture.sample(shadowSampler, posLS.xy, level(0)).r;
    shadowDepthSample.y = shadowTexture.sample(shadowSampler, posLS.xy, level(0), int2(1, 0)).r;
    shadowDepthSample.z = shadowTexture.sample(shadowSampler, posLS.xy, level(0), int2(0, 1)).r;
    shadowDepthSample.w = shadowTexture.sample(shadowSampler, posLS.xy, level(0), int2(1, 1)).r;
    float avgShadowDepthSample = (shadowDepthSample.x + shadowDepthSample.y + shadowDepthSample.z + shadowDepthSample.w) * 0.25f;
    shadowFactor = saturate(2.0 - exp((posLS.z - avgShadowDepthSample) * 100.0f ));
    return shadowFactor;
}

float random(float3 seed, float3 freq)
{
    // project seed on random constant vector
    float dt = dot(floor(seed * freq), float3(53.1215, 21.1352, 9.1322));
    // return only the fractional part
    return fract(sin(dt) * 2105.2354);
}

float CalcPCFShadowFactor(float4x4 LightViewProj, float3 worldPos, depth2d<float, access::sample> shadowTexture, sampler shadowSampler)
{
    float4 posLS = LightViewProj * float4(worldPos.xyz, 1.0);
    posLS /= posLS.w;
    posLS.y *= -1;
    posLS.xy = posLS.xy * 0.5 + float2(0.5, 0.5);


    float2 HalfGaps = float2(0.00048828125, 0.00048828125);
    //float2 Gaps = float2(0.0009765625, 0.0009765625);

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
        float shadowMapValue = shadowTexture.sample(shadowSampler, posLS.xy + offset, level(0));
        shadowFactor += (shadowMapValue - 0.002f > posLS.z ? 0.0f : 1.0f);
    }
    shadowFactor *= NUM_SHADOW_SAMPLES_INV;
    return shadowFactor;
}

float ClaculateShadow(float4x4 LightViewProj, float3 worldPos, texture2d<float, access::sample> shadowTexture, sampler shadowSampler)
{
    float4 NDC = LightViewProj * float4(worldPos, 1.0);
    NDC /= NDC.w;
    float Depth = NDC.z;
    float2 ShadowCoord = float2((NDC.x + 1.0)*0.5, (1.0 - NDC.y)*0.5);
    float ShadowDepth = shadowTexture.sample(shadowSampler, ShadowCoord).r;
    

    if(ShadowDepth - 0.002f > Depth)
        return 0.0f;
    else
        return 1.0f;
}

struct Uniforms_cbPerPass {
    float4x4 projView;
    float4   camPos;
    float4   lightColor[4];
	float4   lightDirection[3];
    int4     quantizationParams;
};

struct Uniforms_cbPerProp {
    float4x4 world;
    float4x4 InvTranspose;
    int unlit;
    int hasAlbedoMap;
    int hasNormalMap;
    int hasMetallicRoughnessMap;
    int hasAOMap;
    int hasEmissiveMap;
    float4 centerOffset;
    float4 posOffset;
    float2 uvOffset;
    float2 uvScale;
    float posScale;   
    float padding0;   
};

struct Uniforms_ShadowUniformBuffer
{
    float4x4 LightViewProj;
};

struct PsIn
{
    float4 position [[position]];
    float4 baseColor;
    float3 normal;
    float3 pos;
    float2 uv;
    float2 metallicRoughness;
    float2 alphaSettings;
};

struct PSOut
{
    float4 outColor [[color(0)]];
};

struct Scene
{
	depth2d<float, access::sample> ShadowTexture                    [[id(0)]];
	sampler  clampMiplessLinearSampler                                [[id(1)]];
};

struct PerFrame
{
	constant Uniforms_cbPerPass &           cbPerPass                 [[id(0)]];
	constant Uniforms_ShadowUniformBuffer & ShadowUniformBuffer       [[id(1)]];
};

struct PerDraw
{
	constant Uniforms_cbPerProp & cbPerProp [[id(0)]];
	texture2d<float, access::sample> albedoMap                        [[id(1)]];
	texture2d<float, access::sample> normalMap                        [[id(2)]];
	texture2d<float, access::sample> metallicRoughnessMap             [[id(3)]];
	texture2d<float, access::sample> aoMap                            [[id(4)]];
	texture2d<float, access::sample> emissiveMap                      [[id(5)]];
	
	sampler  samplerAlbedo                                            [[id(6 + 0)]];
	sampler  samplerNormal                                            [[id(6 + 1)]];
	sampler  samplerMR                                                [[id(6 + 2)]];
	sampler  samplerAO                                                [[id(6 + 3)]];
	sampler  samplerEmissive                                          [[id(6 + 4)]];
};

fragment PSOut stageMain(PsIn     input                                                    [[stage_in]],
						 constant Scene& argBufferStatic                                   [[buffer(UPDATE_FREQ_NONE)]],
						 constant PerFrame& argBufferPerFrame                              [[buffer(UPDATE_FREQ_PER_FRAME)]],
						 constant PerDraw& argBufferPerDraw                                 [[buffer(UPDATE_FREQ_PER_DRAW)]])
{
    PSOut Out;
	float4 albedoInfo = argBufferPerDraw.albedoMap.sample(argBufferPerDraw.samplerAlbedo, input.uv);
   
    float3 albedo = albedoInfo.rgb;
    float alpha = albedoInfo.a;
    float3 metallicRoughness = ( argBufferPerDraw.metallicRoughnessMap.sample(argBufferPerDraw.samplerMR, input.uv)).rgb;
    float ao = ( argBufferPerDraw.aoMap.sample(argBufferPerDraw.samplerAO, input.uv)).r;
    float3 emissive = ( argBufferPerDraw.emissiveMap.sample(argBufferPerDraw.samplerEmissive, input.uv)).rgb;
    float3 normal = getNormalFromMap(argBufferPerDraw.normalMap, argBufferPerDraw.samplerNormal, input.uv, input.pos, input.normal);
    
    float3 metalness = float3(metallicRoughness.b);
    float roughness = metallicRoughness.g;
    
    // Apply alpha mask
    uint alphaMode = uint(input.alphaSettings.x);	
	if (alphaMode == 1)
	{
		float alphaCutoff = input.alphaSettings.y;
		if(alpha < alphaCutoff)
			discard_fragment();
		else
			alpha = 1.0f;
	}
	else
	{
		alpha = mix(input.baseColor.a, alpha * input.baseColor.a, (float)argBufferPerDraw.cbPerProp.hasAlbedoMap);
	}

	albedo = albedo * input.baseColor.rgb;
	metalness = metalness * input.metallicRoughness.x;
	roughness = roughness * input.metallicRoughness.y;

	albedo = mix(input.baseColor.rgb, albedo, (float)argBufferPerDraw.cbPerProp.hasAlbedoMap);
	normal = mix(input.normal, normal, (float)argBufferPerDraw.cbPerProp.hasNormalMap);
	metalness = mix(float3(input.metallicRoughness.x, input.metallicRoughness.x, input.metallicRoughness.x), metalness, (float)argBufferPerDraw.cbPerProp.hasMetallicRoughnessMap);
	roughness = mix(input.metallicRoughness.y, roughness, (float)argBufferPerDraw.cbPerProp.hasMetallicRoughnessMap);
	emissive = mix(float3(0.0f,0.0f,0.0f), emissive, (float)argBufferPerDraw.cbPerProp.hasEmissiveMap);
	ao = mix(1.0 - metallicRoughness.r, ao, (float)argBufferPerDraw.cbPerProp.hasAOMap);

	roughness = clamp(0.05f, 1.0f, roughness);
    
    // Compute Direction light
    float3 N = normal;   
    float3 V = normalize(argBufferPerFrame.cbPerPass.camPos.xyz - input.pos);
	float NoV = max(dot(N,V), 0.0);

	float3 result = float3(0.0, 0.0, 0.0);
   
    for(uint i=0; i<3; ++i)
	{
		float3 L = normalize(float3(argBufferPerFrame.cbPerPass.lightDirection[i].x, argBufferPerFrame.cbPerPass.lightDirection[i].y, argBufferPerFrame.cbPerPass.lightDirection[i].z));
		float3 H = normalize(V + L);
		float NoL = max(dot(N,L),0.0);

		result += ComputeLight(albedo, argBufferPerFrame.cbPerPass.lightColor[i].rgb, metalness, roughness, N, L, V, H, NoL, NoV, alphaMode, argBufferPerDraw.cbPerProp.unlit) * argBufferPerFrame.cbPerPass.lightColor[i].a;
	}

	// AO
	result *= ao;

    result *= CalcPCFShadowFactor(argBufferPerFrame.ShadowUniformBuffer.LightViewProj, input.pos, argBufferStatic.ShadowTexture, argBufferStatic.clampMiplessLinearSampler);

	// Ambeint Light
	result += albedo * argBufferPerFrame.cbPerPass.lightColor[3].rgb * argBufferPerFrame.cbPerPass.lightColor[3].a;
	result += emissive;
    
    // Tonemap and gamma correct
    //color = color/(color+float3(1.0));
    //color = pow(color, float3(1.0/2.2));
    
    Out.outColor = float4(result,alpha);
    
    return Out;
}
