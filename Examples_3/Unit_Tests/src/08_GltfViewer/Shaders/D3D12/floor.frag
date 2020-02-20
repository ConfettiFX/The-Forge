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

Texture2D ShadowTexture				: register(t14);
SamplerState clampMiplessLinearSampler : register(s7);

struct VSOutput
{
	float4 Position : SV_POSITION;
	float3 WorldPos : POSITION;
    float2 TexCoord : TEXCOORD;
};

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

	/*
	if ( !all(posLS.xy > 0) || !all(posLS.xy < 1))
	{
		return shadowFactor;
	}
	*/

	float4 shadowDepthSample = float4(0, 0, 0, 0);
	shadowDepthSample.x = ShadowTexture.SampleLevel(clampMiplessLinearSampler, posLS.xy, 0).r;
	shadowDepthSample.y = ShadowTexture.SampleLevel(clampMiplessLinearSampler, posLS.xy, 0, int2(1, 0)).r;
	shadowDepthSample.z = ShadowTexture.SampleLevel(clampMiplessLinearSampler, posLS.xy, 0, int2(0, 1)).r;
	shadowDepthSample.w = ShadowTexture.SampleLevel(clampMiplessLinearSampler, posLS.xy, 0, int2(1, 1)).r;
	float avgShadowDepthSample = (shadowDepthSample.x + shadowDepthSample.y + shadowDepthSample.z + shadowDepthSample.w) * 0.25f;
	shadowFactor = saturate(2.0 - exp((posLS.z - avgShadowDepthSample) * 1.0f ));
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

float ClaculateShadow(float3 worldPos)
{
	float4 NDC = mul(shadowLightViewProj, float4(worldPos, 1.0));
	NDC /= NDC.w;
	float Depth = NDC.z;
	float2 ShadowCoord = float2((NDC.x + 1.0)*0.5, (1.0 - NDC.y)*0.5);
	float ShadowDepth = ShadowTexture.Sample(clampMiplessLinearSampler, ShadowCoord).r;
	

	if(ShadowDepth - 0.002f > Depth)
		return 0.1f;
	else
		return 1.0f;
}

float4 main(VSOutput input) : SV_TARGET
{
	float3 color = float3(1.0, 1.0, 1.0);

	color *= CalcPCFShadowFactor(input.WorldPos);

	float i = 1.0 - length(abs(input.TexCoord.xy));
	i = pow(i, 1.2f);	
	return float4(color.rgb, i);
}