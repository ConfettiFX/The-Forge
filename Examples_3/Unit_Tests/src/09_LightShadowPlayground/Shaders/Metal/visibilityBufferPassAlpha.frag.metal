/*
 * Copyright (c) 2018-2020 The Forge Interactive Inc.
 *
 * This file is part of TheForge
 * (see https://github.com/ConfettiFX/The-Forge).
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarsnding copyright ownership.  The ASF licenses this file
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

// This shader performs the Visibility Buffer pass: store draw / triangle IDs per pixel.

#include <metal_stdlib>
using namespace metal;

#include "Shader_Defs.h"
#include "vb_argument_buffers.h"

struct VSOutput {
	float4 position [[position]];
	float2 texCoord;
};

uint packVisBufData(bool opaque, uint drawId, uint triangleId)
{
    uint packed = ((drawId << 23) & 0x7F800000) | (triangleId & 0x007FFFFF);
    return (opaque ? packed : (1 << 31) | packed);
}

// Pixel shader for alpha tested geometry
fragment float4 stageMain(
    VSOutput input                               [[stage_in]],
    uint primitiveID                             [[primitive_id]],
    constant ArgData& fsData                     [[buffer(UPDATE_FREQ_NONE)]],
    constant ArgDataPerFrame& fsDataPerFrame     [[buffer(UPDATE_FREQ_PER_FRAME)]],
    constant uint& drawID                        [[buffer(UPDATE_FREQ_USER)]]
)
{
	uint matBaseSlot = BaseMaterialBuffer(true, VIEW_CAMERA);
	uint materialID = fsDataPerFrame.indirectMaterialBuffer[matBaseSlot + drawID];
	texture2d<float> diffuseMap = fsData.diffuseMaps[materialID];
	
    // Perform alpha testing: sample the texture and discard the fragment if alpha is under a threshold
    float4 texColor = diffuseMap.sample(fsData.nearClampSampler,input.texCoord);
    if (texColor.a < 0.5) discard_fragment();
    
    // Pack draw / triangle Id data into a 32-bit uint and store it in a RGBA8 texture
    return unpack_unorm4x8_to_float(packVisBufData(false, drawID, primitiveID));
}
