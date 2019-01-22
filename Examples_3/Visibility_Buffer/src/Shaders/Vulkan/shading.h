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

#include "shader_defs.h"

#define SHADOW_PCF 0
#define SHADOW_ESM 1
#define SHADOW SHADOW_PCF
#define NUM_SHADOW_SAMPLES 32
const float NUM_SHADOW_SAMPLES_INV = float(0.03125);
#if NUM_SHADOW_SAMPLES==16
const float shadowSamples[(NUM_SHADOW_SAMPLES * 2)] = { (-0.17466460), (-0.79131840), 0.08863912, (-0.8981690), 0.17484090, (-0.5252063), 0.45293192, (-0.384986), 0.3857658, (-0.9096935), 0.768011, (-0.4906538), 0.6946555, 0.16058660, 0.7986544, 0.53259124, 0.2847693, 0.2293397, (-0.4357893), (-0.3808875), (-0.139129), 0.23940650, 0.4287854, 0.899425, (-0.6924323), (-0.2203967), (-0.26117242), 0.7359962, (-0.8501040), 0.12639350, (-0.5380967), 0.6264234 };
#else
const float shadowSamples[(NUM_SHADOW_SAMPLES * 2)] = { (-0.17466460), (-0.79131840), (-0.129792), (-0.44771160), 0.08863912, (-0.8981690), (-0.58914988), (-0.678163), 0.17484090, (-0.5252063), 0.6483325, (-0.752117), 0.45293192, (-0.384986), 0.09757467, (-0.1166954), 0.3857658, (-0.9096935), 0.56130584, (-0.1283066), 0.768011, (-0.4906538), 0.8499438, (-0.220937), 0.6946555, 0.16058660, 0.9614297, 0.0597522, 0.7986544, 0.53259124, 0.45139648, 0.5592551, 0.2847693, 0.2293397, (-0.2118996), (-0.1609127), (-0.4357893), (-0.3808875), (-0.4662672), (-0.05288446), (-0.139129), 0.23940650, 0.1781853, 0.5254948, 0.4287854, 0.899425, 0.12893490, 0.8724155, (-0.6924323), (-0.2203967), (-0.48997), 0.2795907, (-0.26117242), 0.7359962, (-0.7704172), 0.42331340, (-0.8501040), 0.12639350, (-0.83452672), (-0.499136), (-0.5380967), 0.6264234, (-0.9769312), (-0.15505689) };
#endif

float random(vec3 seed, float freq)
{
	// project seed on random constant vector
	float dt = dot(floor(seed * freq), vec3(53.1215f, 21.1352f, 9.1322f));
	// return only the fractional part
	return fract(sin(dt)*2105.2354f);
}

const float PI = 3.14159274;
vec2 LightingFunGGX_FV(float dotLH, float roughness)
{
	float alpha = (roughness * roughness);
	float F_a, F_b;
	float dotLH5 = pow(clamp((1.0 - dotLH), 0.0, 1.0), float(5.0));
	(F_a = 1.0);
	(F_b = dotLH5);
	float vis;
	float k = (alpha * 0.5);
	float k2 = (k * k);
	float invK2 = (1.0 - k2);
	(vis = (1.0 / (((dotLH * dotLH) * invK2) + k2)));
	return vec2(((F_a - F_b) * vis), (F_b * vis));
}
float LightingFuncGGX_D(float dotNH, float roughness)
{
	float alpha = (roughness * roughness);
	float alphaSqr = (alpha * alpha);
	float denom = max((((dotNH * dotNH) * (alphaSqr - 1.0)) + 1.0), 0.0010000000);
	return (alphaSqr / ((PI * denom) * denom));
}
vec3 GGX_Spec(vec3 Normal, vec3 HalfVec, float Roughness, vec3 SpecularColor, vec2 paraFV)
{
	float NoH = clamp(dot(Normal, HalfVec), 0.0, 1.0);
	float NoH2 = (NoH * NoH);
	float NoH4 = (NoH2 * NoH2);
	float D = LightingFuncGGX_D(NoH4, Roughness);
	vec2 FV_helper = paraFV;
	vec3 FV = ((SpecularColor * vec3((FV_helper).x)) + vec3((FV_helper).y, (FV_helper).y, (FV_helper).y));
	return (vec3(D) * FV);
}
vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
	vec3 ret = vec3(0.0, 0.0, 0.0);
	float powTheta = pow((float(1.0) - cosTheta), float(5.0));
	float oneMinusRough = float((float(1.0) - roughness));
	((ret).x = ((F0).x + ((max(oneMinusRough, (F0).x) - (F0).x) * powTheta)));
	((ret).y = ((F0).y + ((max(oneMinusRough, (F0).y) - (F0).y) * powTheta)));
	((ret).z = ((F0).z + ((max(oneMinusRough, (F0).z) - (F0).z) * powTheta)));
	return ret;
}
float D_GGX(float a2, float NoH)
{
	float d = ((((NoH * a2) - NoH) * NoH) + float(1));
	return (a2 / ((PI * d) * d));
}
float Vis_SmithJointApprox(float a2, float NoV, float NoL)
{
	float a = sqrt(a2);
	float Vis_SmithV = (NoL * ((NoV * (1.0 - a)) + a));
	float Vis_SmithL = (NoV * ((NoL * (1.0 - a)) + a));
	return (float(0.5) * 1 / max((Vis_SmithV + Vis_SmithL), float(0.0010000000)));
}
float Pow5(float x)
{
	float xx = (x * x);
	return ((xx * xx) * x);
}
vec3 F_Schlick(vec3 SpecularColor, float VoH)
{
	float Fc = Pow5((1.0 - VoH));
	return (vec3((clamp((SpecularColor).g, 0.0, 1.0) * Fc)) + (vec3((1.0 - Fc)) * SpecularColor));
}
vec3 SpecularGGX(float Roughness, inout vec3 SpecularColor, float NoL, float Nov, float NoH, float VoH)
{
	float a = (Roughness * Roughness);
	float a2 = (a * a);
	float D = D_GGX(a2, NoH);
	float Vis = Vis_SmithJointApprox(a2, Nov, NoL);
	(SpecularColor = F_Schlick(SpecularColor, VoH));
	return (vec3((D * Vis)) * SpecularColor);
}
vec3 PBR(float NoL, float NoV, vec3 LightVec, vec3 ViewVec, vec3 HalfVec, vec3 NormalVec, vec3 ReflectVec, inout vec3 albedo, inout vec3 specColor, float Roughness, float Metallic, bool isBackface, float shadowFactor)
{
	vec3 specularTerm = vec3(0.0, 0.0, 0.0);
	float NoH = clamp(dot(NormalVec, HalfVec), 0.0, 1.0);
	float VoH = clamp(dot(ViewVec, HalfVec), 0.0, 1.0);
	(specColor = vec3(mix(vec3((vec3(0.080000000) * (specColor).rgb)), vec3(albedo), vec3(Metallic))));
	(albedo = (vec3((float(1.0) - Metallic)) * albedo));
	if ((!isBackface))
	{
		(specularTerm = vec3(mix(vec3(vec3(0.0, 0.0, 0.0)), vec3(SpecularGGX(Roughness, specColor, NoL, NoV, NoH, VoH)), vec3(shadowFactor))));
	}
	return (albedo + specularTerm);
}
vec3 PBR(float NoL, float NoV, vec3 LightVec, vec3 ViewVec, vec3 HalfVec, vec3 NormalVec, vec3 ReflectVec, inout vec3 albedo, inout vec3 specColor, float Roughness, float Metallic, bool isBackface)
{
	vec3 specularTerm = vec3(0.0, 0.0, 0.0);
	float NoH = clamp(dot(NormalVec, HalfVec), 0.0, 1.0);
	float VoH = clamp(dot(ViewVec, HalfVec), 0.0, 1.0);
	(specColor = vec3(mix(vec3((vec3(0.080000000) * (specColor).rgb)), vec3(albedo), vec3(Metallic))));
	(albedo = (vec3((float(1.0) - Metallic)) * albedo));
	if ((!isBackface))
	{
		(specularTerm = SpecularGGX(Roughness, specColor, NoL, NoV, NoH, VoH));
	}
	return (albedo + specularTerm);
}
float random(vec3 seed, vec3 freq)
{
	float dt = dot(floor((seed * freq)), vec3(53.12149811, 21.1352005, 9.13220024));
	return fract((sin(dt) * float(2105.23535156)));
}
vec3 calculateSpecular(vec3 specularColor, vec3 camPos, vec3 pixelPos, vec3 normalizedDirToLight, vec3 normal)
{
	vec3 viewVec = normalize((camPos - pixelPos));
	vec3 halfVec = normalize((viewVec + normalizedDirToLight));
	float specIntensity = float(128);
	float specular = pow(clamp(dot(halfVec, normal), 0.0, 1.0), float(specIntensity));
	return (specularColor * vec3(specular));
}
float linearDepth(float depth)
{
	return (20.0 / (2010.0 - (depth * 1990.0)));
}
vec3 calculateIllumination(vec3 normal, vec3 ViewVec, vec3 HalfVec, vec3 ReflectVec, float NoL, float NoV, vec3 camPos, float esmControl, vec3 normalizedDirToLight, vec4 posLS, vec3 position, texture2D shadowMap, vec3 albedo, vec3 specularColor, float Roughness, float Metallic, sampler sh, bool isBackface, float isPBR, out float shadowFactor)
{
	(posLS /= vec4((posLS).w));
	((posLS).y *= float((-1)));
	((posLS).xy = (((posLS).xy * vec2(0.5)) + vec2(0.5, 0.5)));
	vec2 HalfGaps = vec2(0.00048828124, 0.00048828124);
	vec2 Gaps = vec2(0.0009765625, 0.0009765625);
	((posLS).xy += HalfGaps);
	(shadowFactor = 0.0);
	float isInShadow = 0.0;
	if ((all(greaterThan((posLS).xy, vec2(0))) && all(lessThan((posLS).xy, vec2(1)))))
	{
#if SHADOW==SHADOW_PCF
		float shadowFilterSize = float(0.0016000000);
		float angle = random(position, vec3(20.0));
		float s = sin(angle);
		float c = cos(angle);
		for (int i = 0; (i < NUM_SHADOW_SAMPLES); (i++))
		{
			vec2 offset = vec2(shadowSamples[(i * 2)], shadowSamples[((i * 2) + 1)]);
			(offset = vec2((((offset).x * c) + ((offset).y * s)), (((offset).x * (-s)) + ((offset).y * c))));
			(offset *= vec2(shadowFilterSize));
			float shadowMapValue = float(textureLod(sampler2D(shadowMap, sh), vec2(((posLS).xy + offset)), float(0)));
			(shadowFactor += float((((shadowMapValue < ((posLS).z - float(0.0020000000)))) ? (float(0)) : (float(1)))));
		}
		(shadowFactor *= NUM_SHADOW_SAMPLES_INV);
#elif SHADOW==SHADOW_ESM
		float CShadow = linearDepth(float((textureLod(sampler2D(shadowMap, sh), vec2((posLS).xy), float(0))).r));
		float LShadow = linearDepth(float((textureLod(sampler2D(shadowMap, sh), vec2(((posLS).xy + vec2((-(Gaps).x), 0.0))), float(0))).r));
		float RShadow = linearDepth(float((textureLod(sampler2D(shadowMap, sh), vec2(((posLS).xy + vec2((Gaps).x, 0.0))), float(0))).r));
		float LTShadow = linearDepth(float((textureLod(sampler2D(shadowMap, sh), vec2(((posLS).xy + vec2((-(Gaps).x), (Gaps).y))), float(0))).r));
		float RTShadow = linearDepth(float((textureLod(sampler2D(shadowMap, sh), vec2(((posLS).xy + vec2((Gaps).x, (Gaps).y))), float(0))).r));
		float TShadow = linearDepth(float((textureLod(sampler2D(shadowMap, sh), vec2(((posLS).xy + vec2(0.0, (Gaps).y))), float(0))).r));
		float BShadow = linearDepth(float((textureLod(sampler2D(shadowMap, sh), vec2(((posLS).xy + vec2(0.0, (-(Gaps).y)))), float(0))).r));
		float LBShadow = linearDepth(float((textureLod(sampler2D(shadowMap, sh), vec2(((posLS).xy + vec2((-(Gaps).x), (-(Gaps).y)))), float(0))).r));
		float RBShadow = linearDepth(float((textureLod(sampler2D(shadowMap, sh), vec2(((posLS).xy + vec2((Gaps).x, (-(Gaps).y)))), float(0))).r));
		float nbShadow = (((((((LShadow + RShadow) + TShadow) + BShadow) + LTShadow) + RTShadow) + LBShadow) + RBShadow);
		float avgShadowDepthSample = ((CShadow + nbShadow) / 9.0);
		float linearD = linearDepth((posLS).z);
		float esm = (exp(((-esmControl) * linearD)) * exp((esmControl * avgShadowDepthSample)));
		(shadowFactor = clamp(esm, 0.0, 1.0));
#endif
	}

	vec3 finalColor;
	if ((isPBR > 0.5))
	{
		(finalColor = PBR(NoL, NoV, (-normalizedDirToLight), ViewVec, HalfVec, normal, ReflectVec, albedo, specularColor, Roughness, Metallic, isBackface, shadowFactor));
	}
	else
	{
		(specularColor = calculateSpecular(specularColor, camPos, position, (-normalizedDirToLight), normal));
		(finalColor = (albedo + vec3(mix(vec3(vec3(0.0, 0.0, 0.0)), vec3(specularColor), vec3(shadowFactor)))));
	}
	(finalColor *= vec3(shadowFactor));
	return finalColor;
}
vec3 pointLightShade(vec3 normal, vec3 ViewVec, vec3 HalfVec, vec3 ReflectVec, float NoL, float NoV, vec3 lightPos, vec3 lightCol, vec3 camPos, vec3 normalizedDirToLight, vec4 posLS, vec3 position, vec3 albedo, vec3 specularColor, float Roughness, float Metallic, bool isBackface, float isPBR)
{
	vec3 lVec = ((lightPos - position) * vec3((float(1.0) / LIGHT_SIZE)));
	vec3 lightVec = normalize(lVec);
	float atten = clamp((1.0 - dot(lVec, lVec)), 0.0, 1.0);
	vec3 finalColor;
	if ((isPBR > 0.5))
	{
		(finalColor = PBR(NoL, NoV, (-normalizedDirToLight), ViewVec, HalfVec, normal, ReflectVec, albedo, specularColor, Roughness, Metallic, isBackface));
	}
	else
	{
		(specularColor = calculateSpecular(specularColor, camPos, position, (-normalizedDirToLight), normal));
		(finalColor = (albedo + specularColor));
	}
	return ((lightCol * finalColor) * vec3(atten));
}