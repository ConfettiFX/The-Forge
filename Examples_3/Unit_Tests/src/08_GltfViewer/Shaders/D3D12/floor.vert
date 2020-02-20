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

cbuffer cbPerPass : register(b0, UPDATE_FREQ_PER_FRAME)
{
	float4x4	projView;
	float4x4	shadowLightViewProj;
	float4      camPos;
	float4      lightColor[4];
	float4      lightDirection[3];
}

struct VSInput
{
	float3 Position : POSITION;
	float2 TexCoord : TEXCOORD0;
};

struct VSOutput
{
	float4 Position : SV_POSITION;
	float3 WorldPos : POSITION;
    float2 TexCoord : TEXCOORD;
};

VSOutput main(VSInput input)
{
	VSOutput Out;

	float4 worldPos = float4(input.Position, 1.0f);
	worldPos.xyz *= 3.0;
	Out.Position = mul(projView, worldPos);
	Out.WorldPos = worldPos.xyz;
	Out.TexCoord = input.TexCoord;

	return Out;
}
