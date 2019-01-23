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
// Copyright (C) 2009 - 2016 Confetti Interactive Inc.
// All rights reserved.
//
// This source may not be distributed and/or modified without expressly written permission
// from Confetti Interactive Inc.
//
//--------------------------------------------------------------------------------------------

cbuffer rootConstant : register(b0)
{
    uint drawID;
}

cbuffer uniformBlock : register(b5)
{
    float4x4 viewProj;
}

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
    float4 albedo : Color;
};

struct AsteroidDynamic
{
	float4x4 transform;
    uint indexStart;
    uint indexEnd;
    uint padding[2];
};

struct AsteroidStatic
{

    float4 rotationAxis;
    float4 surfaceColor;
    float4 deepColor;

    float scale;
    float orbitSpeed;
    float rotationSpeed;

    uint textureID;
    uint vertexStart;
    uint padding[3];
};


RWStructuredBuffer<AsteroidStatic> asteroidsStatic : register(u1);
RWStructuredBuffer<AsteroidDynamic> asteroidsDynamic : register(u2);

PsIn main(VsIn In)
{
    PsIn result;

    AsteroidStatic asteroidStatic = asteroidsStatic[drawID];
    AsteroidDynamic asteroidDynamic = asteroidsDynamic[drawID];

    float4x4 worldMatrix = asteroidDynamic.transform;
    result.position = mul(viewProj, mul(worldMatrix, float4(In.position.xyz, 1.0f)));
    result.posModel = In.position.xyz;
    result.normal = mul((float3x3)worldMatrix, In.normal.xyz);

    float depth = saturate((length(In.position.xyz) - 0.5f) / 0.2);
    result.albedo.xyz = lerp(asteroidStatic.deepColor.xyz, asteroidStatic.surfaceColor.xyz, depth);
    result.albedo.w = (float)asteroidStatic.textureID;

    return result;
}
