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

#version 460 core
#define EPSILON 1e-7f

layout(set = 0, binding = 0) uniform cbCamera
{
	mat4 CamVPMatrix;
	mat4 CamInvVPMatrix;
	vec3 CamPos;
};

layout(push_constant) uniform CapsuleRootConstantBuffer
{
	vec3 Center0;
	float Radius0;
	vec3 Center1;
	float Radius1;
} CapsuleRootConstant;

layout(location = 0) in vec3 Position;
layout(location = 1) in vec3 Normal;
layout(location = 2) in vec2 Uv;

vec4 NormalizeQuaternion(vec4 q)
{
	vec4 qq = q;
	float n = q.x*q.x + q.y*q.y + q.z*q.z + q.w*q.w;

	if (n < 1e-10f)
	{
		qq.w = 1;
		return qq;
	}

	qq *= 1.0f / sqrt(n);
	return qq;
}

vec4 QuatFromUnitVectors(vec3 u, vec3 v)
{
	float r = 1.f + dot(u, v);
	vec3 n;

	// if u and v are parallel
	if (r < 1e-7)
	{
		r = 0.0f;
		n = abs(u.x) > abs(u.z) ? vec3(-u.y, u.x, 0.f) : vec3(0.f, -u.z, u.y);
	}
	else
	{
		n = cross(u, v);
	}

	vec4 q = vec4(n.x, n.y, n.z, r);
	return NormalizeQuaternion(q);
}

vec3 RotateVec(vec4 q, vec3 v)
{
	vec3 uv, uuv;
	vec3 qvec = vec3(q.x, q.y, q.z);
	uv = cross(qvec, v);
	uuv = cross(qvec, uv);
	uv *= (2.0f * q.w);
	uuv *= 2.0f;

	return v + uv + uuv;
}

void main()
{
	vec3 pos = Position;

	// Calculate weight from vertex height. Center0 up - Center1 down
	float weight = clamp((pos.y + 1.0f) * 0.5f, 0.0f, 1.0f);	

	// Collapse the capsule to a sphere.
	pos.y -= weight * 2.0f - 1.0f;	

	// Rotate the sphere
	if (length(CapsuleRootConstant.Center0 - CapsuleRootConstant.Center1) > EPSILON)
	{
		vec3 dir = normalize(CapsuleRootConstant.Center0 - CapsuleRootConstant.Center1);
		vec4 quat = QuatFromUnitVectors(vec3(0.0f, 1.0f, 0.0f), dir);
		pos = RotateVec(quat, pos);
	}

	// Expand half spheres to match radius.
	pos *= CapsuleRootConstant.Radius0 * weight + CapsuleRootConstant.Radius1 * (1.0f - weight);
	// Expand sphere to a capsule.
	pos += CapsuleRootConstant.Center0 * weight + CapsuleRootConstant.Center1 * (1.0f - weight);

	gl_Position = CamVPMatrix * vec4(pos, 1.0f);
}
