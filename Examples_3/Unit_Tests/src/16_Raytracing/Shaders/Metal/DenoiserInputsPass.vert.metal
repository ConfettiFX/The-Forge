	/*
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
#include <metal_atomic>
using namespace metal;

// Include FSL metal to have access to defines: UPDATE_FREQ_*
// This is copied to the destination folder in build script
#include "metal.h"

struct VertexOutput
{
	float4 position [[position]];
    float4 previousPosition;
	float3 viewSpacePosition;
	float3 worldSpaceNormal;
};

struct Uniforms
{
	float4x4 worldToCamera;
	float4x4 cameraToProjection;
	float4x4 worldToProjectionPrevious;
	float2 rtInvSize;
	uint frameIndex;
	uint padding;
};

struct UniformDataPerFrame
{
    constant Uniforms &uniforms;
};

struct VertexInput
{
	float3 position [[attribute(0)]];
	float3 normal [[attribute(1)]];
};

vertex VertexOutput stageMain(
    uint vid [[vertex_id]],
    VertexInput input [[stage_in]],
    constant UniformDataPerFrame& uniformDataPerFrame [[buffer(UPDATE_FREQ_PER_FRAME)]]
)
{
	float3 position = input.position;
	float3 normal = input.normal;
	
	VertexOutput output;
	output.viewSpacePosition = (uniformDataPerFrame.uniforms.worldToCamera * float4(position, 1)).xyz;
	output.worldSpaceNormal = normal;
	output.position = uniformDataPerFrame.uniforms.cameraToProjection * float4(output.viewSpacePosition, 1);
	output.previousPosition = uniformDataPerFrame.uniforms.worldToProjectionPrevious * float4(position, 1);
	
	return output;
}
