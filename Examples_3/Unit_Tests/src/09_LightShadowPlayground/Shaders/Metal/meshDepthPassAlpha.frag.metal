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

#include "Shader_Defs.h"

struct PackedVertexPosData {
    packed_float3 position;
};

struct PackedVertexTexcoord {
    packed_float2 texCoord;
};

struct PsIn
{
	float4 Position [[position]];
	float2 TexCoord;
};

struct BindlessDiffuseData
{
	array<texture2d<float>,MATERIAL_BUFFER_SIZE> textures;
};

fragment void stageMain(PsIn input                                    [[stage_in]],
                        constant uint* indirectMaterialBuffer          [[buffer(0)]],
                        constant BindlessDiffuseData& diffuseMaps      [[buffer(1)]],
                        sampler textureFilter                          [[sampler(0)]],
                        constant uint& drawID                          [[buffer(20)]])
{
	uint matBaseSlot = BaseMaterialBuffer(true, VIEW_SHADOW);
	uint materialID = indirectMaterialBuffer[matBaseSlot + drawID];
	texture2d<float> diffuseMap = diffuseMaps.textures[materialID];

    float4 texColor = diffuseMap.sample(textureFilter, input.TexCoord, level(0));
    if(texColor.a < 0.5) discard_fragment();
}
