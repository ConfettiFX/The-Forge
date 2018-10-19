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

#include <metal_stdlib>
using namespace metal;

#include "shader_defs.h"
#include "shading.h"

struct VSInput
{
    float4 position [[attribute(0)]];
};

struct VSOutput
{
    float4 position [[position]];
    float3 color;
    float3 lightPos;
};

fragment float4 stageMain(VSOutput input                                        [[stage_in]],
                          uint32_t sampleID                                     [[sample_id]],
                          constant PerFrameConstants& uniforms                  [[buffer(1)]],
#if SAMPLE_COUNT > 1
                          texture2d_ms<float,access::read> gBufferNormal        [[texture(0)]],
                          texture2d_ms<float,access::read> gBufferSpecular      [[texture(1)]],
                          texture2d_ms<float,access::read> gBufferSimulation    [[texture(2)]],
                          depth2d_ms<float,access::read> gBufferDepth           [[texture(3)]])
#else
                          texture2d<float,access::read> gBufferNormal           [[texture(0)]],
                          texture2d<float,access::read> gBufferSpecular         [[texture(1)]],
                          texture2d<float,access::read> gBufferSimulation       [[texture(2)]],
                          depth2d<float,access::read> gBufferDepth              [[texture(3)]])
#endif
{
    float4 normalData = gBufferNormal.read(uint2(input.position.xy), sampleID);
    if (normalData.x == 0 && normalData.y == 0 && normalData.z == 0) return float4(0);

    float depth = gBufferDepth.read(uint2(input.position.xy), sampleID);
    float4 specularData = gBufferSpecular.read(uint2(input.position.xy), sampleID);
    float4 simulation = gBufferSimulation.read(uint2(input.position.xy), sampleID);
    
    bool twoSided = (normalData.w > 0.5);
    
    float3 normal = normalData.xyz * 2.0 - 1.0;
    float2 screenPos = ((input.position.xy / uniforms.cullingViewports[VIEW_CAMERA].windowSize) * 2.0 - 1.0);
    screenPos.y = -screenPos.y;
    float4 position = uniforms.transform[VIEW_CAMERA].invVP * float4(screenPos,depth,1) + simulation;
    float3 posWS = position.xyz / position.w;
    
    float3 color = pointLightShade(input.lightPos, input.color, float4(uniforms.camPos).xyz, posWS, normal, specularData.xyz, twoSided);
    return float4(color,1);
}
