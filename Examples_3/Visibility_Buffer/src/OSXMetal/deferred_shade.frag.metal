/*
 * Copyright (c) 2018 Confetti Interactive Inc.
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

// USERMACRO: SAMPLE_COUNT [1,2,4]
// USERMACRO: USE_AMBIENT_OCCLUSION [0,1]

// This shader loads gBuffer data and shades the pixel.

#include <metal_stdlib>
using namespace metal;

#include "shader_defs.h"
#include "shading.h"

struct VSOutput {
	float4 position [[position]];
    float2 screenPos;
    uint triangleId;
};

// Pixel shader
fragment float4 stageMain(VSOutput input                                       [[stage_in]],
                          uint32_t sampleID                                    [[sample_id]],
#if SAMPLE_COUNT > 1
                          texture2d_ms<float,access::read> gBufferColor        [[texture(0)]],
                          texture2d_ms<float,access::read> gBufferNormal       [[texture(1)]],
                          texture2d_ms<float,access::read> gBufferSpecular     [[texture(2)]],
                          texture2d_ms<float,access::read> gBufferSimulation   [[texture(3)]],
                          depth2d_ms<float,access::read> gBufferDepth          [[texture(4)]],
#else
                          texture2d<float,access::read> gBufferColor           [[texture(0)]],
                          texture2d<float,access::read> gBufferNormal          [[texture(1)]],
                          texture2d<float,access::read> gBufferSpecular        [[texture(2)]],
                          texture2d<float,access::read> gBufferSimulation      [[texture(3)]],
                          depth2d<float,access::read> gBufferDepth             [[texture(4)]],
#endif
                          texture2d<float,access::sample> aoTex                [[texture(5)]],
                          depth2d<float,access::sample> shadowMap              [[texture(6)]],
                          constant PerFrameConstants& uniforms                 [[buffer(7)]],
                          constant LightData* lights                           [[buffer(8)]],
                          sampler depthSampler                                 [[sampler(0)]])
{
    // Load gBuffer data from render target
    float depth = gBufferDepth.read(uint2(input.position.xy), sampleID);
    if (depth == 1.0f) discard_fragment();
    
    float4 colorData = gBufferColor.read(uint2(input.position.xy), sampleID);
    float4 normalData = gBufferNormal.read(uint2(input.position.xy), sampleID);
    float4 specularData = gBufferSpecular.read(uint2(input.position.xy), sampleID);
    float4 simulation = gBufferSimulation.read(uint2(input.position.xy), sampleID);
    
#if USE_AMBIENT_OCCLUSION
    float ao = aoTex.read(uint2(input.position.xy)).x;
#else
    float ao = 1.0f;
#endif
    bool twoSided = (normalData.w > 0.5);
     
    float3 normal = normalData.xyz * 2.0f - 1.0f;
    float4 position = uniforms.transform[VIEW_CAMERA].invVP * float4(input.screenPos, depth, 1.0f);
    float3 posWS = position.xyz / position.w;
    
    float4 posLS = uniforms.transform[VIEW_SHADOW].vp * position + simulation;
    float3 color = calculateIllumination(normal, float4(uniforms.camPos).xyz, uniforms.esmControl, float4(uniforms.lightDir).xyz, twoSided, posLS, posWS, shadowMap, colorData.xyz, specularData.xyz, ao, depthSampler);
    
    return float4(color,1);
}

