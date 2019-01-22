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

cbuffer cbCamera : register(b0) 
{
	float4x4 projView;
	float4x4 invProjView;
	float3 camPos;
}

cbuffer cbObject : register(b1)
{
	float4x4 worldMat;
	float3 albedo;
	float roughness;
	float metalness;

	// specifies which texture maps are to be used 
	// instead of the constant buffer values above.
	int textureConfig;
}

struct VSInput
{
	float3 Position : POSITION;
	float3 Normal	: NORMAL;
	float2 Uv		: TEXCOORD0;
};

struct VSOutput 
{
	float4 Position	: SV_POSITION;
	float3 pos		: POSITION;
	float3 normal	: NORMAL;
	float2 uv		: TEXCOORD0;
};

VSOutput main(VSInput input)
{
	VSOutput result;
	float4x4 tempMat = mul(projView, worldMat);
	result.Position = mul(tempMat, float4(input.Position.xyz, 1.0f));

	result.pos = float3(mul(worldMat, float4(input.Position.xyz, 1.0f)).rgb);
	result.normal = mul(worldMat, float4(input.Normal.rgb, 0.0f)).rgb;
	result.uv = input.Uv;
	return result;
}
