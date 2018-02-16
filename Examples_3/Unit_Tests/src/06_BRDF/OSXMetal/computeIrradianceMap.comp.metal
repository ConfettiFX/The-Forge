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

constant float Pi = 3.14159265359;
constant float SampleDelta = 0.025;

float4 computeIrradiance(texturecube<float,access::sample> skybox, sampler samp, float3 N)
{
    float4 irradiance = float4(0.0);
    
    float3 up       = float3(0.0, 1.0, 0.0);
    float3 right    = cross(up, N);
    up              = cross(N, right);
    
    float nrSamples = 0.0;
    for(float phi = 0.0; phi < 2.0 * Pi; phi += SampleDelta)
    {
        for(float theta = 0.0; theta < 0.5 * Pi; theta += SampleDelta)
        {
            // spherical to cartesian (in tangent space)
            float3 tangentSample = float3(sin(theta) * cos(phi),  sin(theta) * sin(phi), cos(theta));
            // tangent space to world
            float3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * N;
            
            float4 sampledValue = skybox.sample(samp, sampleVec);
            irradiance += float4(sampledValue.rgb * cos(theta) * sin(theta), sampledValue.a);
            nrSamples++;
        }
    }
    return Pi * irradiance * (1.0 / float(nrSamples));
}

//[numthreads(16, 16, 1)]
kernel void stageMain(uint3 threadPos                                  [[thread_position_in_grid]],
                      texturecube<float,access::sample> srcTexture     [[texture(0)]],
                      device float4* dstBuffer                         [[buffer(0)]],
                      sampler skyboxSampler                            [[sampler(0)]])
{
    // Calculate the initial offset.
    uint pixelOffset = 0;
    for(uint i = 0; i < threadPos.z; i++)
    {
        pixelOffset += 32 * 32;
    }

    float2 texcoords = float2(float(threadPos.x + 0.5) / 32.0f, float(threadPos.y + 0.5) / 32.0f);
    
    float3 sphereDir;
    if(threadPos.z <= 0) {
        sphereDir = normalize(float3(0.5, -(texcoords.y - 0.5), -(texcoords.x - 0.5)));
    }
    else if(threadPos.z <= 1) {
        sphereDir = normalize(float3(-0.5, -(texcoords.y - 0.5), texcoords.x - 0.5));
    }
    else if(threadPos.z <= 2) {
        sphereDir = normalize(float3(texcoords.x - 0.5, 0.5, texcoords.y - 0.5));
    }
    else if(threadPos.z <= 3) {
        sphereDir = normalize(float3(texcoords.x - 0.5, -0.5, -(texcoords.y - 0.5)));
    }
    else if(threadPos.z <= 4) {
        sphereDir = normalize(float3(texcoords.x - 0.5, -(texcoords.y - 0.5), 0.5));
    }
    else if(threadPos.z <= 5) {
        sphereDir = normalize(float3(-(texcoords.x - 0.5), -(texcoords.y - 0.5), -0.5));
    }

    float4 irradiance = computeIrradiance(srcTexture, skyboxSampler, sphereDir);
    uint pixelId = pixelOffset + threadPos.y * 32 + threadPos.x;
    dstBuffer[pixelId] = irradiance;
}
