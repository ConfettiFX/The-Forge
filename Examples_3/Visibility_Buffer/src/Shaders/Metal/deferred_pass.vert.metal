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

// This shader performs the Deferred rendering pass: store per pixel geometry data.

#include <metal_stdlib>
using namespace metal;

#include "shader_defs.h"
#include "packing.h"

struct VSInput
{
	float4 position [[attribute(0)]];
	uint texCoord [[attribute(1)]];
	uint normal   [[attribute(2)]];
	uint tangent  [[attribute(3)]];
};

struct VSOutput {
	float4 position [[position]];
    float2 texCoord;
    float3 normal;
    float3 tangent;
};

struct PerBatchUniforms {
    uint drawId;
    uint twoSided;  // possible values: 0/1
};

struct IndirectDrawArguments
{
    uint vertexCount;
    uint instanceCount;
    uint startVertex;
    uint startInstance;
};

struct VSData {
    constant PerFrameConstants& uniforms              [[buffer(4)]];
};

// Vertex shader
vertex VSOutput stageMain(
    VSInput input                                     [[stage_in]],
    constant PerFrameConstants& uniforms    [[buffer(UNIT_VBPASS_UNIFORMS)]]
)
{
	VSOutput Out;
	Out.position = uniforms.transform[VIEW_CAMERA].mvp * input.position;
	Out.texCoord = unpack2Floats(input.texCoord);
	Out.normal =  decodeDir(unpack_unorm2x16_to_float(input.normal));
	Out.tangent = decodeDir(unpack_unorm2x16_to_float(input.tangent));
	return Out;
}
