/*
 * Copyright (c) 2018-2021 The Forge Interactive Inc.
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

#pragma clang diagnostic ignored "-Wmissing-prototypes"

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct CSData
{
    device uint* g_tile_counter [[id(0)]];
    device uint* g_ray_counter [[id(1)]];
    device uint* g_intersect_args [[id(2)]];
    device uint* g_denoiser_args [[id(3)]];
};

//[numthreads(1, 1, 1)]
kernel void stageMain(
device CSData& csData [[buffer(UPDATE_FREQ_NONE)]]
)
{
    uint tile_counter = csData.g_tile_counter[0];
    uint ray_counter = csData.g_ray_counter[0];

    csData.g_tile_counter[0] = 0u;
    csData.g_ray_counter[0] = 0u;

    csData.g_intersect_args[0] = (ray_counter + 63u) / 64u;
    csData.g_intersect_args[1] = 1u;
    csData.g_intersect_args[2] = 1u;

    csData.g_denoiser_args[0] = tile_counter;
    csData.g_denoiser_args[1] = 1u;
    csData.g_denoiser_args[2] = 1u;
}

