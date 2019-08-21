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

#include "Packing.h"

cbuffer objectUniformBlock : register(b0)
{
    float4x4 WorldViewProjMat;
    float4x4 WorldMat;
};

struct VsIn
{
	float3 Position : POSITION;
	uint TexCoord : TEXCOORD;
};

struct PsIn
{
	float4 Position : SV_Position;
	float2 TexCoord : TEXCOORD0;
};


PsIn main(VsIn In)
{
	PsIn Out;
	Out.Position = mul(WorldViewProjMat, float4(In.Position.xyz, 1.0));
	Out.TexCoord = unpack2Floats(In.TexCoord);
	return Out;
}






