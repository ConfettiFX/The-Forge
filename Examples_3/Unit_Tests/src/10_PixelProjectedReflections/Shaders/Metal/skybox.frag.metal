/*
 * Copyright (c) 2018-2019 Confetti Interactive Inc.
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

struct VSInput {
    float4 Position [[attribute(0)]];
};

struct VSOutput {
    float4 Position [[position]];
    float3 WorldPos;
};

fragment float4 stageMain(VSOutput input                               [[stage_in]],
                          texturecube<float, access::sample> skyboxTex [[texture(0)]],
                          sampler skyboxSampler                        [[sampler(0)]])
{
    float4 outColor = skyboxTex.sample(skyboxSampler, normalize(input.WorldPos));
    outColor = float4(pow(outColor.xyz, 1/2.2), 1.0);
    return outColor;
}
