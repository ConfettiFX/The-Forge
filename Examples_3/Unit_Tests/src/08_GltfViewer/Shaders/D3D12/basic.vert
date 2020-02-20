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
	// uint4 baseColor         : COLOR;
	// uint2 metallicRoughness : TEXCOORD1;
	// uint2 alphaSettings     : TEXCOORD2;
};

cbuffer cbPerPass : register(b0, UPDATE_FREQ_PER_FRAME)
{
	float4x4	projView;
	float4x4	shadowLightViewProj;
	float4      camPos;
	float4      lightColor[4];
	float4      lightDirection[3];
}

cbuffer cbRootConstants {
	uint nodeIndex;
};

StructuredBuffer<float4x4> modelToWorldMatrices : register(t0, UPDATE_FREQ_NONE);

struct PsIn
{    
    float3 pos               : POSITION;
	float3 normal	         : NORMAL;
	float2 texCoord          : TEXCOORD0;
    float4 position          : SV_Position;
};

PsIn main(VsIn In)
{
	float4x4 modelToWorld = modelToWorldMatrices[nodeIndex];

	PsIn Out;
	float4 inPos = float4(In.position.xyz, 1.0f);
	float3 inNormal = mul(modelToWorld, float4(In.normal, 0)).xyz;
	float4 worldPosition = mul(modelToWorld, inPos);
	Out.position = mul(projView, worldPosition);
	
	Out.pos = worldPosition.xyz;
	Out.normal = normalize(inNormal);
	
	Out.texCoord = float2(In.texCoord.xy);

	return Out;
}
