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
	float3 position          : POSITION;
	float3 normal            : NORMAL;
	float2 texCoord          : TEXCOORD0;
	float3 baseColor         : COLOR;
	float2 metallicRoughness : TEXCOORD1;
	float2 alphaSettings     : TEXCOORD2;
};

cbuffer cbPerPass : register(b0) 
{
	float4x4	projView;
	float4      camPos;
	float4      lightColor;
}

cbuffer cbPerProp : register(b1)
{
	float4x4	world;
	int         unlit;
	int         hasAlbedoMap;
	int         hasNormalMap;
	int         hasMetallicRoughnessMap;
	int         hasAOMap;
	int         hasEmissiveMap;
	float4		posOffset;
	float2		uvOffset;
	float2		uvScale;
	float2		padding00;
}

struct PsIn
{    
    float3 pos               : POSITION;
	float3 normal	         : NORMAL;
	float2 texCoord          : TEXCOORD0;
	float3 baseColor         : COLOR;
	float2 metallicRoughness : TEXCOORD1;
	float2 alphaSettings     : TEXCOORD2;
    float4 position          : SV_Position;
};

PsIn main(VsIn In)
{
	PsIn Out;

	Out.position = mul(projView, mul(world, float4(In.position.xyz, 1.0f)));
	
	Out.pos = mul(world, float4(In.position.xyz, 1.0f)).xyz;
	Out.normal = mul((float3x3)world, In.normal.xyz).xyz;

	Out.texCoord = In.texCoord;
	Out.baseColor = In.baseColor;
	Out.metallicRoughness = In.metallicRoughness;
	Out.alphaSettings = In.alphaSettings;
	Out.alphaSettings[0] = Out.alphaSettings[0] + 0.125f;

	return Out;
}
