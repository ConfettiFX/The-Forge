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
#include <metal_simdgroup>
#include <metal_quadgroup>
using namespace metal;

struct Uniforms_SceneConstantBuffer
{
	float4x4 orthProjMatrix;
	float2 mousePosition;
	float2 resolution;
	float time;
	uint renderMode;
	uint laneSize;
	uint padding;
};

struct PSInput
{
	float4 position [[position]];
	float4 color;
};


float texPattern(float2 position)
{
	float scale = 0.13;
	float t = (sin((position.x * scale)) + cos((position.y * scale)));
	float c = smoothstep(0.0, 0.2, (t * t));
	return c;
};

fragment float4 stageMain(PSInput input [[stage_in]]
				,constant Uniforms_SceneConstantBuffer & SceneConstantBuffer [[buffer(1)]])
{
	uint simd_lane_id = 0;
	uint simd_lane_count = SceneConstantBuffer.laneSize / 2;
	
	//workaround to get current lane id?
	for(uint i = 0 ; i < simd_lane_count ; i++)
	{
		//get input.position.(x,y) from specific simd lane id
		//compare with current input
		//if equal then that should be our lane id
		//seems like this returns half the size of laneSize because fragments
		//run in quadgroups.
		float posX = simd_shuffle(input.position.x, i);
		float posY = simd_shuffle(input.position.y, i);
		if(posX == input.position.x && posY == input.position.y)
		{
			simd_lane_id = i;
			break;
		}
	}
	
	float4 outputColor;
	float texP = texPattern(input.position.xy);
	(outputColor = ((float4)(texP) * input.color));
	switch (SceneConstantBuffer.renderMode)
	{
		case 1:
		{
			break;
		}
		case 2:
		{
			outputColor = (float)simd_lane_id / ((float)simd_lane_count);
			break;
		}
		case 3:
		{
			//if current lane is is the first active lane id.
			if (simd_is_first())
			{
				outputColor = float4(1.0, 1.0, 1.0, 1.0);
			}
			break;
		}
		case 4:
		{
			//if current lane is is the first active lane id.
			if (simd_is_first())
			{
				(outputColor = float4(1.0, 1.0, 1.0, 1.0));
			}
			//if current lane id is the max active lane id.
			if(simd_lane_id == simd_max(simd_lane_id))
			{
				(outputColor = float4(1.0, 0.0, 0.0, 1.0));
			}
			break;
		}
		case 5:
		{
			simd_vote activeLaneMask = simd_ballot(true);
			simd_vote::vote_t voteValue(activeLaneMask);
			//when using simd_vote, vote_t is 64 bit (i.e: size_t) and does not work
			//with popcount without converting explicitely to integral type.
			//since its 64 bit we store high and low bits into uint2.
			uint2 voteValueUnsigned;
			voteValueUnsigned[0] = (uint)voteValue;
			voteValueUnsigned[1] = (size_t)voteValue >> 32;
			uint numActiveLanes = popcount(voteValueUnsigned.x) + popcount(voteValueUnsigned.y);
			
			float activeRatio = ((float)(numActiveLanes) / float(simd_lane_count));
			(outputColor = float4(activeRatio, activeRatio, activeRatio, 1.0));
			break;
		}
		case 6:
		{
			(outputColor = simd_broadcast_first(outputColor));
			break;
		}
		case 7:
		{
			simd_vote activeLaneMask = simd_ballot(true);
			size_t voteValue(activeLaneMask);
			//store low and high bit of 64 bit unsigned int.
			uint2 voteValueUnsigned;
			voteValueUnsigned[0] = (uint)voteValue;
			voteValueUnsigned[1] = (size_t)voteValue >> 32;
			uint numActiveLanes = popcount(voteValueUnsigned.x) + popcount(voteValueUnsigned.y);
			
			float4 avgColor = (simd_sum(outputColor) / (float(numActiveLanes)));
			(outputColor = avgColor);
			break;
		}
		case 8:
		{
			float4 basePos = simd_broadcast_first(input.position);
			float4 prefixSumPos = simd_prefix_exclusive_sum(input.position - basePos);
			//simd ballot works with iMac AMD
			//active_threads_mask causes internal compiler error with iMac AMD
			simd_vote activeLaneMask = simd_ballot(true);
			//simd_vote activeLaneMask = simd_active_threads_mask();
			simd_vote::vote_t voteValue(activeLaneMask);
			uint2 voteValueUnsigned;
			voteValueUnsigned[0] = (uint)voteValue;
			voteValueUnsigned[1] = (size_t)voteValue >> 32;
			uint numActiveLanes = popcount(voteValueUnsigned.x) + popcount(voteValueUnsigned.y);
			
			outputColor = prefixSumPos / float(numActiveLanes);
			break;
		}
		case 9:
		{
			//any repeated shuffle instruction causes shader compile error on iMac radeon 580x
			uint quadId = simd_lane_id % 4u;
			float dx = quad_shuffle(input.position.x, (quadId + 1) % 4) - input.position.x;
			float dy = quad_shuffle(input.position.y, (quadId + 2) % 4) - input.position.y;
			
			if (dx > 0.f && dy > 0.f)
			{
				(outputColor = float4(1, 0, 0, 1));
			}
			else if (dx < 0.f && dy >0.f)
			{
				(outputColor = float4(0, 1, 0, 1));
			}
			else if (dx > 0.f && dy < 0.f)
			{
				(outputColor = float4(0, 0, 1, 1));
			}
			else if (dx < 0.f && dy < 0.f)
			{
				(outputColor = float4(1, 1, 1, 1));
			}
			else
			{
				(outputColor = float4(0, 0, 0, 1));
			}
			break;
		}
		default:
		{
			break;
		}
	}
	
	return outputColor;
	
}
