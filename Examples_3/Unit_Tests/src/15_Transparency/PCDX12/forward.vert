/*
 * Copyright (c) 2018 Confetti Interactive Inc.
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
	float4		color;
	float4x4	toWorld;
};

cbuffer ObjectUniformBlock : register(b0)
{
	float4x4	viewProj;
	ObjectInfo	objectInfo[MAX_NUM_OBJECTS];
};

cbuffer DrawInfoRootConstant : register(b1)
{
	uint baseInstance = 0;
};

struct VSInput
{
	float4 Position : POSITION;
	float4 Normal : NORMAL;
};
struct VSOutput
{
	float4 Position : SV_POSITION;
	float4 WorldPosition : POSITION;
	float4 Color : COLOR;
	float4 Normal : NORMAL;
};


VSOutput main(VSInput input, uint InstanceID : SV_InstanceID)
{
	VSOutput output;

	uint instanceID = InstanceID + baseInstance;
	output.Normal = normalize(mul(objectInfo[instanceID].toWorld, float4(input.Normal.xyz, 0)));
	float4x4 mvp = mul(viewProj, objectInfo[instanceID].toWorld);
	output.Position = mul(mvp, input.Position);
	output.WorldPosition = mul(objectInfo[instanceID].toWorld, input.Position);
	output.Color = objectInfo[instanceID].color;

	return output;
}
