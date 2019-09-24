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

struct VsIn
{
	uint4 position          : POSITION;
	int4 normal            : NORMAL;
	uint2 texCoord          : TEXCOORD0;
	uint4 baseColor         : COLOR;
	uint2 metallicRoughness : TEXCOORD1;
	uint2 alphaSettings     : TEXCOORD2;
};

cbuffer cbPerPass : register(b0, UPDATE_FREQ_PER_FRAME)
{
	float4x4	projView;
	float4      camPos;
	float4      lightColor[4];
	float4      lightDirection[3];
	int4        quantizationParams;
}

cbuffer cbPerProp : register(b1, UPDATE_FREQ_PER_DRAW)
{
	float4x4	world;
	float4x4	InvTranspose;
	int         unlit;
	int         hasAlbedoMap;
	int         hasNormalMap;
	int			hasMetallicRoughnessMap;
	int         hasAOMap;
	int         hasEmissiveMap;
	float4		centerOffset;
	float4		posOffset;
	float2		uvOffset;
	float2		uvScale;
	float		posScale;	
	float		padding0;	
}

struct PsIn
{    
    float3 pos               : POSITION;
	float3 normal	         : NORMAL;
	float2 texCoord          : TEXCOORD0;
	float4 baseColor         : COLOR;
	float2 metallicRoughness : TEXCOORD1;
	float2 alphaSettings     : TEXCOORD2;
    float4 position          : SV_Position;
};

PsIn main(VsIn In)
{
	PsIn Out;

    float unormPositionScale = float(1 << quantizationParams[0]) - 1.0f;
    float unormTexScale = float(1 << quantizationParams[1]) - 1.0f;
    float snormNormalScale = float(1 << (quantizationParams[2] - 1)) - 1.0f;
    float unorm16Scale = float(1 << 16) - 1.0f;
    float unorm8Scale = float(1 << 8) - 1.0f;

	float4 inPos = float4((float3(In.position.xyz) / unormPositionScale) * posScale, 1.0f) + posOffset;
	inPos += centerOffset;
	float3 inNormal = float3(In.normal.xyz) / snormNormalScale;
	float4 worldPosition = mul(world, inPos);
	worldPosition.xyz /= posScale;
	Out.position = mul(projView, worldPosition);
	
	Out.pos = worldPosition.xyz;
	Out.normal = normalize(mul((float3x3)InvTranspose, inNormal).xyz);
	
	Out.texCoord = float2(In.texCoord.xy) / unormTexScale * uvScale + uvOffset;

	Out.baseColor = In.baseColor / unorm8Scale;

	Out.metallicRoughness = In.metallicRoughness / unorm16Scale;
	Out.alphaSettings = float2(In.alphaSettings);
	Out.alphaSettings[0] = Out.alphaSettings[0] + 0.5f;
	Out.alphaSettings[1] = Out.alphaSettings[1] / unorm16Scale;

	return Out;
}
