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

#include <metal_stdlib>
using namespace metal;

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

vertex VSOutput VSMain(VSInput input [[stage_in]],
                       constant PerFrameConstants& uniforms [[buffer(0)]],
                       constant LightData* lights [[buffer(1)]],
                       uint instanceId [[instance_id]])
{
    VSOutput output;
    output.lightPos = lights[instanceId].position;
    output.color = lights[instanceId].color;
    output.position = uniforms.transform[VIEW_CAMERA].mvp * float4((input.position.xyz * LIGHT_SIZE) + output.lightPos, 1);
    return output;
}

fragment float4 PSMain(VSOutput input                                   [[stage_in]],
                       uint32_t sampleID                                [[sample_id]],
                       constant PerFrameConstants& uniforms             [[buffer(0)]],
#if SAMPLE_COUNT > 1
                       texture2d_ms<float,access::read> gBufferNormal   [[texture(1)]],
                       texture2d_ms<float,access::read> gBufferSpecular [[texture(2)]],
                       depth2d_ms<float,access::read> gBufferDepth      [[texture(3)]])
#else
                       texture2d<float,access::read> gBufferNormal      [[texture(1)]],
                       texture2d<float,access::read> gBufferSpecular    [[texture(2)]],
                       depth2d<float,access::read> gBufferDepth         [[texture(3)]])
#endif
{
    float4 normalData = gBufferNormal.read(uint2(input.position.xy), sampleID);
    float4 specularData = gBufferSpecular.read(uint2(input.position.xy), sampleID);
    float depth = gBufferDepth.read(uint2(input.position.xy), sampleID);
    
    float2 screenPos = ((input.position.xy / uniforms.cullingViewports[VIEW_CAMERA].windowSize) * 2.0 - 1.0);
    screenPos.y = -screenPos.y;
    float3 normal = normalData.xyz*2.0 - 1.0;
    float4 position = uniforms.transform[VIEW_CAMERA].invVP * float4(screenPos,depth,1);
    float3 posWS = position.xyz/position.w;
    bool twoSided = (normalData.w > 0.5);
    
    float3 color = pointLightShade(input.lightPos, input.color, float4(uniforms.camPos).xyz, posWS, normal, specularData.xyz, twoSided);
    return float4(color,1);
}
