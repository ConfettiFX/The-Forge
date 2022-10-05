/*
 * Copyright (c) 2017-2022 The Forge Interactive Inc.
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

#ifndef ShaderTypes_h
#define ShaderTypes_h

#include <simd/simd.h>

// Include FSL metal to have access to defines: UPDATE_FREQ_*
// This is copied to the destination folder in build script
#include "metal.h"

#define THREADS_PER_THREADGROUP 64
#define SQRT_THREADS_PER_THREADGROUP 8

struct Uniforms
{
    unsigned int width;
    unsigned int height;
    unsigned int blocksWide;
	unsigned int maxCallStackDepth;
};

// Represents a three dimensional ray which will be intersected with the scene. The ray type
// is customized using properties of the MPSRayIntersector.
struct Ray {
    // Starting point
    packed_float3 origin;
    
    // Mask which will be bitwise AND-ed with per-triangle masks to filter out certain
    // intersections. This is used to make the light source visible to the camera but not
    // to shadow or secondary rays.
    uint mask;
    
    // Direction the ray is traveling
    packed_float3 direction;
    
    // Maximum intersection distance to accept. This is used to prevent shadow rays from
    // overshooting the light source when checking for visibility.
    float maxDistance;
};

// Represents an intersection between a ray and the scene, returned by the MPSRayIntersector.
// The intersection type is customized using properties of the MPSRayIntersector.
// MPSIntersectionDataTypeDistancePrimitiveIndexInstanceIndexCoordinates
struct Intersection {
    // The distance from the ray origin to the intersection point. Negative if the ray did not
    // intersect the scene.
    float distance;
    
    // The index of the intersected primitive (triangle), if any. Undefined if the ray did not
    // intersect the scene.
    unsigned primitiveIndex;
    
    unsigned instanceIndex;
    
    // The barycentric coordinates of the intersection point, if any. Undefined if the ray did
    // not intersect the scene.
    float2 coordinates;
};

struct Payload
{
	packed_float3 throughput;
	uint intersectionIndex;
	packed_float3 radiance;
	float randomSeed;
	packed_float3 lightSample;
	uint recursionDepth;
	float3 indirectRayOrigin;
	float3 indirectRayDirection;
#if DENOISER_ENABLED
	float3 surfaceAlbedo;
#endif
};

struct RayGenConfigBlock
{
    float4x4 mCameraToWorld;
	float2 mZ1PlaneSize;
	float mProjNear;
	float mProjFarMinusNear;
    packed_float3 mLightDirection;
	float mRandomSeed;
	float2 mSubpixelJitter;
	uint mFrameIndex;
	uint mFramesSinceCameraMove;
};

#define TOTAL_IMGS 84

struct CSData
{
#ifndef TARGET_IOS
    texture2d<float, access::read_write> gOutput;
    texture2d<float, access::read_write> gAlbedoTex;
#endif
	
	const device uint* indices;
	const device packed_float3* positions;
	const device packed_float3* normals;
	const device float2* uvs;
	
	const device uint* materialIndices;
	constant uint* materialTextureIndices;
	array<texture2d<float, access::sample>, TOTAL_IMGS> materialTextures;
};
	
struct CSDataPerFrame
{
	constant RayGenConfigBlock & gSettings;
};

#define THREAD_STACK_SIZE 128

struct PathCallStackHeader {
	short missShaderIndex; // -1 if we're not generating any rays, and < 0 if there's no miss shader.
	uchar nextFunctionIndex;
//	uchar rayContributionToHitGroupIndex : 4;
//	uchar multiplierForGeometryContributionToHitGroupIndex : 4;
	uchar shaderIndexFactors; // where the lower four bits are rayContributionToHitGroupIndex and the upper four are multiplierForGeometryContributionToHitGroupIndex.
};

struct PathCallStack {
	device PathCallStackHeader& header;
	device ushort* functions;
	uint maxCallStackDepth;
	
	PathCallStack(device PathCallStackHeader* headers, device ushort* functions, uint pathIndex, uint maxCallStackDepth) :
		header(headers[pathIndex]), functions(&functions[pathIndex * maxCallStackDepth]), maxCallStackDepth(maxCallStackDepth) {
	}
	
	void Initialize() thread {
		header.nextFunctionIndex = 0;
		ResetRayParams();
	}
	
	void ResetRayParams() thread {
		header.missShaderIndex = -1;
		header.shaderIndexFactors = 0;
	}
	
	uchar GetRayContributionToHitGroupIndex() thread const {
		return header.shaderIndexFactors & 0b1111;
	}
	
	void SetRayContributionToHitGroupIndex(uchar contribution) thread {
		header.shaderIndexFactors &= ~0b1111;
		header.shaderIndexFactors |= contribution & 0b1111;
	}
	
	uchar GetMultiplierForGeometryContributionToHitGroupIndex() thread const {
		return header.shaderIndexFactors >> 4;
	}
	
	void SetMultiplierForGeometryContributionToHitGroupIndex(uchar multiplier) thread {
		header.shaderIndexFactors &= ~(0b1111 << 4);
		header.shaderIndexFactors |= multiplier << 4;
	}

	ushort GetMissShaderIndex() const thread {
		return header.missShaderIndex;
	}

	void SetMissShaderIndex(ushort missShaderIndex) thread {
		header.missShaderIndex = (short)missShaderIndex;
	}
	
	void SetHitShaderOnly() thread {
		header.missShaderIndex = SHRT_MIN;
	}
	
	void PushFunction(ushort functionIndex) thread {
		if (header.nextFunctionIndex >= maxCallStackDepth) {
			return;
		}
		functions[header.nextFunctionIndex] = functionIndex;
		header.nextFunctionIndex += 1;
	}
	
	ushort PopFunction() thread {
		if (header.nextFunctionIndex == 0) {
			return ~0;
		}
		header.nextFunctionIndex -= 1;
		return functions[header.nextFunctionIndex];
	}
};

struct RaytracingArguments {
	constant Uniforms &uniforms [[id(0)]];
	device Ray *rays  [[id(1)]];
	device Intersection *intersections [[id(2)]];
	device PathCallStackHeader *pathCallStackHeaders [[id(3)]];
	device ushort *pathCallStackFunctions [[id(4)]];
	device Payload *payloads [[id(5)]];
};

#endif /* ShaderTypes_h */

