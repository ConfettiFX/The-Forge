/*
 * Copyright (c) 2018 Kostas Anagnostou (https://twitter.com/KostasAAA).
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
#include<metal_math>
using namespace metal;

#define BACKFACE_CULLING 0

#define USE_SHADOWS_AA 0

#define kEpsilon 0.00001

bool RayIntersectsBox(float3 origin, float3 rayDirInv, float3 BboxMin, float3 BboxMax)
{
	const float3 t0 = (BboxMin - origin) * rayDirInv;
	const float3 t1 = (BboxMax - origin) * rayDirInv;
	
	const float3 tmax = max(t0, t1);
	const float3 tmin = min(t0, t1);
	
	const float a1 = min(tmax.x, min(tmax.y, tmax.z));
	const float a0 = max(max(tmin.x, tmin.y), max(tmin.z, 0.0f));
	
	return a1 >= a0;
}

//Adapted from https://github.com/kayru/RayTracedShadows/blob/master/Source/Shaders/RayTracedShadows.comp
bool RayTriangleIntersect(
						  const float3 orig,
						  const float3 dir,
						  float3 v0,
						  float3 e0,
						  float3 e1,
						  thread float& t,
						  thread float2& bCoord)
{
	const float3 s1 = cross(dir.xyz, e1);
	const float  invd = 1.0 / (dot(s1, e0));
	const float3 d = orig.xyz - v0;
	bCoord.x = dot(d, s1) * invd;
	const float3 s2 = cross(d, e0);
	bCoord.y = dot(dir.xyz, s2) * invd;
	t = dot(e1, s2) * invd;
	
	if (
#if BACKFACE_CULLING
		dot(s1, e0) < -kEpsilon ||
#endif
		bCoord.x < 0.0 || bCoord.x > 1.0 || bCoord.y < 0.0 || (bCoord.x + bCoord.y) > 1.0 || t < 0.0 || t > 1e9)
	{
		return false;
	}
	else
	{
		return true;
	}
}

bool RayTriangleIntersect(
						  const float3 orig,
						  const float3 dir,
						  float3 v0,
						  float3 e0,
						  float3 e1)
{
	float t = 0;
	float2 bCoord = float2(0,0);
	return RayTriangleIntersect(orig, dir, v0, e0, e1, t, bCoord);
}

struct ConstantBuffer
{
	float4x4	projView;
	float4x4	invProjView;
	float4		rtSize;
	float4		lightDir; 
	float4		cameraPos;
};
     
//[numthreads(8, 8, 1)]
kernel void stageMain(uint3 DTid[[thread_position_in_grid]],
					  constant  ConstantBuffer & cbPerPass [[buffer(1)]],
					  constant  float4 * BVHTree [[buffer(2)]],
                      texture2d<float, access::read> depthBuffer[[texture(0)]],
                      texture2d<float, access::read> normalBuffer[[texture(1)]],
                      texture2d<float, access::read_write> outputRT[[texture(2)]])
{
	bool collision = false;
	int offsetToNextNode = 1;
	 
	float depth = depthBuffer.read(DTid.xy).x;
	float3 normal = normalBuffer.read(DTid.xy).xyz;
	float NdotL = dot(normal,cbPerPass.lightDir.xyz);

	if (depth < 1 && NdotL > 0)
	{
		float2 uv = float2(DTid.xy) * cbPerPass.rtSize.zw;

		//get world position from depth
		float4 clipPos = float4(2 * uv - 1, depth, 1);
		clipPos.y = -clipPos.y;

		float4 worldPos = cbPerPass.invProjView * clipPos;
		worldPos.xyz /= worldPos.w;

		float3 rayDir = cbPerPass.lightDir.xyz;
		float3 rayDirInv = 1 / rayDir;

		//offset to avoid selfshadows
		worldPos.xyz += 5 * normal;

		float t = 0;
		float2 bCoord = 0;

		int dataOffset = 0;

		while (offsetToNextNode != 0)
		{
			float4 element0 = BVHTree[dataOffset++].xyzw;
			float4 element1 = BVHTree[dataOffset++].xyzw;

			offsetToNextNode = int(element0.w);

			collision = false;

			if (offsetToNextNode < 0)
			{
				//try collision against this node's bounding box	
				float3 bboxMin = element0.xyz;
				float3 bboxMax = element1.xyz;

				//intermediate node check for intersection with bounding box
				collision = RayIntersectsBox(worldPos.xyz, rayDirInv, bboxMin.xyz, bboxMax.xyz);

				//if there is collision, go to the next node (left) or else skip over the whole branch
				if (!collision)
					dataOffset += abs(offsetToNextNode);
			}
			else if (offsetToNextNode > 0)
			{
				float4 element2 = BVHTree[dataOffset++].xyzw;

				float3 vertex0 = element0.xyz;
				float3 vertex1MinusVertex0 = element1.xyz;
				float3 vertex2MinusVertex0 = element2.xyz;

				//leaf node check for intersection with triangle
				collision = RayTriangleIntersect(worldPos.xyz, rayDir, vertex0.xyz, vertex1MinusVertex0.xyz, vertex2MinusVertex0.xyz, t, bCoord);

				if (collision)
				{
					break;
				}
			}
		};
	}
	float shadowFactor = 1.0f - float(collision);
	outputRT.write(float4(shadowFactor,0.0,0.0,0.0), uint2(DTid.xy) );
}
