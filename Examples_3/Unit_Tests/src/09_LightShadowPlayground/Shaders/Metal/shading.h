/*
 * Copyright (c) 2018-2019 Confetti Interactive Inc.
 *
 * This file is part of TheForge
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

//
//  shading.mh.metal
//  Visibility_Buffer
//

#ifndef SHADING_METAL
#define SHADING_METAL

#include <metal_stdlib>
using namespace metal;


constant float PI = 3.1415926535897932384626422832795028841971f;

float2 LightingFunGGX_FV(float dotLH, float roughness)
{
	float alpha = roughness * roughness;
	
	//F
	float F_a, F_b;
	float dotLH5 = pow(saturate(1.0f - dotLH), 5.0f);
	F_a = 1.0f;
	F_b = dotLH5;
	
	//V
	float vis;
	float k = alpha * 0.5f;
	float k2 = k * k;
	float invK2 = 1.0f - k2;
	vis = 1.0f / (dotLH*dotLH*invK2 + k2);
	
	return float2((F_a - F_b)*vis, F_b*vis);
}

float LightingFuncGGX_D(float dotNH, float roughness)
{
	float alpha = roughness * roughness;
	float alphaSqr = alpha * alpha;
	float denom = max(dotNH * dotNH * (alphaSqr - 1.0f) + 1.0f, 0.001f);
	
	return alphaSqr / (PI*denom*denom);
}

float3 GGX_Spec(float3 Normal, float3 HalfVec, float Roughness, float3 SpecularColor, float2 paraFV)
{
	float NoH = saturate(dot(Normal, HalfVec));
	float NoH2 = NoH * NoH;
	float NoH4 = NoH2 * NoH2;
	float D = LightingFuncGGX_D(NoH4, Roughness);
	float2 FV_helper = paraFV;
	
	//float3 F0 = SpecularColor;
	float3 FV = SpecularColor * FV_helper.x + float3(FV_helper.y, FV_helper.y, FV_helper.y);
	
	return D * FV;
}

float3 FresnelSchlickRoughness(float cosTheta, float3 F0, float roughness)
{
	//return F0 + (max(float3(1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
	float3 ret = float3(0.0, 0.0, 0.0);
	float powTheta = pow(1.0 - cosTheta, 5.0);
	float oneMinusRough = float(1.0 - roughness);
	
	ret.x = F0.x + (max(oneMinusRough, F0.x) - F0.x) * powTheta;
	ret.y = F0.y + (max(oneMinusRough, F0.y) - F0.y) * powTheta;
	ret.z = F0.z + (max(oneMinusRough, F0.z) - F0.z) * powTheta;
	
	return ret;
}

// GGX / Trowbridge-Reitz
// [Walter et al. 2007, "Microfacet models for refraction through rough surfaces"]
float D_GGX(float a2, float NoH)
{
	float d = (NoH * a2 - NoH) * NoH + 1;	// 2 mad
	return a2 / (PI*d*d);					// 4 mul, 1 rcp
}

// Appoximation of joint Smith term for GGX
// [Heitz 2014, "Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs"]
float Vis_SmithJointApprox(float a2, float NoV, float NoL)
{
	float a = sqrt(a2);
	float Vis_SmithV = NoL * (NoV * (1.0f - a) + a);
	float Vis_SmithL = NoV * (NoL * (1.0f - a) + a);
	return 0.5 * (1.0 / max(Vis_SmithV + Vis_SmithL, 0.001));
}

float Pow5(float x)
{
	float xx = x * x;
	return xx * xx * x;
}

// [Schlick 1994, "An Inexpensive BRDF Model for Physically-Based Rendering"]
float3 F_Schlick(float3 SpecularColor, float VoH)
{
	float Fc = Pow5(1.0f - VoH);		// 1 sub, 3 mul
	//return Fc + (1 - Fc) * SpecularColor;		// 1 add, 3 mad
	
	// Anything less than 2% is physically impossible and is instead considered to be shadowing
	//return saturate(50.0f * SpecularColor.g) * Fc + (1.0f - Fc) * SpecularColor;
	return saturate(SpecularColor.g) * Fc + (1.0f - Fc) * SpecularColor;
	
}

float3 SpecularGGX(float Roughness, float3 SpecularColor, float NoL, float Nov, float NoH, float VoH)
{
	float a = Roughness * Roughness;
	float a2 = a * a;
	//float Energy = EnergyNormalization(a2, Context.VoH, AreaLight);
	
	// Generalized microfacet specular
	//float D = D_GGX(a2, Context.NoH) * Energy;
	float D = D_GGX(a2, NoH);
	float Vis = Vis_SmithJointApprox(a2, Nov, NoL);
	SpecularColor = F_Schlick(SpecularColor, VoH);
	
	return (D * Vis) * SpecularColor;
}

float3 PBR(float NoL, float NoV, float3 LightVec, float3 ViewVec,
		   float3 HalfVec, float3 NormalVec, float3 ReflectVec, float3 albedo,
		   float3 specColor, float Roughness, float Metallic, bool isBackface, float shadowFactor)
{
	//float3 diffuseTerm = float3(0.0f, 0.0f, 0.0f);
	float3 specularTerm = float3(0.0f, 0.0f, 0.0f);
	
	//float LoH = clamp(dot(LightVec, HalfVec), 0.0f, 1.0f);
	
	float NoH = saturate(dot(NormalVec, HalfVec));
	float VoH = saturate(dot(ViewVec, HalfVec));
	
	//DIFFUSE
	specColor = mix(0.08 * specColor.rgb, albedo, Metallic);
	albedo = (1.0 - Metallic) * albedo;
	
	
	//SPECULAR
	if (!isBackface)
		specularTerm = mix(float3(0.0f, 0.0f, 0.0f), SpecularGGX(Roughness, specColor, NoL, NoV, NoH, VoH), shadowFactor);
	
	return (albedo + specularTerm);
}

float3 PBR(float NoL, float NoV, float3 LightVec, float3 ViewVec,
		   float3 HalfVec, float3 NormalVec, float3 ReflectVec, float3 albedo,
		   float3 specColor, float Roughness, float Metallic, bool isBackface)
{
	//float3 diffuseTerm = float3(0.0f, 0.0f, 0.0f);
	float3 specularTerm = float3(0.0f, 0.0f, 0.0f);
	
	//float LoH = clamp(dot(LightVec, HalfVec), 0.0f, 1.0f);
	
	float NoH = saturate(dot(NormalVec, HalfVec));
	float VoH = saturate(dot(ViewVec, HalfVec));
	
	//DIFFUSE
	specColor = mix(0.08 * specColor.rgb, albedo, Metallic);
	albedo = (1.0 - Metallic) * albedo;
	
	
	//SPECULAR
	if (!isBackface)
		specularTerm = SpecularGGX(Roughness, specColor, NoL, NoV, NoH, VoH);
	
	return (albedo + specularTerm);
}

float random(float3 seed, float3 freq)
{
	// project seed on random constant vector
	float dt = dot(floor(seed*freq), float3(53.1215, 21.1352, 9.1322));
	// return only the fractional part
	return fract(sin(dt)*2105.2354);
}

float3 calculateSpecular(float3 specularColor, float3 camPos, float3 pixelPos, float3 normalizedDirToLight, float3 normal)
{
	float3 viewVec = normalize(camPos - pixelPos);
	float3 halfVec = normalize(viewVec + normalizedDirToLight);
	float specIntensity = 128;
	float specular = pow(saturate(dot(halfVec, normal)), specIntensity);
	return specularColor * specular;
}

float linearDepth(float depth)
{
	//float f = 2000.0;
	//float n = 10.0;
	return (20.0f) / (2010.0f - depth * (1990.0f));
}

float3 calculateIllumination(
							 float3 normal,
							 float3 ViewVec,
							 float3 HalfVec,
							 float3 ReflectVec,
							 float NoL,
							 float NoV,
							 float3 camPos,
							 float3 normalizedDirToLight,
							 float3 position,
							 float3 albedo,
							 float3 specularColor,
							 float Roughness,
							 float Metallic,
							 bool isBackface,
							 float isPBR,
							 float shadowFactor)
{
	
	float3 finalColor;
	
	if (isPBR > 0.5f)
	{
		finalColor = PBR(NoL, NoV, -normalizedDirToLight, ViewVec, HalfVec, normal, ReflectVec, albedo, specularColor, Roughness, Metallic, isBackface, shadowFactor);
	}
	else
	{
		specularColor = calculateSpecular(specularColor, camPos, position, -normalizedDirToLight, normal);
		finalColor = albedo + mix(float3(0.0, 0.0, 0.0), specularColor, shadowFactor);
	}
	
	finalColor *= shadowFactor;
	
	return finalColor;
}


#endif

