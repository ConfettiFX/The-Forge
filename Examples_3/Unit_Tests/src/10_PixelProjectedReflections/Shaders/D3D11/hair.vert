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

#define EPSILON 1e-7f

const bool ExpandPixels = true;

StructuredBuffer<float4> GuideHairVertexPositions : register(t0);
StructuredBuffer<float4> GuideHairVertexTangents : register(t1);
StructuredBuffer<float2> HairStrandUV : register(t2);
texture2D<float3> HairColorTexture : register(t3);
SamplerState LinearSampler : register(s0);

cbuffer cbCamera : register(b0)
{
	float4x4 CamVPMatrix;
	float4x4 CamInvVPMatrix;
	float3 CamPos;
}

cbuffer cbObject : register(b1)
{
	float4x4 ModelMat;
	float Roughness;
	float Metallic;
	int ObjectID;
}

cbuffer cbHair : register(b2)
{
	float4 Viewport;
	uint BaseColor;
	float Kd;
	float Ks1;
	float Ex1;
	float Ks2;
	float Ex2;
	float FiberRadius;
	uint NumVerticesPerStrand;
}

struct VSOutput 
{
	float4 Position : SV_POSITION;
	float4 Tangent : TANGENT;
	float4 P0P1 : POINT;
	float4 StrandColor : COLOR;
};

float4 GetStrandColor(int index)
{
	/*float2 uv = HairStrandUV[(uint)index / (uint)NumVerticesPerStrand];
	float3 color = HairColorTexture.SampleLevel(LinearSampler, uv, 0).xyz;
	return color;*/

	return float4(BaseColor >> 24, (BaseColor >> 16) & 0xFF, (BaseColor >> 8) & 0xFF, BaseColor & 0xFF) * (1.0f / 255.0f);
}

VSOutput main(uint vertexID : SV_VertexID)
{
	uint index = vertexID / 2;

	float3 v = GuideHairVertexPositions[index].xyz;
	float3 t = GuideHairVertexTangents[index].xyz;

	v = mul(ModelMat, float4(v, 1.0f)).xyz;
	t = normalize(mul(ModelMat, float4(t, 0.0f)).xyz);

	float3 right = normalize(cross(t, normalize(v - CamPos)));
	float2 projRight = normalize(mul(CamVPMatrix, float4(right, 0)).xy);

	float expandPixels = ExpandPixels ? 0.0f : 0.71f;

	float4 hairEdgePositions[2];
	hairEdgePositions[0] = float4(v + -right * FiberRadius, 1.0f);
	hairEdgePositions[1] = float4(v + right * FiberRadius, 1.0f);
	hairEdgePositions[0] = mul(CamVPMatrix, hairEdgePositions[0]);
	hairEdgePositions[1] = mul(CamVPMatrix, hairEdgePositions[1]);

	float dir = (vertexID & 1) ? -1.0f : 1.0f;

	VSOutput output = (VSOutput)0;
	output.Position = hairEdgePositions[vertexID & 1] + dir * float4(projRight * expandPixels / Viewport.w, 0.0f, 0.0f);
	output.Tangent = float4(t, 1.0f);
	output.P0P1 = float4(hairEdgePositions[0].xy / max(hairEdgePositions[0].w, EPSILON), hairEdgePositions[1].xy / max(hairEdgePositions[1].w, EPSILON));
	output.StrandColor = GetStrandColor(index);
	return output;
}
