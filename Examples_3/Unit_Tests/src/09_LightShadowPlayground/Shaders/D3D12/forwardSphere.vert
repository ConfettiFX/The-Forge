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

cbuffer objectUniformBlock : register(b0, space3)
{
    row_major float4x4 WorldViewProjMat[26] : packoffset(c0);
    row_major float4x4 WorldMat[26] : packoffset(c104);
};

struct VsIn
{
    float4 PositionIn : POSITION;
    float4 NormalIn : NORMAL;
    uint InstanceIndex : SV_InstanceID;
};

struct PsIn
{
    float3 WorldPosition : POSITION;
    float3 Color : TEXCOORD1;
    float3 NormalOut : NORMAL;
    int IfPlane : TEXCOORD2;
    float4 Position : SV_Position;
};

PsIn main(VsIn input)
{
	PsIn output;
	output.Position = mul(input.PositionIn, WorldViewProjMat[input.InstanceIndex]);
    output.WorldPosition = mul(input.PositionIn, WorldMat[input.InstanceIndex]).xyz;
    if (input.InstanceIndex == 25)
    {
        output.NormalOut = float3(0.0f, 1.0f, 0.0f);
        output.IfPlane = 1;
    }
    else
    {
        output.NormalOut = mul(normalize(input.NormalIn.xyz), float3x3(WorldMat[input.InstanceIndex][0].xyz, WorldMat[input.InstanceIndex][1].xyz, WorldMat[input.InstanceIndex][2].xyz));
        output.IfPlane = 0;
    }
    output.Color = 1.0f.xxx;

    return output;
}
