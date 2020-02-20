/*
* Copyright (c) 2018-2020 The Forge Interactive Inc.
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

cbuffer ShadowUniformBuffer : register(b2, UPDATE_FREQ_PER_FRAME)
{
    float4x4 ViewProjMat;
};

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

struct VSInput
{
	float3 Position : POSITION;
	float2 TexCoord : TEXCOORD0;
};

struct PsIn
{
    float4 Position : SV_Position;
};

PsIn main(VSInput input)
{
    PsIn output;
	float4 inPos = float4(float3(input.Position.xyz), 1.0f);
	output.Position = mul(ViewProjMat, mul(world, inPos));
    return output;
}
