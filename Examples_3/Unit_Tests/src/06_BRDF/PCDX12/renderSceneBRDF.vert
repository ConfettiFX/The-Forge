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


struct Light
{
	float4 pos;
	float4 col;
	float radius;
	float intensity;
};

static const float PI = 3.14159265359;

cbuffer cbCamera : register(b0) {
	float4x4 projView;
	float3 camPos;
}

cbuffer cbObject : register(b1) {
	float4x4 worldMat;
	float roughness;
	float metalness;
}

cbuffer cbLights : register(b2) {
	int currAmountOflights;
	int pad0;
	int pad1;
	int pad2;
	Light lights[16];
}

struct VSInput
{
    float4 Position : POSITION;
    float4 Normal : NORMAL;
};

struct VSOutput {
	float4 Position : SV_POSITION;
	float3 pos : POSITION;
	float3 normal : NORMAL;
};

VSOutput main(VSInput input, uint InstanceID : SV_InstanceID)
{
    VSOutput result;
    float4x4 tempMat = mul(projView, worldMat);
    result.Position = mul(tempMat, input.Position);

	result.pos = float3(mul(worldMat, input.Position).rgb);
	result.normal = mul(worldMat, float4(input.Normal.rgb, 0.0f)).rgb;

    return result;
}
