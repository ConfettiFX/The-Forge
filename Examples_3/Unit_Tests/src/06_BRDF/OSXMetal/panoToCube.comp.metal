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

constant float2 invAtan = float2(0.1591f, 0.3183f);
constant uint nMips = 11;
constant uint maxSize = 1024;

//[numthreads(16, 16, 1)]
kernel void stageMain(uint3 threadPos                                  [[thread_position_in_grid]],
                      texture2d<float,access::sample> srcTexture       [[texture(0)]],
                      device float4* dstBuffer                         [[buffer(0)]],
                      sampler skyboxSampler                            [[sampler(0)]])
{
    uint pixelOffset = 0;
    for(uint mip = 0; mip < nMips; mip++)
    {
        uint mipSize = maxSize >> mip;
        if(threadPos.x >= mipSize || threadPos.y >= mipSize) return;

        float2 texcoords = float2(float(threadPos.x + 0.5) / mipSize, 1.0f - float(threadPos.y + 0.5) / mipSize);
        float3 sphereDir;
        if(threadPos.z <= 0) {
            sphereDir = normalize(float3(0.5, -(texcoords.y - 0.5), -(texcoords.x - 0.5)));
        }
        else if(threadPos.z <= 1) {
            sphereDir = normalize(float3(-0.5, -(texcoords.y - 0.5), texcoords.x - 0.5));
        }
        else if(threadPos.z <= 2) {
            sphereDir = normalize(float3(texcoords.x - 0.5, -0.5, -(texcoords.y - 0.5)));
        }
        else if(threadPos.z <= 3) {
            sphereDir = normalize(float3(texcoords.x - 0.5, 0.5, texcoords.y - 0.5));
        }
        else if(threadPos.z <= 4) {
            sphereDir = normalize(float3(texcoords.x - 0.5, -(texcoords.y - 0.5), 0.5));
        }
        else if(threadPos.z <= 5) {
            sphereDir = normalize(float3(-(texcoords.x - 0.5), -(texcoords.y - 0.5), -0.5));
        }
        
        float2 panoUVs = float2(atan2(sphereDir.z, sphereDir.x), asin(sphereDir.y));
        panoUVs *= invAtan;
        panoUVs += 0.5f;
        
        float4 outColor = srcTexture.sample(skyboxSampler, panoUVs);

        pixelOffset += threadPos.z * mipSize * mipSize;
        uint pixelId = pixelOffset + threadPos.y * mipSize + threadPos.x;
        dstBuffer[pixelId] = outColor;
        pixelOffset += (6 - threadPos.z) * mipSize * mipSize;
    }
}
