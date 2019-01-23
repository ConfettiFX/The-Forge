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
#include <metal_compute>
using namespace metal;

inline float3x3 matrix_ctor(float4x4 m)
{
    return float3x3(m[0].xyz, m[1].xyz, m[2].xyz);
}
struct Compute_Shader
{

    const float Pi = 3.141593;
    float SampleDelta = 0.025000;

    texturecube<float> srcTexture;

    texture2d_array<float, access::read_write> dstTexture;
    sampler skyboxSampler;

    float4 computeIrradiance(float3 N)
    {

        float4 irradiance = float4(0.000000, 0.000000, 0.000000, 0.000000);

        float3 up = float3(0.000000, 1.000000, 0.000000);
        float3 right = cross(up, N);
        up = cross(N, right);

        float nrSamples = 0.000000;

        for (float phi = 0.000000; phi < ((const float)(2.000000) * Pi); phi += SampleDelta)
        {

            for (float theta = 0.000000; theta < ((const float)(0.500000) * Pi); theta += SampleDelta)
            {

                float3 tangentSample = float3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));

                float3 sampleVec = (((float3)(tangentSample.x) * right) + ((float3)(tangentSample.y) * up)) + ((float3)(tangentSample.z) * N);

                float4 sampledValue = srcTexture.sample(skyboxSampler, sampleVec, level(0));

                irradiance += float4((sampledValue.rgb * (float3)(cos(theta))) * (float3)(sin(theta)), sampledValue.a);
                (nrSamples++);
            }
        }

        return ((float4)(Pi)*irradiance) * (float4)(((const float)(1.000000) / float(nrSamples)));
    };

    void main(uint3 DTid)
    {

        float3 threadPos = (float3)DTid;
        uint pixelOffset = 0;

        for (uint i = 0; (float)(i) < threadPos.z; (++i))
        {

            pixelOffset += (uint)((32 * 32));
        }

        float2 texcoords = float2(float(threadPos.x + (float)(0.500000)) / 32.000000, float(threadPos.y + (float)(0.500000)) / 32.000000);

        float3 sphereDir;

        if (threadPos.z <= (float)(0))
        {
            sphereDir = normalize(float3(0.500000, (-(texcoords.y - (float)(0.500000))), (-(texcoords.x - (float)(0.500000)))));
        }
        else if (threadPos.z <= (float)(1))
        {
            sphereDir = normalize(float3((-0.500000), (-(texcoords.y - (float)(0.500000))), texcoords.x - (float)(0.500000)));
        }
        else if (threadPos.z <= (float)(2))
        {
            sphereDir = normalize(float3(texcoords.x - (float)(0.500000), 0.500000, texcoords.y - (float)(0.500000)));
        }
        else if (threadPos.z <= (float)(3))
        {
            sphereDir = normalize(float3(texcoords.x - (float)(0.500000), (-0.500000), (-(texcoords.y - (float)(0.500000)))));
        }
        else if (threadPos.z <= (float)(4))
        {
            sphereDir = normalize(float3(texcoords.x - (float)(0.500000), (-(texcoords.y - (float)(0.500000))), 0.500000));
        }
        else
        {
            sphereDir = normalize(float3((-(texcoords.x - (float)(0.500000))), (-(texcoords.y - (float)(0.500000))), (-0.500000)));
        }

        //uint pixelId = ((float)(pixelOffset) + (threadPos.y * (float)(32))) + threadPos.x;

        float4 irradiance = computeIrradiance(sphereDir);

        dstTexture.write(irradiance, uint2(int2(threadPos.xy)), threadPos.z);
    };

    Compute_Shader(texturecube<float> srcTexture,
                   texture2d_array<float, access::read_write> dstTexture,
                   sampler skyboxSampler) : srcTexture(srcTexture),
                                            dstTexture(dstTexture),
                                            skyboxSampler(skyboxSampler) {}
};

//[numthreads(16, 16, 1)]
kernel void stageMain(uint3 DTid[[thread_position_in_grid]],
                      texturecube<float> srcTexture[[texture(1)]],
                      texture2d_array<float, access::read_write> dstTexture[[texture(3)]],
                      sampler skyboxSampler[[sampler(4)]])
{
    uint3 DTid0;
    DTid0 = DTid;
    Compute_Shader main(srcTexture, dstTexture, skyboxSampler);
    return main.main(DTid0);
}
