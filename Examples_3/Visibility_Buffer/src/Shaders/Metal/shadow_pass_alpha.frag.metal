/*
 * Copyright (c) 2018-2019 Confetti Interactive Inc.
 *
 * This file is part of TheForge
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
using namespace metal;

#include "shader_defs.h"

struct PackedVertexPosData {
    packed_float3 position;
};

struct PackedVertexTexcoord {
    packed_float2 texCoord;
};

struct VSOut {
    float4 position [[position]];
    float2 texCoord;
};

struct FSData {
    array<texture2d<float>,MATERIAL_BUFFER_SIZE> diffuseMaps;
    sampler textureFilter;
};

struct FSDataPerFrame {
    constant uint* indirectMaterialBuffer;
};

fragment void stageMain(VSOut input                                    [[stage_in]],
                        constant FSData& fsData                         [[buffer(UPDATE_FREQ_NONE)]],
                        constant FSDataPerFrame& fsDataPerFrame         [[buffer(UPDATE_FREQ_PER_FRAME)]],
                        constant uint& drawID                          [[buffer(20)]]
)
{
	uint matBaseSlot = BaseMaterialBuffer(true, VIEW_SHADOW);
	uint materialID = fsDataPerFrame.indirectMaterialBuffer[matBaseSlot + drawID];
	texture2d<float> diffuseMap = fsData.diffuseMaps[materialID];

    float4 texColor = diffuseMap.sample(fsData.textureFilter, input.texCoord, level(0));
    if(texColor.a < 0.5) discard_fragment();
}
