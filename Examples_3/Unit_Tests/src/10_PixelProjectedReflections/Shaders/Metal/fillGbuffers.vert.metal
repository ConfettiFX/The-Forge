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

#include <metal_stdlib>
using namespace metal;

struct CameraData
{
    float4x4 projView;
    float3 camPos;
};

struct ObjectData
{
    float4x4 worldMat;
	float roughness;
	float metalness;
	int pbrMaterials;
};

struct VsIn
{
    float3 position [[attribute(0)]];
	float3 normal	[[attribute(1)]];
    float2 texCoord [[attribute(2)]];
};

struct PsIn
{
    float4 position [[position]];    
    float3 normal;
	float3 pos;
	float2 uv;
};

vertex PsIn stageMain(	VsIn	 In						[[stage_in]],
						constant CameraData& cbCamera	[[buffer(1)]],
						constant ObjectData& cbObject	[[buffer(2)]])
{
	PsIn Out;
	Out.position = cbCamera.projView * cbObject.worldMat * float4(In.position.xyz, 1.0f);
	Out.normal = normalize((cbObject.worldMat * float4(In.normal, 0.0f)).rgb);
	Out.pos = (cbObject.worldMat * float4(In.position.xyz, 1.0f)).rgb;
	Out.uv = In.texCoord;

	return Out;
}
