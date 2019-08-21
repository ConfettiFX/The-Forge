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

// This compute shader resets the vertex count property of the IndirectDrawArguments struct.
// This needs to be done before triangle filtering is executed to reset it to 0.

#include <metal_stdlib>
using namespace metal;

#include "Shader_Defs.h"

struct RootConstantData
{
    uint numBatches;
};

struct IndirectDrawArgsBufferAlphaBlock {
	device uint* data[NUM_CULLING_VIEWPORTS];
};

struct IndirectDrawArgsBufferNoAlphaBlock {
	device uint* data[NUM_CULLING_VIEWPORTS];
};

struct uncompactedDrawArgsBlock {
	device UncompactedDrawArguments* data[NUM_CULLING_VIEWPORTS];
};

//[numthreads(256, 1, 1)]
kernel void stageMain(uint tid [[thread_position_in_grid]],
                      device IndirectDrawArgsBufferAlphaBlock& indirectDrawArgsBufferAlpha      [[buffer(UNIT_INDIRECT_DRAW_ARGS_ALPHA_RW)]],
                      device IndirectDrawArgsBufferNoAlphaBlock& indirectDrawArgsBufferNoAlpha  [[buffer(UNIT_INDIRECT_DRAW_ARGS_RW)]],
                      device uncompactedDrawArgsBlock& uncompactedDrawArgsRW                    [[buffer(UNIT_UNCOMPACTED_ARGS_RW)]]
)
{
	if (tid >= MAX_DRAWS_INDIRECT - 1)
		return;
	
	for (uint i = 0; i < NUM_CULLING_VIEWPORTS; ++i)
	{
		uncompactedDrawArgsRW.data[i][tid].mNumIndices = 0;
	}
	
	if (tid == 0)
	{
		for (uint i = 0; i < NUM_CULLING_VIEWPORTS; ++i)
		{
			indirectDrawArgsBufferAlpha.data[i][DRAW_COUNTER_SLOT_POS] = 0;
			indirectDrawArgsBufferNoAlpha.data[i][DRAW_COUNTER_SLOT_POS] = 0;
		}
	}
}

