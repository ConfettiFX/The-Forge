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

cbuffer cbCamera : register(b0)
{
	float4x4 CamVPMatrix;
	float4x4 CamInvVPMatrix;
	float3 CamPos;
}

cbuffer CapsuleRootConstant : register(b1)
{
	float3 Center0;
	float Radius0;
	float3 Center1;
	float Radius1;
}
struct VSInput
{
	float3 Position : POSITION;
	float3 Normal : NORMAL;
	float2 Uv : TEXCOORD0;
};

struct VSOutput 
{
	float4 Position : SV_POSITION;
};

float4 NormalizeQuaternion(float4 q)
{
	float4 qq = q;
	float n = q.x*q.x + q.y*q.y + q.z*q.z + q.w*q.w;

	if (n < 1e-10f)
	{
		qq.w = 1;
		return qq;
	}

	qq *= 1.0f / sqrt(n);
	return qq;
}

float4 QuatFromUnitVectors(float3 u, float3 v)
{
	float r = 1.f + dot(u, v);
	float3 n;

	// if u and v are parallel
	if (r < 1e-7)
	{
		r = 0.0f;
		n = abs(u.x) > abs(u.z) ? float3(-u.y, u.x, 0.f) : float3(0.f, -u.z, u.y);
	}
	else
	{
		n = cross(u, v);
	}

	float4 q = float4(n.x, n.y, n.z, r);
	return NormalizeQuaternion(q);
}

float3 RotateVec(float4 q, float3 v)
{
	float3 uv, uuv;
	float3 qvec = float3(q.x, q.y, q.z);
	uv = cross(qvec, v);
	uuv = cross(qvec, uv);
	uv *= (2.0f * q.w);
	uuv *= 2.0f;

	return v + uv + uuv;
}

VSOutput main(VSInput input)
{
	VSOutput output;

	float3 pos = input.Position;

	// Calculate weight from vertex height. Center0 up - Center1 down
	float weight = saturate((pos.y + 1.0f) * 0.5f);	

	// Collapse the capsule to a sphere.
	pos.y -= weight * 2.0f - 1.0f;	

	// Rotate the sphere
	if (length(Center0 - Center1) > EPSILON)
	{
		float3 dir = normalize(Center0 - Center1);
		float4 quat = QuatFromUnitVectors(float3(0.0f, 1.0f, 0.0f), dir);
		pos = RotateVec(quat, pos);
	}

	// Expand half spheres to match radius.
	pos *= Radius0 * weight + Radius1 * (1.0f - weight);
	// Expand sphere to a capsule.
	pos += Center0 * weight + Center1 * (1.0f - weight);

	output.Position = mul(CamVPMatrix, float4(pos, 1.0f));

	return output;
}
