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
#include <metal_compute>
#include <metal_atomic>
using namespace metal;

#include "shader_defs.h"

/*
layout(std430, set = 0, binding = 0) restrict buffer indirectDrawArgsBufferAlphaBlock
{
	uint data[];
} indirectDrawArgsBufferAlpha[NUM_CULLING_VIEWPORTS];

layout(std430, set = 0, binding = NUM_CULLING_VIEWPORTS) restrict buffer indirectDrawArgsBufferNoAlphaBlock
{
	uint data[];
} indirectDrawArgsBufferNoAlpha[NUM_CULLING_VIEWPORTS];

layout(std430, set = 0, binding = NUM_CULLING_VIEWPORTS * 2) restrict readonly buffer uncompactedDrawArgsBlock
{
	UncompactedDrawArguments data[];
} uncompactedDrawArgs[NUM_CULLING_VIEWPORTS];

layout(std430, set = 0, binding = NUM_CULLING_VIEWPORTS * 2 + 1) restrict writeonly buffer indirectMaterialBuffer
{
	uint IndirectMaterialBufferData[];
};

layout(std430, set = 0, binding = NUM_CULLING_VIEWPORTS * 2 + 2) restrict readonly buffer materialProps
{
	uint materialPropsData[];
};
*/

struct IndirectDrawArgsBufferAlphaData {
	device atomic_uint* data[NUM_CULLING_VIEWPORTS];
};

struct IndirectDrawArgsBufferNoAlphaData {
	device atomic_uint* data[NUM_CULLING_VIEWPORTS];
};

struct UncompactedDrawArgumentsData {
	device UncompactedDrawArguments* data[NUM_CULLING_VIEWPORTS];
};

//[numthreads(256, 1, 1)]
kernel void stageMain(
                      uint groupId                                                               [[thread_position_in_grid]],
                      constant uint* materialProps                                               [[buffer(UNIT_MATERIAL_PROPS)]],
                      device uint* indirectMaterialBuffer                                        [[buffer(UNIT_INDIRECT_MATERIAL_RW)]],
                      device IndirectDrawArgsBufferAlphaData& indirectDrawArgsBufferAlpha        [[buffer(UNIT_INDIRECT_DRAW_ARGS_ALPHA_RW)]],
                      device IndirectDrawArgsBufferNoAlphaData& indirectDrawArgsBufferNoAlpha    [[buffer(UNIT_INDIRECT_DRAW_ARGS_RW)]],
                      device UncompactedDrawArgumentsData& uncompactedDrawArgs                   [[buffer(UNIT_UNCOMPACTED_ARGS)]]
)
{
	if (groupId >= MAX_DRAWS_INDIRECT - 1)
		return;

	uint numIndices[NUM_CULLING_VIEWPORTS];
	uint sum = 0;
	for (uint i = 0; i < NUM_CULLING_VIEWPORTS; ++i)
	{
		numIndices[i] = uncompactedDrawArgs.data[i][groupId].numIndices;
		sum += numIndices[i];
	}

	if (sum == 0)
		return;
	
	uint slot = 0;
	for (uint i = 0; i < NUM_CULLING_VIEWPORTS; ++i)
	{
		if (numIndices[i] > 0)
		{
			uint matID = uncompactedDrawArgs.data[i][groupId].materialID;
			bool hasAlpha = (materialProps[matID] == 1);
			uint baseMatSlot = BaseMaterialBuffer(hasAlpha, i);

			if (hasAlpha)
			{
				slot = atomic_fetch_add_explicit(&indirectDrawArgsBufferAlpha.data[i][DRAW_COUNTER_SLOT_POS], 1, memory_order_relaxed);
				atomic_store_explicit(&indirectDrawArgsBufferAlpha.data[i][slot * INDIRECT_DRAW_ARGUMENTS_STRUCT_NUM_ELEMENTS + 0], numIndices[i], memory_order_relaxed);
				atomic_store_explicit(&indirectDrawArgsBufferAlpha.data[i][slot * INDIRECT_DRAW_ARGUMENTS_STRUCT_NUM_ELEMENTS + 2], uncompactedDrawArgs.data[i][groupId].startIndex, memory_order_relaxed);
			}
			else
			{
				slot =  atomic_fetch_add_explicit(&indirectDrawArgsBufferNoAlpha.data[i][DRAW_COUNTER_SLOT_POS], 1, memory_order_relaxed);
				atomic_store_explicit(&indirectDrawArgsBufferNoAlpha.data[i][slot * INDIRECT_DRAW_ARGUMENTS_STRUCT_NUM_ELEMENTS + 0], numIndices[i], memory_order_relaxed);
				atomic_store_explicit(&indirectDrawArgsBufferNoAlpha.data[i][slot * INDIRECT_DRAW_ARGUMENTS_STRUCT_NUM_ELEMENTS + 2], uncompactedDrawArgs.data[i][groupId].startIndex, memory_order_relaxed);
			}
			indirectMaterialBuffer[baseMatSlot + slot] = matID;
		}
	}
}
