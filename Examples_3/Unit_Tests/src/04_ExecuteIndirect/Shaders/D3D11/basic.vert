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

float linstep(float min, float max, float s)
{
    return saturate((s - min) / (max - min));
}

PsIn main(VsIn In)
{
    PsIn result;
    result.position = mul(instanceBuffer[index].mvp, float4(In.position.xyz, 1));
    result.posModel = In.position.xyz;
    result.normal = normalize(mul(instanceBuffer[index].normalMat, float4(In.normal.xyz, 0)).xyz);

    float depth = linstep(0.5f, 0.7f, length(In.position.xyz));
    result.albedo = lerp(instanceBuffer[index].deepColor.xyz, instanceBuffer[index].surfaceColor.xyz, depth);

    return result;
}
