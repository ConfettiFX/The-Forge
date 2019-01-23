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
#define PI 3.1415926289793f
#define PI_2 (3.1415926289793f*2.0f)

Texture2D SphereTex : register(t0);
Texture2D PlaneTex : register(t1);

SamplerState textureSampler : register (s0);
struct PSOut
{
    float4 albedo   : SV_Target0;
    float4 normal   : SV_Target1;
    float4 position : SV_Target2;
};
struct VSOutput {
	float4 Position : SV_POSITION;
    float4 WorldPosition : POSITION;
    float4 Color : COLOR;
    float4 Normal : NORMAL;
    int IfPlane : BLENDINDICES0;
};

PSOut main(VSOutput input)
{    
    PSOut Out = (PSOut)0;
    Out.albedo = input.Color;
    Out.normal = (input.Normal + 1) * 0.5f;
    if (input.IfPlane == 0){//sphere
        Out.albedo *= SphereTex.Sample(textureSampler, Out.normal);
    }
    else {//plane
        Out.albedo *= PlaneTex.Sample(textureSampler, input.WorldPosition.xz);
        Out.albedo = PlaneTex.Sample(textureSampler, input.WorldPosition.xz*5.f);
    }   
    Out.position = input.WorldPosition;
    return Out;
}