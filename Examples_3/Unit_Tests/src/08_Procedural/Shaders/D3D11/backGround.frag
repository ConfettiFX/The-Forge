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

struct VSBGOutput {
	float4 Position : SV_POSITION;
	float2 fs_UV : TEXTURE0;
};

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

//refer to Morgan McGuire's Earth-like Tiny Planet
float3 addStars(float2 screenSize, float2 fs_UV)
{
	float time = u_TimeInfo.x;

	// Background starfield
	float galaxyClump = (pow(noise(fs_UV.xy * (30.0 * screenSize.x)), 3.0) * 0.5 + pow(noise(100.0 + fs_UV.xy * (15.0 * screenSize.x)), 5.0)) / 3.5;

	float color = galaxyClump * pow(hash(fs_UV.xy), 1500.0) * 80.0;
	float3 starColor = float3(color, color, color);

	starColor.x *= sqrt(noise(fs_UV.xy) * 1.2);
	starColor.y *= sqrt(noise(fs_UV.xy * 4.0));

	float2 delta = (fs_UV.xy - screenSize.xy * 0.5) * screenSize.y * 1.2;
	float radialNoise = lerp(1.0, noise(normalize(delta) * 20.0 + time * 0.5), 0.12);

	float att = 0.057 * pow(max(0.0, 1.0 - (length(delta) - 0.9) / 0.9), 8.0);

	starColor += radialNoise * u_AtmosphereColor.xyz * min(1.0, att);

	float randSeed = rand(fs_UV);

	return starColor * ((sin(randSeed + randSeed * time* 0.05) + 1.0)* 0.4 + 0.2);
}

float4 main(VSBGOutput input) : SV_TARGET
{
	float4 out_Col = float4(0.0, 0.0, 0.0, 1.0);

	float2 screenSize = u_screenSize.xy;

	// Background stars
	out_Col.xyz += addStars(screenSize, input.fs_UV);
	return out_Col;
}