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

/*
*Copyright (c) 2017 Advanced Micro Devices, Inc. All rights reserved.
*
*Permission is hereby granted, free of charge, to any person obtaining a copy
*of this software and associated documentation files (the "Software"), to deal
*in the Software without restriction, including without limitation the rights
*to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*copies of the Software, and to permit persons to whom the Software is
*furnished to do so, subject to the following conditions:
*
*The above copyright notice and this permission notice shall be included in
*all copies or substantial portions of the Software.
*
*THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
*AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
*THE SOFTWARE.
*/

#include <metal_stdlib>
#include <metal_compute>
using namespace metal;

#define THREAD_GROUP_SIZE 64

struct VertexIndices
{
	uint globalStrandIndex;
	uint localStrandIndex;
	uint globalVertexIndex;
	uint localVertexIndex;
	uint indexSharedMem;
};

struct StrandIndices
{
	uint globalStrandIndex;
	uint globalRootVertexIndex;
};

struct Capsule
{
	packed_float3 center0;
	float radius0;
	packed_float3 center1;
	float radius1;
};

struct SimulationData
{
	float4x4 Transform;
	float4 QuatRotation;
#if HAIR_MAX_CAPSULE_COUNT > 0
	Capsule Capsules[HAIR_MAX_CAPSULE_COUNT];
	uint mCapsuleCount;
#endif
	float Scale;
	uint NumStrandsPerThreadGroup;
	uint NumFollowHairsPerGuideHair;
	uint NumVerticesPerStrand;
	float Damping;
	float GlobalConstraintStiffness;
	float GlobalConstraintRange;
	float VSPStrength;
	float VSPAccelerationThreshold;
	float LocalStiffness;
	uint LocalConstraintIterations;
	uint LengthConstraintIterations;
	float TipSeperationFactor;
};

struct GlobalHairData
{
	float4 Viewport;
	float4 Gravity;
	float4 Wind;
	float TimeStep;
};

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

float4 InverseQuaternion(float4 q)
{
	float lengthSqr = q.x*q.x + q.y*q.y + q.z*q.z + q.w*q.w;

	if (lengthSqr < 0.001)
		return float4(0, 0, 0, 1.0f);

	q.x = -q.x / lengthSqr;
	q.y = -q.y / lengthSqr;
	q.z = -q.z / lengthSqr;
	q.w = q.w / lengthSqr;

	return q;
}

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

float4 MultQuaternionAndQuaternion(float4 qA, float4 qB)
{
	float4 q;

	q.w = qA.w * qB.w - qA.x * qB.x - qA.y * qB.y - qA.z * qB.z;
	q.x = qA.w * qB.x + qA.x * qB.w + qA.y * qB.z - qA.z * qB.y;
	q.y = qA.w * qB.y + qA.y * qB.w + qA.z * qB.x - qA.x * qB.z;
	q.z = qA.w * qB.z + qA.z * qB.w + qA.x * qB.y - qA.y * qB.x;

	return q;
}

float4 MakeQuaternion(float angle_radian, float3 axis)
{
	// create quaternion using angle and rotation axis
	float4 quaternion;
	float halfAngle = 0.5f * angle_radian;
	float sinHalf = sin(halfAngle);

	quaternion.w = cos(halfAngle);
	quaternion.xyz = sinHalf * axis.xyz;

	return quaternion;
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


VertexIndices CalculateVertexIndices(uint localID, uint groupID, constant SimulationData& cbSimulation)
{
	VertexIndices result;

	result.indexSharedMem = localID;

	result.localStrandIndex = localID % cbSimulation.NumStrandsPerThreadGroup;
	result.globalStrandIndex = groupID * cbSimulation.NumStrandsPerThreadGroup + result.localStrandIndex;
	result.globalStrandIndex *= cbSimulation.NumFollowHairsPerGuideHair + 1;
	result.localVertexIndex = (localID - result.localStrandIndex) / cbSimulation.NumStrandsPerThreadGroup;

	result.globalVertexIndex = result.globalStrandIndex * cbSimulation.NumVerticesPerStrand + result.localVertexIndex;

	return result;
}

StrandIndices CalculateStrandIndices(uint localID, uint groupID, constant SimulationData& cbSimulation)
{
	StrandIndices result;
	result.globalStrandIndex = THREAD_GROUP_SIZE * groupID + localID;
	result.globalStrandIndex *= cbSimulation.NumFollowHairsPerGuideHair + 1;
	result.globalRootVertexIndex = result.globalStrandIndex * cbSimulation.NumVerticesPerStrand;
	return result;
}

float4 Integrate(float4 currentPosition, float4 prevPosition, VertexIndices indices, constant GlobalHairData& cbHairGlobal, constant SimulationData& cbSimulation)
{
	float4 result = currentPosition;

	result.xyz += (1.0f - cbSimulation.Damping) * (currentPosition.xyz - prevPosition.xyz) + cbHairGlobal.Gravity.xyz * cbHairGlobal.TimeStep * cbHairGlobal.TimeStep;

	return result;
}

//[numthreads(64, 1, 1)]

#ifdef HAIR_INTEGRATE
kernel void stageMain(uint groupIndex[[thread_index_in_threadgroup]],
    uint3 groupID[[threadgroup_position_in_grid]],
    device float4* HairVertexPositions[[buffer(0)]],
    device float4* HairVertexPositionsPrev[[buffer(1)]],
    device float4* HairVertexPositionsPrevPrev[[buffer(2)]],
    device float4* HairRestPositions[[buffer(3)]],
    constant SimulationData& cbSimulation[[buffer(4)]],
    constant GlobalHairData& cbHairGlobal[[buffer(5)]])
{
    threadgroup float4 sharedPos[THREAD_GROUP_SIZE];
	VertexIndices indices = CalculateVertexIndices(groupIndex, groupID.x, cbSimulation);

	float4 restPosition = HairRestPositions[indices.globalVertexIndex];
	restPosition.xyz = (cbSimulation.Transform * float4(restPosition.xyz, 1.0f)).xyz;
	float4 currentPosition = sharedPos[indices.indexSharedMem] = HairVertexPositions[indices.globalVertexIndex];
	float weight = currentPosition.w;

    threadgroup_barrier(mem_flags::mem_threadgroup);

	// Integrate
	float4 prevPosition = HairVertexPositionsPrev[indices.globalVertexIndex];

	if (weight > 0.0f)	// Is movable
		sharedPos[indices.indexSharedMem] = Integrate(currentPosition, prevPosition, indices, cbHairGlobal, cbSimulation);
	else
		sharedPos[indices.indexSharedMem] = restPosition;

	// Global shape constraint
	if (cbSimulation.GlobalConstraintStiffness > 0.0f &&
		cbSimulation.GlobalConstraintRange > 0.0f &&
		sharedPos[indices.indexSharedMem].w > 0.0f && 
		(float)indices.localVertexIndex < cbSimulation.GlobalConstraintRange * (float)cbSimulation.NumVerticesPerStrand)
	{
		sharedPos[indices.indexSharedMem].xyz += cbSimulation.GlobalConstraintStiffness * (restPosition.xyz - sharedPos[indices.indexSharedMem].xyz);
	}

	HairVertexPositionsPrevPrev[indices.globalVertexIndex] = HairVertexPositionsPrev[indices.globalVertexIndex];
	HairVertexPositionsPrev[indices.globalVertexIndex] = currentPosition;
	HairVertexPositions[indices.globalVertexIndex] = sharedPos[indices.indexSharedMem];
}
#elif defined(HAIR_SHOCK_PROPAGATION)
kernel void stageMain(uint groupIndex[[thread_index_in_threadgroup]],
    uint3 groupID[[threadgroup_position_in_grid]],
    device float4* HairVertexPositions[[buffer(0)]],
    device float4* HairVertexPositionsPrev[[buffer(1)]],
    device float4* HairVertexPositionsPrevPrev[[buffer(2)]],
    constant SimulationData& cbSimulation[[buffer(4)]])
{
	StrandIndices indices = CalculateStrandIndices(groupIndex, groupID.x, cbSimulation);

	float4 rootPosPrevPrev;
	float4 rootPosPrev[2];
	float4 rootPos[2];

	rootPosPrevPrev = HairVertexPositionsPrevPrev[indices.globalRootVertexIndex + 1];

	rootPosPrev[0] = HairVertexPositionsPrev[indices.globalRootVertexIndex];
	rootPosPrev[1] = HairVertexPositionsPrev[indices.globalRootVertexIndex + 1];

	rootPos[0] = HairVertexPositions[indices.globalRootVertexIndex];
	rootPos[1] = HairVertexPositions[indices.globalRootVertexIndex + 1];

	float3 u = normalize(rootPosPrev[1].xyz - rootPosPrev[0].xyz);
	float3 v = normalize(rootPos[1].xyz - rootPos[0].xyz);

	float4 rotation = QuatFromUnitVectors(u, v);
	float3 RotateVecation = rootPos[0].xyz - RotateVec(rotation, rootPosPrev[0].xyz);

	float vspStrength = cbSimulation.VSPStrength;
	float acceleration = length((rootPos[1] - 2.0f * rootPosPrev[1] + rootPosPrevPrev[1]).xyz);

	if (acceleration > cbSimulation.VSPAccelerationThreshold)
		vspStrength = 1.0f;

	for(uint localVertexIndex = 2; localVertexIndex < cbSimulation.NumVerticesPerStrand; ++localVertexIndex)
	{
		uint globalVertexIndex = indices.globalRootVertexIndex + localVertexIndex;

		float4 pos = HairVertexPositions[globalVertexIndex];
		float4 posPrev = HairVertexPositionsPrev[globalVertexIndex];

		pos.xyz = (1.0f - vspStrength) * pos.xyz + vspStrength * (RotateVec(rotation, pos.xyz) + RotateVecation);
		posPrev.xyz = (1.0f - vspStrength) * posPrev.xyz + vspStrength * (RotateVec(rotation, posPrev.xyz) + RotateVecation);

		HairVertexPositions[globalVertexIndex] = pos;
		HairVertexPositionsPrev[globalVertexIndex] = posPrev;
	}
}
#elif defined(HAIR_LOCAL_CONSTRAINTS)

kernel void stageMain(uint groupIndex[[thread_index_in_threadgroup]],
    uint3 groupID[[threadgroup_position_in_grid]],
    device float4* HairVertexPositions[[buffer(0)]],
    device float4* HairGlobalRotations[[buffer(1)]],
    device float4* HairRefsInLocalFrame[[buffer(2)]],
    constant SimulationData& cbSimulation[[buffer(3)]])
{
	StrandIndices indices = CalculateStrandIndices(groupIndex, groupID.x, cbSimulation);

	float stiffness = cbSimulation.LocalStiffness;
	stiffness = 0.5f * min(stiffness, 0.95f);

	uint globalVertexIndex = 0;
	float4 position = HairVertexPositions[indices.globalRootVertexIndex + 1];
	float4 nextPosition;
	float4 globalRotation = HairGlobalRotations[indices.globalRootVertexIndex];
	float4 worldRotation;

	for (uint localVertexIndex = 1; localVertexIndex < cbSimulation.NumVerticesPerStrand - 1; ++localVertexIndex)
	{
		globalVertexIndex = indices.globalRootVertexIndex + localVertexIndex;
		nextPosition = HairVertexPositions[globalVertexIndex + 1];

		worldRotation = MultQuaternionAndQuaternion(cbSimulation.QuatRotation, globalRotation);

		float3 localNextPositionRest = HairRefsInLocalFrame[globalVertexIndex + 1].xyz * cbSimulation.Scale;
		float3 globalNextPosition = RotateVec(worldRotation, localNextPositionRest) + position.xyz;

		float3 weighted = stiffness * (globalNextPosition - nextPosition.xyz);

		if (position.w > 0.0f)
			position.xyz -= weighted;

		if (nextPosition.w > 0.0f)
			nextPosition.xyz += weighted;

		float4 invGlobalRotation = InverseQuaternion(worldRotation);
		float3 dir = normalize(nextPosition.xyz - position.xyz);

		float3 x = normalize(RotateVec(invGlobalRotation, dir));
		float3 e = float3(1.0f, 0.0f, 0.0f);
		float3 rotAxis = cross(e, x);

		if (length(rotAxis) > 0.001f)
		{
			float angle = acos(dot(e, x));
			rotAxis = normalize(rotAxis);
			float4 localRotation = MakeQuaternion(angle, rotAxis);
			globalRotation = MultQuaternionAndQuaternion(globalRotation, localRotation);
		}

		HairVertexPositions[globalVertexIndex] = position;
		HairVertexPositions[globalVertexIndex + 1] = nextPosition;

		position = nextPosition;
	} 
}

#elif defined(HAIR_LENGTH_CONSTRAINTS)
float2 ConstraintMultiplier(float4 particle0, float4 particle1)
{
	if (particle0.w > 0.0f)
	{
		if (particle1.w > 0.0f)
			return float2(0.5, 0.5);
		else
			return float2(1, 0);
	}
	else
	{
		if (particle1.w > 0.0f)
			return float2(0, 1);
		else
			return float2(0, 0);
	}
}

void ApplyDistanceConstraint(threadgroup float4& pos0, threadgroup float4& pos1, float targetDistance, float stiffness = 1.0f)
{
	float3 delta = pos1.xyz - pos0.xyz;
	float distance = max(length(delta), 1e-7);
	float stretching = 1 - targetDistance / distance;
	delta = stretching * delta;
	float2 multiplier = ConstraintMultiplier(pos0, pos1);

	pos0.xyz += multiplier[0] * delta * stiffness;
	pos1.xyz -= multiplier[1] * delta * stiffness;
}

bool CapsuleCollision(thread float3& newPosition, float3 prevPosition, constant Capsule& capsule)
{
	const float friction = 0.4f;

	float3 segment = capsule.center1 - capsule.center0;
	float3 delta0 = newPosition.xyz - capsule.center0;
	float3 delta1 = capsule.center1 - newPosition.xyz;

	float dist0 = dot(delta0, segment);
	float dist1 = dot(delta1, segment);

	// Collide sphere 0
	if (dist0 < 0.0f)
	{
		if (dot(delta0, delta0) < capsule.radius0 * capsule.radius0)
		{
			float3 n = normalize(delta0);
			newPosition = capsule.radius0 * n + capsule.center0;
			return true;
		}

		return false;
	}

	// Collide sphere 1
	if (dist1 < 0.0f)
	{
		if (dot(delta1, delta1) < capsule.radius1 * capsule.radius1)
		{
			float3 n = normalize(-delta1);
			newPosition = capsule.radius1 * n + capsule.center1;
			return true;
		}

		return false;
	}

	// Collide cylinder
	float3 x = (dist0 * capsule.center1 + dist1 * capsule.center0) / (dist0 + dist1);
	float3 delta = newPosition - x;
	float radius = (dist0 * capsule.radius1 + dist1 * capsule.radius0) / (dist0 + dist1);

	if (dot(delta, delta) < radius * radius)
	{
		float3 n = normalize(delta);
		float3 vec = newPosition - prevPosition;
		float3 nSeg = normalize(segment);
		float3 tangent = dot(vec, nSeg) * nSeg;
		float3 normal = vec - tangent;
		newPosition = prevPosition + friction * tangent + (normal + radius * n - delta);
		return true;
	}

	return false;
}

#if HAIR_MAX_CAPSULE_COUNT > 0
bool ResolveCapsuleCollisions(threadgroup float4& currentPosition, float4 prevPosition, constant SimulationData& cbSimulation)
{
	if (currentPosition.w == 0.0f)
		return false;

	bool anyCollided = false;

	float3 newPosition = currentPosition.xyz;

	for (uint i = 0; i < cbSimulation.mCapsuleCount; ++i)
	{
		bool collided = CapsuleCollision(newPosition, prevPosition.xyz, cbSimulation.Capsules[i]);

		if (collided)
			currentPosition.xyz = newPosition;

		anyCollided = anyCollided || collided;
	}

	return anyCollided;
}
#endif

kernel void stageMain(uint groupIndex[[thread_index_in_threadgroup]],
    uint3 groupID[[threadgroup_position_in_grid]],
    device float4* HairVertexPositions[[buffer(0)]],
    device float4* HairVertexPositionsPrev[[buffer(1)]],
    device float4* HairVertexTangents[[buffer(2)]],
    device float* HairRestLengths[[buffer(3)]],
    device float4* HairRefsInLocalFrame[[buffer(4)]],
    constant SimulationData& cbSimulation[[buffer(5)]],
    constant GlobalHairData& cbHairGlobal[[buffer(6)]])
{
    threadgroup float4 sharedPos[THREAD_GROUP_SIZE];
    threadgroup float sharedLength[THREAD_GROUP_SIZE];

	// Copy to shared memory
	VertexIndices indices = CalculateVertexIndices(groupIndex, groupID.x, cbSimulation);
	sharedPos[indices.indexSharedMem] = HairVertexPositions[indices.globalVertexIndex];
	sharedLength[indices.indexSharedMem] = HairRestLengths[indices.globalVertexIndex] * cbSimulation.Scale;

    threadgroup_barrier(mem_flags::mem_threadgroup);
	
	// Wind
	if (indices.localVertexIndex >= 2 && indices.localVertexIndex < cbSimulation.NumVerticesPerStrand - 1)
	{
		uint sharedIndex = indices.localVertexIndex * cbSimulation.NumStrandsPerThreadGroup + indices.localStrandIndex;

		float3 v = sharedPos[sharedIndex].xyz - sharedPos[sharedIndex + cbSimulation.NumStrandsPerThreadGroup].xyz;
		float3 force = -cross(cross(v, cbHairGlobal.Wind.xyz), v);
		sharedPos[sharedIndex].xyz += force * cbHairGlobal.TimeStep * cbHairGlobal.TimeStep;
	}

    threadgroup_barrier(mem_flags::mem_threadgroup);

	// Length constraint
	uint a = floor(cbSimulation.NumVerticesPerStrand * 0.5f);
	uint b = floor((cbSimulation.NumVerticesPerStrand - 1) * 0.5f);

	for(uint i = 0; i < cbSimulation.LengthConstraintIterations; ++i)
	{
		uint sharedIndex = 2 * indices.localVertexIndex * cbSimulation.NumStrandsPerThreadGroup + indices.localStrandIndex;

		if (indices.localVertexIndex < a)
			ApplyDistanceConstraint(sharedPos[sharedIndex], sharedPos[sharedIndex + cbSimulation.NumStrandsPerThreadGroup], sharedLength[sharedIndex]);

        threadgroup_barrier(mem_flags::mem_threadgroup);

		if (indices.localVertexIndex < b)
			ApplyDistanceConstraint(sharedPos[sharedIndex + cbSimulation.NumStrandsPerThreadGroup], sharedPos[sharedIndex + cbSimulation.NumStrandsPerThreadGroup * 2], sharedLength[sharedIndex + cbSimulation.NumStrandsPerThreadGroup]);

        threadgroup_barrier(mem_flags::mem_threadgroup);
	}

#if HAIR_MAX_CAPSULE_COUNT > 0
	// Capsule collisions
	bool collided = ResolveCapsuleCollisions(sharedPos[indices.indexSharedMem], HairVertexPositionsPrev[indices.globalVertexIndex], cbSimulation);
	if(collided)
		HairVertexPositionsPrev[indices.globalVertexIndex] = sharedPos[indices.indexSharedMem];
    threadgroup_barrier(mem_flags::mem_threadgroup);
#endif

	// Calculate tangents
	float3 tangent = sharedPos[indices.indexSharedMem + cbSimulation.NumStrandsPerThreadGroup].xyz - sharedPos[indices.indexSharedMem].xyz;
	HairVertexTangents[indices.globalVertexIndex].xyz = normalize(tangent);

	HairVertexPositions[indices.globalVertexIndex] = sharedPos[indices.indexSharedMem];
}

#elif defined(HAIR_UPDATE_FOLLOW_HAIRS)

kernel void stageMain(uint groupIndex[[thread_index_in_threadgroup]],
    uint3 groupID[[threadgroup_position_in_grid]],
    device float4* HairVertexPositions[[buffer(0)]],
    device float4* HairVertexTangents[[buffer(1)]],
    device float4* FollowHairRootOffsets[[buffer(2)]],
    constant SimulationData& cbSimulation[[buffer(3)]])
{
    threadgroup float4 sharedPos[THREAD_GROUP_SIZE];
    threadgroup float4 sharedTangent[THREAD_GROUP_SIZE];

	VertexIndices indices = CalculateVertexIndices(groupIndex, groupID.x, cbSimulation);

	sharedPos[indices.indexSharedMem] = HairVertexPositions[indices.globalVertexIndex];
	sharedTangent[indices.indexSharedMem] = HairVertexTangents[indices.globalVertexIndex];
	threadgroup_barrier(mem_flags::mem_threadgroup);

	for (uint i = 0; i < cbSimulation.NumFollowHairsPerGuideHair; ++i)
	{
		int globalFollowVertexIndex = indices.globalVertexIndex + cbSimulation.NumVerticesPerStrand * (i + 1);
		int globalFollowStrandIndex = indices.globalStrandIndex + i + 1;
		float factor = cbSimulation.TipSeperationFactor * (float(indices.localVertexIndex) / float(cbSimulation.NumVerticesPerStrand)) + 1.0f;
		float3 followPosition = sharedPos[indices.indexSharedMem].xyz + factor * FollowHairRootOffsets[globalFollowStrandIndex].xyz * cbSimulation.Scale;
		HairVertexPositions[globalFollowVertexIndex].xyz = followPosition;
		HairVertexTangents[globalFollowVertexIndex] = sharedTangent[indices.indexSharedMem];
	}
}

#elif defined(HAIR_PRE_WARM)

kernel void stageMain(uint groupIndex[[thread_index_in_threadgroup]],
    uint3 groupID[[threadgroup_position_in_grid]],
    device float4* HairVertexPositions[[buffer(0)]],
    device float4* HairVertexPositionsPrev[[buffer(1)]],
    device float4* HairVertexPositionsPrevPrev[[buffer(2)]],
    device float4* HairRestPositions[[buffer(3)]],
    constant SimulationData& cbSimulation[[buffer(4)]])
{
	VertexIndices indices = CalculateVertexIndices(groupIndex, groupID.x, cbSimulation);

	float4 restPosition = HairRestPositions[indices.globalVertexIndex];
	restPosition.xyz = (cbSimulation.Transform * float4(restPosition.xyz, 1.0f)).xyz;

	HairVertexPositionsPrevPrev[indices.globalVertexIndex] = restPosition;
	HairVertexPositionsPrev[indices.globalVertexIndex] = restPosition;
    HairVertexPositions[indices.globalVertexIndex] = restPosition;
}
#endif
