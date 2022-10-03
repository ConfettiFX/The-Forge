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

struct Uniforms {
	float4x4 worldToCamera;
	float4x4 worldToCameraPrevious;
	float4x4 cameraToProjection;
	float2 rtInvSize;
	uint frameIndex;
	uint padding;
};

struct UniformDataPerFrame {
    constant Uniforms &uniforms;
};

struct FragmentOutput {
	float4 depthNormal [[color(0)]];
	float2 motionVector [[color(1)]];
};

fragment FragmentOutput stageMain(
    VertexOutput input [[stage_in]],
    constant UniformDataPerFrame& uniformDataPerFrame [[buffer(UPDATE_FREQ_PER_FRAME)]]
)
{
	float2 motionVector = 0.0f;
    
    // Compute motion vectors
    if (uniformDataPerFrame.uniforms.frameIndex > 0) {
        // Map current pixel location to 0..1
		float2 uv = input.position.xy * uniformDataPerFrame.uniforms.rtInvSize;
        
        // Unproject the position from the previous frame then transform it from
        // NDC space to 0..1
        float2 prevUV = input.previousPosition.xy / input.previousPosition.w * float2(0.5f, -0.5f) + 0.5f;
        
        // Then the motion vector is simply the difference between the two
		motionVector = uv - prevUV;
    }
	
	FragmentOutput output;
	output.depthNormal = float4(length(input.viewSpacePosition), normalize(input.worldSpaceNormal));
	output.motionVector = motionVector;
	return output;
}
