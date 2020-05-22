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

#include <metal_stdlib>
using namespace metal;

#include "renderSceneBRDF.h"

struct VSInput
{
    float3 position [[attribute(0)]];
    float3 normal   [[attribute(1)]];
    float2 uv   [[attribute(2)]];
};

struct VSOutput {
    float4 position [[position]];
    float3 pos;
    float3 normal;
    float2 uv;
};

vertex VSOutput stageMain(VSInput In                              [[stage_in]],
                          constant VSDataPerFrame& vsDataPerFrame [[buffer(UPDATE_FREQ_PER_FRAME)]],
                          constant VSDataPerDraw& vsDataPerDraw   [[buffer(UPDATE_FREQ_PER_DRAW)]]
)
{
    VSOutput result;
    float4x4 tempMat = vsDataPerFrame.cbCamera.projView * vsDataPerDraw.cbObject.worldMat;
    result.position = tempMat * float4(In.position,1.0);
    result.pos = (vsDataPerDraw.cbObject.worldMat * float4(In.position,1.0)).xyz;
    result.normal = (vsDataPerDraw.cbObject.worldMat * float4(In.normal.xyz, 0.0f)).xyz;
    result.uv= In.uv;
    return result;
}
