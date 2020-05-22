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

#include "shader_defs.h"
#include "shading.h"
#include "deferred_shade_pointlight.h"

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

vertex VSOutput stageMain(VSInput input                         [[stage_in]],
                          uint instanceId                       [[instance_id]],
                          constant ArgData& vsData [[buffer(UPDATE_FREQ_NONE)]],
                          constant ArgDataPerFrame& vsDataPerFrame [[buffer(UPDATE_FREQ_PER_FRAME)]]
)
{
    VSOutput output;
    output.lightPos = vsData.lights[instanceId].position;
    output.color = vsData.lights[instanceId].color;
    output.position = vsDataPerFrame.uniforms.transform[VIEW_CAMERA].vp * float4((input.position.xyz * LIGHT_SIZE) + output.lightPos, 1);
    return output;
}
