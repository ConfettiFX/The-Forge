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

struct Uniforms_RootConstant
{
    uint2 MipSize;
};

//[numthreads(16, 16, 1)]
kernel void stageMain(
constant Uniforms_RootConstant& RootConstant [[buffer(UPDATE_FREQ_USER)]],
texture2d<float, access::read_write> Source [[texture(0)]],
texture2d<float, access::write> Destination [[texture(1)]],
uint3 id [[thread_position_in_grid]]
)
{
    if ((id.x < RootConstant.MipSize.x) && (id.y < RootConstant.MipSize.y))
    {
        float color = 1.0;
        for (uint x = 0; x < 2; x++)
        {
            for (uint y = 0; y < 2; y++)
            {
                color = fast::min(color, Source.read(id.xy * 2u + uint2(x, y)).x);
            }
        }
        Destination.write(float4(color), uint2(id.xy));
    }
}


