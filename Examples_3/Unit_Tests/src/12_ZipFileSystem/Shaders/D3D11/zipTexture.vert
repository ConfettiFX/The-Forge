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


struct VSInput
{
    float3 Position : POSITION;
    float3 Normal 	: NORMAL;
	float2 TexCoords:TEXCOORD;
};

struct VSOutput {
	float4 Position : SV_POSITION;
    float2 texcoords:TEXCOORD;
};

cbuffer uniformBlock : register(b0)
{
	float4x4 ProjectionViewMat;
    float4x4 ModelMatrixCapsule;
    float4x4 ModelMatrixCube;
};

VSOutput main (VSInput input)
{
	VSOutput result;
	
	float4x4 mvp = mul(ProjectionViewMat, ModelMatrixCube);
	result.Position = mul(mvp,float4(input.Position.xyz, 1.0f));
	result.texcoords = input.TexCoords;
	
	return result;
}
