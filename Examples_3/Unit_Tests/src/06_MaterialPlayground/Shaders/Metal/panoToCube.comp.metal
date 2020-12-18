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
 * "License") you may not use this file except in compliance
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
#include <metal_atomic>
#include <metal_compute>
using namespace metal;

inline float3x3 matrix_ctor(float4x4 m) {
        return float3x3(m[0].xyz, m[1].xyz, m[2].xyz);
}
struct Compute_Shader
{
#define NUM_THREADS_X 16
#define NUM_THREADS_Y 16
#define NUM_THREADS_Z 1

    struct Data
    {
        uint mip;
        uint maxSize;
    };

    struct Uniforms_RootConstant {

        uint mip;
        uint maxSize;
    };
    constant Uniforms_RootConstant & RootConstant;

    texture2d<float> srcTexture;

    texture2d_array<float, access::write> dstTexture;
    sampler skyboxSampler;

#define Pi 3.14159274
#define CubeSide 1.1547005
#define SLICES 6

    void main(uint3 DTid)
    {
        float2 invAtan = float2(0.1591, 0.31830000);

        float3 threadPos;
		threadPos.x = DTid.x;
		threadPos.y = DTid.y;
		threadPos.z = DTid.z;

        uint mip = RootConstant.mip;
        {
            uint mipSize = RootConstant.maxSize >> mip;
            if ((threadPos.x >= (float)(mipSize)) || (threadPos.y >= (float)(mipSize)))
            {
                return;
            }
            float2 texcoords = float2(float(threadPos.x + (float)(0.5)) / (float)(mipSize), 1.0 - (float(threadPos.y + (float)(0.5)) / (float)(mipSize)));
            float3 sphereDir;
            if (threadPos.z <= (float)(0))
            {

                sphereDir = normalize(float3(0.5, (-(texcoords.y - (float)(0.5))), (-(texcoords.x - (float)(0.5)))));
            }
            else if (threadPos.z <= (float)(1))
            {

                sphereDir = normalize(float3((-0.5), (-(texcoords.y - (float)(0.5))), texcoords.x - (float)(0.5)));
            }
            else if (threadPos.z <= (float)(2))
            {

                sphereDir = normalize(float3(texcoords.x - (float)(0.5), (-0.5), (-(texcoords.y - (float)(0.5)))));
            }
            else if (threadPos.z <= (float)(3))
            {

                sphereDir = normalize(float3(texcoords.x - (float)(0.5), 0.5, texcoords.y - (float)(0.5)));
            }
            else if (threadPos.z <= (float)(4))
            {

                sphereDir = normalize(float3(texcoords.x - (float)(0.5), (-(texcoords.y - (float)(0.5))), 0.5));
            }
            else
            {

                sphereDir = normalize(float3((-(texcoords.x - (float)(0.5))), (-(texcoords.y - (float)(0.5))), (-0.5)));
            }

            float2 panoUVs = float2(atan2(sphereDir.z, sphereDir.x), asin(sphereDir.y));
            panoUVs *= invAtan;
            panoUVs += (float2)(0.5);

            dstTexture.write( srcTexture.sample(skyboxSampler, panoUVs, level(mip)), uint3(uint3(threadPos.x, threadPos.y, threadPos.z)).xy, uint3(uint3(threadPos.x, threadPos.y, threadPos.z)).z);
        }
    };

    Compute_Shader(constant Uniforms_RootConstant & RootConstant,
        texture2d<float> srcTexture,
        texture2d_array<float, access::write> dstTexture,
    sampler skyboxSampler) : RootConstant(RootConstant),
    srcTexture(srcTexture),
    dstTexture(dstTexture),
    skyboxSampler(skyboxSampler) {}
};

struct CSData
{
    texture2d<float> srcTexture                           [[id(0)]];
    sampler skyboxSampler                                 [[id(1)]];
};

#ifndef TARGET_IOS
struct CSDataPerDraw
{
    texture2d_array<float, access::write> dstTexture [[id(0)]];
};
#endif

//[numthreads(16, 16, 1)]
kernel void stageMain(uint3 DTid                                                   [[thread_position_in_grid]],
#ifndef TARGET_IOS
                      constant CSData& csData                                      [[buffer(UPDATE_FREQ_NONE)]],
                      device CSDataPerDraw& csDataPerDraw                          [[buffer(UPDATE_FREQ_PER_DRAW)]],
#else
					  texture2d<float> srcTexture                           [[texture(0)]],
					  texture2d_array<float, access::write> dstTexture      [[texture(1)]],
					  sampler skyboxSampler                                 [[sampler(0)]],
#endif
                      constant Compute_Shader::Uniforms_RootConstant& RootConstant [[buffer(UPDATE_FREQ_USER)]]
)
{
    uint3 DTid0;
    DTid0 = DTid;
    Compute_Shader main(RootConstant,
#ifndef TARGET_IOS
						csData.srcTexture,
						csDataPerDraw.dstTexture,
						csData.skyboxSampler
#else
						srcTexture,
						dstTexture,
						skyboxSampler
#endif
						);
    return main.main(DTid0);
}
