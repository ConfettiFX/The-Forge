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

Texture2D SparseTexture		: register(t0);
SamplerState uSampler0		: register(s0);
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

RWStructuredBuffer<uint> VisibilityBuffer : register(u1, space1);

struct PsIn
{    
  float4 Position : SV_POSITION;
  float2 UV : TEXCOORD0;
	float3 Normal : NORMAL;
  float3 Pos : POSITION;
};

float4 main(PsIn input) : SV_TARGET
{
  float4 color = float4(0.0, 0.0, 0.0, 0.0);
  float4 outColor = float4(0.0, 0.0, 0.0, 0.0);

  uint minDim     = min(Width, Height);
  uint minPageDim = min(pageWidth, pageHeight);

  float maxLod = floor(log2(float(minDim)) - log2(float(minPageDim)));
  float LOD = SparseTexture.CalculateLevelOfDetailUnclamped(uSampler0, input.UV);
  LOD = clamp(LOD, 0.0f, maxLod);

  float minLod = floor(LOD);
	minLod = clamp(minLod, 0.0f, maxLod);

  float localMinLod = minLod;
  
  uint residencyCode = 0; 

  [loop]
  do
  {
    color = SparseTexture.Sample(uSampler0, input.UV, 0, min(localMinLod, maxLod), residencyCode);
    localMinLod += 1.0f;

    if(localMinLod > maxLod)
      break;

  }while(!CheckAccessFullyMapped(residencyCode));

  uint pageOffset = 0;

  uint pageCountWidth = Width / pageWidth;
  uint pageCountHeight = Height / pageHeight;

  for(int i = 1; i <= int(minLod); i++)
  {
    pageOffset += pageCountWidth * pageCountHeight;

    pageCountWidth = pageCountWidth / 2;
    pageCountHeight = pageCountHeight /2;
  }

  float debugLineWidth = 0.025 / (minLod + 1.0f);

  pageOffset += uint(float(pageCountWidth) * input.UV.x) + uint(float(pageCountHeight) * input.UV.y) * pageCountWidth;

  float NoL = max(dot(input.Normal, -normalize(input.Pos.xyz)), 0.05f);  
  
  float4 debugColor;

  if(DebugMode == 1)
  {
    float4 resultColor = float4(0.0, 0.0, 0.0, 0.0);

    float2 modVal = frac(input.UV * float2(float(pageCountWidth), float(pageCountHeight))) + float2(debugLineWidth, debugLineWidth);

    if(modVal.x < debugLineWidth * 2.0f || modVal.y < debugLineWidth * 2.0f )
    {
      outColor.rgb = lerp(float3(1.0, 0.0, 0.0), float3(0.0, 1.0, 0.0), LOD / maxLod);
      outColor.a = 1.0;
    }

    outColor = lerp(float4(color.rgb * NoL, color.a), float4(outColor.rgb, color.a), outColor.a);
  }  
  else
  {
    outColor = float4(color.rgb * NoL, color.a);
  }

  VisibilityBuffer[pageOffset] = 1; 
  return outColor;
}