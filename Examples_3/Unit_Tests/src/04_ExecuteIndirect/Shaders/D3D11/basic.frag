/*
 * Copyright (c) 2018-2019 Confetti Interactive Inc.
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

//--------------------------------------------------------------------------------------------
//
// Copyright (C) 2009 - 2017 Confetti Interactive Inc.
// All rights reserved.
//
// This source may not be distributed and/or modified without expressly written permission
// from Confetti Interactive Inc.
//
//--------------------------------------------------------------------------------------------

struct InstanceData
{
    float4x4 mvp;
    float4x4 normalMat;
    float4 surfaceColor;
    float4 deepColor;
    int textureID;
	uint _pad0[3];
};

StructuredBuffer<InstanceData> instanceBuffer : register(t0);

cbuffer rootConstant : register(b1)
{
	uint index;
};

struct VsIn
{
    float4 position : Position;
    float4 normal : Normal;
};

struct PsIn
{
    float4 position : SV_Position;
    float3 posModel : PosModel;
    float3 normal : Normal;
    float3 albedo : Color;
};

Texture2DArray<float4> uTex0 : register(t1);
sampler uSampler0 : register(s2);

static const float3 lightDir = -normalize(float3(2,6,1));

float4 main(PsIn In) : SV_TARGET
{
    float wrap_diffuse = saturate(dot(lightDir, normalize(In.normal)));
    float light = 2.0f * wrap_diffuse + 0.06f;

    float3 uvw = In.posModel * 0.5 + 0.5;

    float3 blendWeights = abs(normalize(In.posModel));
    blendWeights = saturate((blendWeights - 0.2) * 7);
    blendWeights /= (blendWeights.x + blendWeights.y + blendWeights.z).xxx;

    float3 coord1 = float3(uvw.yz, (float)instanceBuffer[index].textureID * 3 + 0);
    float3 coord2 = float3(uvw.zx, (float)instanceBuffer[index].textureID * 3 + 1);
    float3 coord3 = float3(uvw.xy, (float)instanceBuffer[index].textureID * 3 + 2);

    float3 texColor = float3(0,0,0);
    texColor += blendWeights.x * uTex0.Sample(uSampler0, coord1).xyz;
    texColor += blendWeights.y * uTex0.Sample(uSampler0, coord2).xyz;
    texColor += blendWeights.z * uTex0.Sample(uSampler0, coord3).xyz;

	float coverage = saturate(In.position.z * 4000.0f);

    float3 color = In.albedo;
    color *= light;
    color *= texColor * 2;
	color *= coverage;

    return float4(color, 1);
}
