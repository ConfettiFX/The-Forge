#version 450 core

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

#extension GL_ARB_sparse_texture2 : enable
#extension GL_ARB_sparse_texture_clamp : enable

layout (set = 0, binding=8) uniform texture2D  SparseTexture;

layout (set = 0, binding=7) uniform sampler   uSampler0;

layout (set = 0, binding=9) uniform SparseTextureInfo
{
  uint Width;
	uint Height;
	uint pageWidth;
	uint pageHeight;

	uint DebugMode;
	uint ID;
	uint pad1;
	uint pad2;
	
	vec4 CameraPos;
};

layout(std430, set = 1, binding = 1) restrict buffer VisibilityBuffer
{
 	 uint Page[];
};

layout(location = 0) in vec2 UV;
layout(location = 1) in vec3 Normal;
layout(location = 2) in vec3 Pos;

layout(location = 0) out vec4 outColor;

void main ()
{
  vec4 color = vec4(0.0);
  outColor = vec4(0.0, 0.0, 0.0, 0.0);

  uint minDim     = min(Width, Height);
  uint minPageDim = min(pageWidth, pageHeight);

  float maxLod = floor(log2(float(minDim)) - log2(float(minPageDim)));
  float LOD = textureQueryLod(sampler2D(SparseTexture, uSampler0), UV).y;
  LOD = clamp(LOD, 0.0f, maxLod);

  float minLod = floor(LOD);
	minLod = clamp(minLod, 0.0f, maxLod);

  float localMinLod = minLod;
  
  int residencyCode = 0;

  do
  {
    residencyCode = sparseTextureClampARB(sampler2D(SparseTexture, uSampler0), UV, min(localMinLod, maxLod), color);
    localMinLod += 1.0f;
  }while(!sparseTexelsResidentARB(residencyCode));


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

  pageOffset += uint(float(pageCountWidth) * UV.x) + uint(float(pageCountHeight) * UV.y) * pageCountWidth;

  float NoL = max(dot(Normal, -normalize(Pos.xyz)), 0.05f);  
  
  vec4 debugColor;

  if(DebugMode == 1)
  {
    vec4 resultColor = vec4(0.0, 0.0, 0.0, 0.0);

    vec2 modVal = fract(UV * vec2(float(pageCountWidth), float(pageCountHeight))) + vec2(debugLineWidth, debugLineWidth);

    if(modVal.x < debugLineWidth * 2.0f || modVal.y < debugLineWidth * 2.0f )
    {
      outColor.rgb = mix(vec3(1.0, 0.0, 0.0), vec3(0.0, 1.0, 0.0), LOD / maxLod);
      outColor.a = 1.0;
    }

    outColor = mix(vec4(color.rgb * NoL, color.a), vec4(outColor.rgb, color.a), outColor.a);
  }  
  else
  {
    outColor = vec4(color.rgb * NoL, color.a);
  }

  Page[pageOffset] = 1; 
}