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

struct ObjectInfo
{
	float4x4 toWorld;
	float4x4 normalMat;
	uint matID;
};

cbuffer ObjectUniformBlock : register(b0)
{
	ObjectInfo	objectInfo[MAX_NUM_OBJECTS];
};

cbuffer DrawInfoRootConstant : register(b1)
{
	uint baseInstance = 0;
};

cbuffer CameraUniform : register(b12)
{
	float4x4 camViewProj;
	float4x4 camViewMat;
	float4 camClipInfo;
	float4 camPosition;
};

struct VSInput
{
	float4 Position : POSITION;
	float3 Normal : NORMAL;
	float2 UV : TEXCOORD0;
};

struct VSOutput
{
	float4 Position : SV_POSITION;
	float4 WorldPosition : POSITION0;
	float4 Normal : NORMAL0;
	float4 UV : TEXCOORD0;
	uint MatID : MAT_ID;
#if (PT_USE_REFRACTION + PT_USE_DIFFUSION) != 0
	float4 CSPosition : POSITION1;
#endif
#if PT_USE_REFRACTION != 0
	float4 CSNormal : NORMAL1;
#endif
};


VSOutput main(VSInput input, uint InstanceID : SV_InstanceID)
{
	VSOutput output;

	uint instanceID = InstanceID + baseInstance;
	output.UV = input.UV.xyyy;
	output.Normal = normalize(mul(objectInfo[instanceID].normalMat, float4(input.Normal.xyz, 0)));
	output.WorldPosition = mul(objectInfo[instanceID].toWorld, input.Position);
	output.Position = mul(camViewProj, output.WorldPosition);
	output.MatID = objectInfo[instanceID].matID;

#if (PT_USE_REFRACTION + PT_USE_DIFFUSION) != 0
	output.CSPosition = mul(camViewMat, output.WorldPosition);
#endif
#if PT_USE_REFRACTION != 0
	output.CSNormal  = mul(camViewMat, float4(output.Normal.xyz, 0));
	output.CSNormal.xyz = normalize(output.CSNormal.xyz);
#endif

	return output;
}
