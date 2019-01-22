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


// source: https://www.shadertoy.com/view/4sfGzS
float hash(float3 p)
{
	p = frac(p*0.3183099 + .1);
	p *= 17.0;
	return frac(p.x*p.y*p.z*(p.x + p.y + p.z));
}

float noise(in float3 x)
{
	float3 p = floor(x);
	float3 f = frac(x);
	f = f * f*(3.0 - 2.0*f);

	return lerp(lerp(lerp(hash(p + float3(0, 0, 0)),
		hash(p + float3(1, 0, 0)), f.x),
		lerp(hash(p + float3(0, 1, 0)),
		hash(p + float3(1, 1, 0)), f.x), f.y),
		lerp(lerp(hash(p + float3(0, 0, 1)),
		hash(p + float3(1, 0, 1)), f.x),
		lerp(hash(p + float3(0, 1, 1)),
		hash(p + float3(1, 1, 1)), f.x), f.y), f.z);
}

struct VSOutput
{
	float4 Position : SV_POSITION;
	float4 WorldPosition : POSITION;
	float4 Normal : NORMAL;
	float4 UV : TEXCOORD0;
	uint MatID : MAT_ID;
};

struct PSOutput
{
	float2 RedVarianceShadowMap : SV_Target0;
	float2 GreenVarianceShadowMap : SV_Target1;
	float2 BlueVarianceShadowMap : SV_Target2;
};

PSOutput main(VSOutput input)
{ 
	Material mat = Materials[input.MatID];
	float4 matColor = mat.TextureFlags & 1 ? MaterialTextures[mat.AlbedoTexID].Sample(LinearSampler, input.UV.xy) : mat.Color;
	float3 p = (1.0f - mat.Transmission) * matColor.a;
	float e = noise(input.WorldPosition.xyz * 10000.0f);

	float3 normal = normalize(input.Normal.xyz);

	float3 ld = float3(camViewMat[2][0], camViewMat[2][1], camViewMat[2][2]);
	float s = saturate((mat.RefractionRatio - 1.0f) * 0.5f);
	float g = 2.0f * saturate(1.0f - pow(dot(normalize(normal), -ld.xyz), 128.0f * s*s)) - 1.0f;
	p = min(1.0f, (1.0f + g * pow(s, 0.2f)) * p);

	PSOutput output;
	float2 moments = ComputeMoments(input.Position.z);
	output.RedVarianceShadowMap = max(moments, e > p.r);
	output.GreenVarianceShadowMap = max(moments, e > p.g);
	output.BlueVarianceShadowMap = max(moments, e > p.b);
	return output;
}
