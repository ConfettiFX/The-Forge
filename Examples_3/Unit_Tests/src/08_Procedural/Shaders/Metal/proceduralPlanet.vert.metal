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

#include <metal_stdlib>
using namespace metal;

struct CameraData
{
    float4x4 projView;
    float3 camPos;
    float pad_0;
};

struct ObjectData
{
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
};

struct VSInput
{
    float4 vs_Pos [[attribute(0)]];
    float4 vs_Nor [[attribute(1)]];
};

struct VSOutput {
	float4 Position [[position]];
	float4 fs_Pos;
	float4 fs_Nor;

	float4 fs_Col;
	float4 fs_TerrainInfo;
	float4 fs_transedPos;
};

float hash(float2 p)
{
    return fract(1e4 * sin(17.0 * p.x + p.y * 0.1) * (0.1 + abs(sin(p.y * 13.0 + p.x))));
}

float noise(float3 x)
{
	float3 step = float3(110, 241, 171);
	float3 i = floor(x);
	float3 f = fract(x);
	float n = dot(i, step);
	float3 u = f * f * (3.0 - 2.0 * f);
	return mix(mix(mix(hash(n + dot(step, float3(0, 0, 0))), hash(n + dot(step, float3(1, 0, 0))), u.x),
		mix(hash(n + dot(step, float3(0, 1, 0))), hash(n + dot(step, float3(1, 1, 0))), u.x), u.y),
		mix(mix(hash(n + dot(step, float3(0, 0, 1))), hash(n + dot(step, float3(1, 0, 1))), u.x),
			mix(hash(n + dot(step, float3(0, 1, 1))), hash(n + dot(step, float3(1, 1, 1))), u.x), u.y), u.z);
}

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

vertex VSOutput stageMain(VSInput input                    [[stage_in]],
                          uint InstanceID                  [[instance_id]],
                          constant CameraData& cbCamera    [[buffer(1)]],
                          constant ObjectData& cbObject    [[buffer(2)]])
{
	VSOutput output;

	output.fs_TerrainInfo = float4(0.0, 0.0, 0.0, 0.0);

	float4 vertexPos = float4(input.vs_Pos.xyz, 1.0);
	output.fs_Pos = vertexPos;

	float oceneHeight = length(vertexPos.xyz) + cbObject.u_HeightsInfo.x;
	float3 localNormal = normalize(vertexPos.xyz);

	float u_resolution = 4.0;

	float noiseResult = fbm(vertexPos.xyz*u_resolution, 6) * 2.0;

	noiseResult = pow(noiseResult, cbObject.u_TimeInfo.z);

	vertexPos.xyz += localNormal * noiseResult;

	float height = length(vertexPos.xyz);

	float gap = clamp((1.0 - (oceneHeight - height)), 0.0, 1.0);
	float gap5 = pow(gap, 3.0);

	float4 ocenColor = cbObject.u_OceanColor * gap5;

	float oceneRougness = 0.15;
	float iceRougness = 0.15;
	float foliageRougness = 0.8;
	float snowRougness = 0.8;
	float shoreRougness = 0.9;

	//ocean
	if (height < oceneHeight)
	{
		vertexPos.xyz = oceneHeight* localNormal;

		output.fs_Pos = vertexPos;
		output.fs_TerrainInfo.w = oceneRougness;
		output.fs_Col = ocenColor;
	}
	//shore
	else
	{
		output.fs_TerrainInfo.x = 0.05;

		float appliedAttitude;

		if (abs(vertexPos.y) > cbObject.u_HeightsInfo.w)
			appliedAttitude = clamp((abs(vertexPos.y) - cbObject.u_HeightsInfo.w) * 3.0, 0.0, 1.0);
		else
			appliedAttitude = 0.0;

		float4 terrainColor = mix(cbObject.u_FoliageColor, cbObject.u_PolarCapsColor, appliedAttitude);
		float terrainRoughness = mix(foliageRougness, iceRougness, appliedAttitude);

		vertexPos.xyz = height * localNormal;

		float oceneLine = oceneHeight + cbObject.u_HeightsInfo.y;
		float snowLine = 1.0 + cbObject.u_HeightsInfo.z;

		if (height < oceneLine)
		{
			output.fs_Col = cbObject.u_ShorelineColor;
			output.fs_TerrainInfo.w = shoreRougness;
		}
		else if (height >= snowLine)
		{
			output.fs_TerrainInfo.x = 0.15;

			float alpha = clamp((height - snowLine) / 0.03, 0.0, 1.0);
			output.fs_Col = mix(terrainColor, cbObject.u_SnowColor, alpha);

			output.fs_TerrainInfo.w = mix(terrainRoughness, snowRougness, alpha);
		}
		else
		{
			float alpha = clamp((height - oceneLine) / cbObject.u_HeightsInfo.y, 0.0, 1.0);
			output.fs_Col = mix(cbObject.u_ShorelineColor, terrainColor, alpha);

			output.fs_TerrainInfo.w = mix(shoreRougness, terrainRoughness, alpha);
		}
	}

	float4 modelposition = cbObject.worldMat * vertexPos;

	output.fs_transedPos = modelposition;
	output.Position = cbCamera.projView * modelposition;

	return output;
}
