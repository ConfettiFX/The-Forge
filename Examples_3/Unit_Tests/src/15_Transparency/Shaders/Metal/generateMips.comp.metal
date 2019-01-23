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
#include <metal_compute>
using namespace metal;

struct Compute_Shader
{
    texture2d<float, access::read_write> Source;
    texture2d<float, access::read_write> Destination;
    struct Uniforms_RootConstant
    {
        uint2 MipSize;
    };
    constant Uniforms_RootConstant & RootConstant;
    void main(uint3 id)
    {
        if (((id.x < RootConstant.MipSize.x) && (id.y < RootConstant.MipSize.y)))
        {
            float3 color = 0.0;
            for (int x = 0; (x < 2); (++x))
            {
                for (int y = 0; (y < 2); (++y))
                {
                    (color += (float3)(Source.read(((id.xy * (uint2)(2)) + uint2(x, y)))));
                }
            }
            (Destination.write(float4(color * (float3)(0.25), 0.0), uint2(id.xy)));
        }
    };

    Compute_Shader(
		texture2d<float, access::read_write> Source,
        texture2d<float, access::read_write> Destination,
        constant Uniforms_RootConstant & RootConstant) :
			Source(Source),
    		Destination(Destination),
    		RootConstant(RootConstant) 
        {}
};

//[numthreads(16, 16, 1)]
kernel void stageMain(
uint3 id [[thread_position_in_grid]],
    texture2d<float, access::read_write> Source [[texture(0)]],
    texture2d<float, access::read_write> Destination [[texture(1)]],
    constant Compute_Shader::Uniforms_RootConstant & RootConstant [[buffer(0)]])
{
    uint3 id0;
    id0 = id;
    Compute_Shader main(
    Source,
    Destination,
    RootConstant);
    return main.main(id0);
}
