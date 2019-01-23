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


struct Light
{
	float4 pos;
	float4 col;
	float radius;
	float intensity;
};

static const float PI = 3.14159265359;

cbuffer cbCamera : register(b0) {
	float4x4 projView;
	float3 camPos;
	float pad_0;
}

cbuffer cbObject : register(b1) {
	float4x4 worldMat;
	float4x4 invWorldMat;

	float4 u_OceanColor;
	float4 u_ShorelineColor;
	float4 u_FoliageColor;
	float4 u_MountainsColor;

	float4 u_SnowColor;
	float4 u_PolarCapsColor;
	float4 u_AtmosphereColor;
	float4 u_HeightsInfo; // x : Ocean, y : Shore, z : Snow, w : Polar

	float4 u_TimeInfo; //time, controls.Noise4D, controls.TerrainExp, controls.TerrainSeed * 39.0
}

cbuffer cbLights : register(b2) {
	int currAmountOflights;
	int pad0;
	int pad1;
	int pad2;
	Light lights[16];
}


cbuffer cbScreen : register(b3) {
	float4 u_screenSize;
}

SamplerState uSampler0 : register(s7);
Texture2D uEnvTex0 : register(t1);


struct VSInput
{
    float4 vs_Pos : POSITION;
    float4 vs_Nor : NORMAL;
};

struct VSOutput {
	float4 Position : SV_POSITION;
	float4 fs_Pos : POSITION;
	float4 fs_Nor : NORMAL;

	float4 fs_Col : TEXTURE0;
	float4 fs_TerrainInfo : TEXTURE1;
	float4 fs_transedPos : TEXTURE2;
};

struct VSBGInput
{
	float4 vs_Pos : POSITION;
	float4 vs_Nor : NORMAL;
};

struct VSBGOutput {
	float4 Position : SV_POSITION;
	float2 fs_UV : TEXTURE0;
};



float2 LightingFunGGX_FV(float dotLH, float roughness)
{
	float alpha = roughness * roughness;

	//F
	float F_a, F_b;
	float dotLH5 = pow(clamp(1.0f - dotLH, 0.0f, 1.0f), 5.0f);
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
	float denom = dotNH * dotNH * (alphaSqr - 1.0f) + 1.0f;

	return alphaSqr / (PI*denom*denom);
}

float3 GGX_Spec(float3 Normal, float3 HalfVec, float Roughness, float3 BaseColor, float3 SpecularColor, float2 paraFV)
{
	float NoH = clamp(dot(Normal, HalfVec), 0.0, 1.0);

	float D = LightingFuncGGX_D(NoH * NoH * NoH * NoH, Roughness);
	float2 FV_helper = paraFV;

	float3 F0 = SpecularColor;
	float3 FV = F0 * FV_helper.x + float3(FV_helper.y, FV_helper.y, FV_helper.y);

	return D * FV;
}

float hash(float n)
{
	//4D
	if (u_TimeInfo.y > 0.0)
	{
		return frac(sin(n) *cos(u_TimeInfo.x * 0.00001) * 1e4);
	}
	else
	{
		return frac(sin(n) * cos(u_TimeInfo.w * 0.00001) * 1e4);
	}

}

float hash(float2 p) { return frac(1e4 * sin(17.0 * p.x + p.y * 0.1) * (0.1 + abs(sin(p.y * 13.0 + p.x)))); }
float noise(float x) { float i = floor(x); float f = frac(x); float u = f * f * (3.0 - 2.0 * f); return lerp(hash(i), hash(i + 1.0), u); }
float noise(float2 x) { float2 i = floor(x); float2 f = frac(x); float a = hash(i); float b = hash(i + float2(1.0, 0.0)); float c = hash(i + float2(0.0, 1.0)); float d = hash(i + float2(1.0, 1.0)); float2 u = f * f * (3.0 - 2.0 * f); return lerp(a, b, u.x) + (c - a) * u.y * (1.0 - u.x) + (d - b) * u.x * u.y; }

float rand(float2 co) {
	return frac(sin(dot(co.xy, float2(12.9898, 78.233))) * 43758.5453);
}

float noise(float3 x)
{
	float3 step = float3(110, 241, 171);
	float3 i = floor(x);
	float3 f = frac(x);
	float n = dot(i, step);
	float3 u = f * f * (3.0 - 2.0 * f);
	return lerp(lerp(lerp(hash(n + dot(step, float3(0, 0, 0))), hash(n + dot(step, float3(1, 0, 0))), u.x),
		lerp(hash(n + dot(step, float3(0, 1, 0))), hash(n + dot(step, float3(1, 1, 0))), u.x), u.y),
		lerp(lerp(hash(n + dot(step, float3(0, 0, 1))), hash(n + dot(step, float3(1, 0, 1))), u.x),
			lerp(hash(n + dot(step, float3(0, 1, 1))), hash(n + dot(step, float3(1, 1, 1))), u.x), u.y), u.z);
}

#define PI 3.1415926535897932384626422832795028841971
#define TwoPi 6.28318530717958647692
#define InvPi 0.31830988618379067154
#define Inv2Pi 0.15915494309189533577
#define Inv4Pi 0.07957747154594766788

#define Epsilon 0.0001

float fbm(float3 x, int LOD)
{
	float v = 0.0;
	float a = 0.5;
	float3 shift = float3(100.0, 100.0, 100.0);

	for (int i = 0; i < LOD; ++i)
	{
		v += a * noise(x);
		x = x * 2.0 + shift;
		a *= 0.5;
	}
	return v;
}

float SphericalTheta(float3 v)
{
	return acos(clamp(v.y, -1.0f, 1.0f));
}

float SphericalPhi(float3 v)
{
	float p = atan2(v.x, v.z);
	return (p < 0.0f) ? (p + TwoPi) : p;
}

VSOutput main(VSInput input, uint InstanceID : SV_InstanceID)
{
	VSOutput output = (VSOutput)0;

	output.fs_TerrainInfo = float4(0.0, 0.0, 0.0, 0.0);

	float4 vertexPos = float4(input.vs_Pos.xyz, 1.0);
	output.fs_Pos = vertexPos;

	float oceneHeight = length(vertexPos.xyz) + u_HeightsInfo.x;
	float3 localNormal = normalize(vertexPos.xyz);

	float u_resolution = 4.0;

	float noiseResult = fbm(vertexPos.xyz*u_resolution, 6) * 2.0;

	noiseResult = pow(noiseResult, u_TimeInfo.z);

	vertexPos.xyz += localNormal * noiseResult;

	float height = length(vertexPos.xyz);

	float gap = clamp((1.0 - (oceneHeight - height)), 0.0, 1.0);
	float gap5 = pow(gap, 3.0);



	float4 ocenColor = u_OceanColor * gap5;

	float oceneRougness = 0.15;
	float iceRougness = 0.15;
	float foliageRougness = 0.8;
	float snowRougness = 0.8;
	float shoreRougness = 0.9;

	//ocean
	if (height < oceneHeight)
	{
		//float gap10 = pow(pow(gap, 100.0), 0.8);

		//float wave = OceanNoise(vertexPos.xyz, oceneHeight, noiseResult, gap10);
		//vertexPos.xyz = (oceneHeight + wave) * localNormal;

		vertexPos.xyz = oceneHeight * localNormal;

		output.fs_Pos = vertexPos;
		output.fs_TerrainInfo.w = oceneRougness;
		output.fs_Col = ocenColor;
	}
	//shore
	else
	{
		output.fs_TerrainInfo.x = 0.05;

		float appliedAttitude;

		if (abs(vertexPos.y) > u_HeightsInfo.w)
			appliedAttitude = clamp((abs(vertexPos.y) - u_HeightsInfo.w) * 3.0, 0.0, 1.0);
		else
			appliedAttitude = 0.0;

		float4 terrainColor = lerp(u_FoliageColor, u_PolarCapsColor, appliedAttitude);
		float terrainRoughness = lerp(foliageRougness, iceRougness, appliedAttitude);

		vertexPos.xyz = height * localNormal;

		float oceneLine = oceneHeight + u_HeightsInfo.y;
		float snowLine = 1.0 + u_HeightsInfo.z;

		if (height < oceneLine)
		{
			output.fs_Col = u_ShorelineColor;
			output.fs_TerrainInfo.w = shoreRougness;
		}
		else if (height >= snowLine)
		{
			output.fs_TerrainInfo.x = 0.15;

			float alpha = clamp((height - snowLine) / 0.03, 0.0, 1.0);
			output.fs_Col = lerp(terrainColor, u_SnowColor, alpha);

			output.fs_TerrainInfo.w = lerp(terrainRoughness, snowRougness, alpha);
		}
		else
		{
			float alpha = clamp((height - oceneLine) / u_HeightsInfo.y, 0.0, 1.0);
			output.fs_Col = lerp(u_ShorelineColor, terrainColor, alpha);

			output.fs_TerrainInfo.w = lerp(shoreRougness, terrainRoughness, alpha);
		}
	}

	float4 modelposition = mul(worldMat, vertexPos);

	output.fs_transedPos = modelposition;
	output.Position = mul(projView, modelposition);

	return output;
}
