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

#define MAX_PLANETS 20

struct VSInput
{
	float4 Position : POSITION;
	float4 Normal : NORMAL;
	float2 UV : TEXCOORD0;
};

struct VSOutput
{
  float4 Position : SV_POSITION;
	float2 outUV : TEXCOORD0;
	float3 outNormal : NORMAL;
  float3 outPos : POSITION;
};

cbuffer SparseTextureInfo : register(b0)
{
	uint Width;
	uint Height;
	uint pageWidth;
	uint pageHeight;

	uint DebugMode;
	uint ID;
	uint pad1;
	uint pad2;
	
	float4 CameraPos;
};

cbuffer uniformBlock : register(b0, space1)
{
	float4x4 mvp;
  float4x4 toWorld[MAX_PLANETS];
  float4 color[MAX_PLANETS];

  // Point Light Information
  float3 lightPosition;
  float3 lightColor;
};

VSOutput main(VSInput input)
{
  VSOutput output;

	float4x4 tempMat = mul(mvp, toWorld[ID]);
	output.Position = mul(tempMat, float4(input.Position.xyz, 1.0f));
	
	float4 normal = normalize(mul(toWorld[ID], float4(input.Normal.xyz, 0.0f)));
	float4 pos = mul(toWorld[ID], float4(input.Position.xyz, 1.0f));

  output.outUV = input.UV;
  output.outNormal = normal.xyz;
  output.outPos = pos.xyz;

  return output;
}
