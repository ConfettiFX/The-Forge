/*
 * Copyright (c) 2018-2020 The Forge Interactive Inc.
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

#include "Shader_Defs.h"
#include "cull_argument_buffers.h"

//[numthreads(256, 1, 1)]
kernel void stageMain(
    uint groupId                              [[thread_position_in_grid]],
    constant CSData& csData                   [[buffer(UPDATE_FREQ_NONE)]],
    constant CSDataPerFrame& csDataPerFrame   [[buffer(UPDATE_FREQ_PER_FRAME)]]
)
{
	if (groupId >= MAX_DRAWS_INDIRECT - 1)
		return;

	uint numIndices[NUM_CULLING_VIEWPORTS];
	uint sum = 0;
	for (uint i = 0; i < NUM_CULLING_VIEWPORTS; ++i)
	{
		numIndices[i] = csDataPerFrame.uncompactedDrawArgs[i][groupId].mNumIndices;
		sum += numIndices[i];
	}

	if (sum == 0)
		return;
	
	uint slot = 0;
	for (uint i = 0; i < NUM_CULLING_VIEWPORTS; ++i)
	{
		if (numIndices[i] > 0)
		{
			uint matID = csDataPerFrame.uncompactedDrawArgs[i][groupId].mMaterialID;
			bool hasAlpha = (csData.materialProps[matID] == 1);
			uint baseMatSlot = BaseMaterialBuffer(hasAlpha, i);

			if (hasAlpha)
			{
				slot = atomic_fetch_add_explicit(&csDataPerFrame.indirectDrawArgsBufferAlpha[i][DRAW_COUNTER_SLOT_POS], 1, memory_order_relaxed);
				atomic_store_explicit(&csDataPerFrame.indirectDrawArgsBufferAlpha[i][slot * INDIRECT_DRAW_ARGUMENTS_STRUCT_NUM_ELEMENTS + 0], numIndices[i], memory_order_relaxed);
				atomic_store_explicit(&csDataPerFrame.indirectDrawArgsBufferAlpha[i][slot * INDIRECT_DRAW_ARGUMENTS_STRUCT_NUM_ELEMENTS + 2], csDataPerFrame.uncompactedDrawArgs[i][groupId].mStartIndex, memory_order_relaxed);
			}
			else
			{
				slot =  atomic_fetch_add_explicit(&csDataPerFrame.indirectDrawArgsBufferNoAlpha[i][DRAW_COUNTER_SLOT_POS], 1, memory_order_relaxed);
				atomic_store_explicit(&csDataPerFrame.indirectDrawArgsBufferNoAlpha[i][slot * INDIRECT_DRAW_ARGUMENTS_STRUCT_NUM_ELEMENTS + 0], numIndices[i], memory_order_relaxed);
				atomic_store_explicit(&csDataPerFrame.indirectDrawArgsBufferNoAlpha[i][slot * INDIRECT_DRAW_ARGUMENTS_STRUCT_NUM_ELEMENTS + 2], csDataPerFrame.uncompactedDrawArgs[i][groupId].mStartIndex, memory_order_relaxed);
			}
			csDataPerFrame.indirectMaterialBuffer[baseMatSlot + slot] = matID;
		}
	}
}
