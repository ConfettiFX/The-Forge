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

struct PsIn
{    
  float4 Position : SV_POSITION;
  float2 UV : TEXCOORD0;
	float3 Normal : NORMAL;
  float3 Pos : POSITION;
};

float4 main(PsIn input) : SV_TARGET
{
  float4 outColor;
  float4 color = float4(0.0, 0.0, 0.0, 0.0);

  float3 viewDir = normalize(CameraPos.xyz - input.Pos);
  float fresnel = pow(max(dot(viewDir, input.Normal), 0.0f), 5.0f);
  outColor = (fresnel) * float4(1.0, 0.9, 0.65, 1.0);
  outColor.rgb *= 3.0f;
  return outColor;
}