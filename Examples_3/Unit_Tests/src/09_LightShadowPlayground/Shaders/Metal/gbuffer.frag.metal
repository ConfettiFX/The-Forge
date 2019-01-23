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

#define PI 3.1415926289793f
#define PI_2 (3.1415926289793f*2.0f)

struct PSOutput
{
    float4 albedo       [[color(0)]];
    float4 normal       [[color(1)]];
    float4 position     [[color(2)]];
};
struct VSOutput
{
  float4 Position [[position]];
  float4 WorldPosition;
  float4 Color;
  float4 Normal;
  int IfPlane;
};
// Pixel shader for opaque geometry
[[early_fragment_tests]] fragment PSOutput stageMain(VSOutput input                 [[stage_in]],
                                                     sampler textureSampler         [[sampler(0)]],
                                                     texture2d<float> SphereTex     [[texture(0)]],
                                                     texture2d<float> PlaneTex      [[texture(1)]])
{
    PSOutput Out;
    Out.albedo = input.Color;
    Out.normal = (input.Normal + 1) * 0.5f;
    if (input.IfPlane == 0){//sphere
        Out.albedo *= SphereTex.sample(textureSampler, Out.normal.xy);
    }
    else {//plane
        Out.albedo *= PlaneTex.sample(textureSampler, input.WorldPosition.xz);
    }   
    Out.position = input.WorldPosition;
    return Out;
}
