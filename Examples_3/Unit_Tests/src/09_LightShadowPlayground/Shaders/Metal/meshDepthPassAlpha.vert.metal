
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

struct PsIn {
	float4 Position [[position]];
	float2 TexCoord;
};

struct PerBatchUniforms {
	uint drawId;
	uint twoSided;
};

struct IndirectDrawArguments
{
	uint vertexCount;
	uint instanceCount;
	uint startVertex;
	uint startInstance;
};

struct VSInput
{
	float4 Position [[attribute(0)]];
	float2 TexCoord [[attribute(1)]];
};

struct Uniforms_objectUniformBlock
{
	float4x4 WorldViewProjMat;
	float4x4 WorldMat;
};

struct VSData {
    constant Uniforms_objectUniformBlock& objectUniformBlock             [[id(0)]];
};

vertex PsIn stageMain(
    VSInput input               [[stage_in]],
    constant VSData& vsData     [[buffer(UPDATE_FREQ_PER_DRAW)]]
)
{
	PsIn output;
	output.Position = vsData.objectUniformBlock.WorldViewProjMat * input.Position;
	output.TexCoord = input.TexCoord;
	return output;
}
