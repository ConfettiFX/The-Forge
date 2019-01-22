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

#include "shading.hlsl"

struct VSOutput
{
	float4 Position : SV_POSITION;
	float4 WorldPosition : POSITION0;
	float4 Normal : NORMAL0;
	float4 UV : TEXCOORD0;
	uint MatID : MAT_ID;
#if (PT_USE_REFRACTION + PT_USE_DIFFUSION) != 0
	float4 CSPosition : POSITION1;
#endif
#if PT_USE_REFRACTION != 0
	float4 CSNormal : NORMAL1;
#endif
};

struct PSOutput
{
	float4 Accumulation : SV_Target0;
	float4 Modulation : SV_Target1;
#if PT_USE_REFRACTION != 0
	float2 Refraction : SV_Target2;
#endif
};

#if PT_USE_DIFFUSION != 0
struct ObjectInfo
{
	float4x4 toWorld;
	float4x4 normalMat;
	uint matID;
};

cbuffer ObjectUniformBlock : register(b0)
{
	ObjectInfo	objectInfo[MAX_NUM_OBJECTS];
};

Texture2D DepthTexture : register(t0);
SamplerState PointSampler : register(s0);
#endif

float WeightFunction(float alpha, float depth)
{
	float tmp = 1.0f - depth * 0.99f;
	tmp *= tmp * tmp * 1e4f;
	return clamp(alpha * tmp, 1e-3f, 1.5e2);
}

float2 ComputeRefractionOffset(float3 csNormal, float3 csPosition, float eta)
{
	const float2 backSizeInMeters = 1000.0f * (1.0f / ((1.0f - eta) * 100.0f));	// HACK: Background size is supposed to be calculated per object. This is a quick work around that looks good to us.

	const float backgroundZ = csPosition.z - 4.0f;

	float3 dir = normalize(csPosition);
	float3 refracted = refract(dir, csNormal, eta);

	bool totalInternalRefraction = dot(refracted, refracted) < 0.01f;
	if (totalInternalRefraction)
		return 0.0f;
	else
	{
		float3 plane = csPosition;
		plane.z -= backgroundZ;

		float2 hit = (plane.xy - refracted.xy * plane.z / refracted.z);
		float2 backCoord = (hit / backSizeInMeters) + 0.5f;
		float2 startCoord = (csPosition.xy / backSizeInMeters) + 0.5f;
		return backCoord - startCoord;
	}
}

PSOutput main(VSOutput input)
{
	PSOutput output;

	float3 transmission = Materials[input.MatID].Transmission.xyz;
	float collimation = Materials[input.MatID].Collimation;
	float4 finalColor = Shade(input.MatID, input.UV.xy, input.WorldPosition.xyz, normalize(input.Normal.xyz));

	float d = input.Position.z / input.Position.w;
	float4 premultipliedColor = float4(finalColor.rgb * finalColor.a, finalColor.a);
	float coverage = finalColor.a;

	output.Modulation.rgb = coverage * (1.0f - transmission);
	coverage *= 1.0f - (transmission.r + transmission.g + transmission.b) * (1.0f / 3.0f);

	float w = WeightFunction(coverage, d);
	output.Accumulation = float4(premultipliedColor.rgb, coverage) * w;

#if PT_USE_DIFFUSION != 0
	if (collimation < 1.0f)
	{
		float backgroundDepth = DepthTexture.Load(int3(input.Position.xy, 0)).r;
		backgroundDepth = camClipInfo[0] / (camClipInfo[1] * backgroundDepth + camClipInfo[2]);

		const float scaling = 8.0f;
		const float focusRate = 0.1f;

		output.Modulation.a = scaling * coverage * (1.0f - collimation) * (1.0f - focusRate / (focusRate + input.CSPosition.z - backgroundDepth)) / max(abs(input.CSPosition.z), 1e-5f);
		output.Modulation.a *= output.Modulation.a;
		output.Modulation.a = max(output.Modulation.a, 1.0f / 256.0f);
	}
	else
		output.Modulation.a = 0.0f;
#else
	output.Modulation.a = 0.0f;
#endif


#if PT_USE_REFRACTION != 0
	float eta = 1.0f / Materials[input.MatID].RefractionRatio;
	float2 refractionOffset = 0.0f;
	if (eta != 1.0f)
		refractionOffset = ComputeRefractionOffset(normalize(input.CSNormal.xyz), input.CSPosition.xyz, eta);
	output.Refraction = refractionOffset * coverage * 8.0f;
#endif

	return output;
}
