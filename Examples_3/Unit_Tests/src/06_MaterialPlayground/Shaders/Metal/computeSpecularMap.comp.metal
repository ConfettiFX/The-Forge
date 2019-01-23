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
#include <metal_atomic>
#include <metal_compute>
using namespace metal;

inline float3x3 matrix_ctor(float4x4 m) {
        return float3x3(m[0].xyz, m[1].xyz, m[2].xyz);
}
struct Compute_Shader
{
    uint nMips = 5;
    uint maxSize = 128;

    float Pi = 3.14159274;
    int SampleCount = IMPORTANCE_SAMPLE_COUNT;

    struct PrecomputeSkySpecularData
    {
        uint mipSize;
        float roughness;
    };

    struct Uniforms_RootConstant {

        uint mipSize;
        float roughness;
    };
    constant Uniforms_RootConstant & RootConstant;

    texturecube<float> srcTexture;
    texture2d_array<float, access::read_write> dstTexture;
    sampler skyboxSampler;

    float RadicalInverse_VdC(uint bits)
    {
        bits = ((bits << (uint)(16)) | (bits >> (uint)(16)));
        bits = (((bits & 1431655765u) << (uint)(1)) | ((bits & 2863311530u) >> (uint)(1)));
        bits = (((bits & 858993459u) << (uint)(2)) | ((bits & 3435973836u) >> (uint)(2)));
        bits = (((bits & 252645135u) << (uint)(4)) | ((bits & 4042322160u) >> (uint)(4)));
        bits = (((bits & 16711935u) << (uint)(8)) | ((bits & 4278255360u) >> (uint)(8)));
        return float(bits) * 2.328306e-10;
    };

    float2 Hammersley(uint i, uint N)
    {
        return float2(float(i) / float(N), RadicalInverse_VdC(i));
    };

    float DistributionGGX(float3 N, float3 H, float roughness)
    {
        float a = roughness * roughness;
        float a2 = a * a;
        float NdotH = max(dot(N, H), 0.0);
        float NdotH2 = NdotH * NdotH;

        float nom = a2;
        float denom = (NdotH2 * (a2 - (float)(1.0))) + (float)(1.0);
        denom = ((Pi * denom) * denom);

        return nom / denom;
    };

    float3 ImportanceSampleGGX(float2 Xi, float3 N, float roughness)
    {
        float a = roughness * roughness;

        float phi = ((float)(2.0) * Pi) * Xi.x;
        float cosTheta = sqrt(((float)(1.0) - Xi.y) / ((float)(1.0) + (((a * a) - (float)(1.0)) * Xi.y)));
        float sinTheta = sqrt((float)(1.0) - (cosTheta * cosTheta));

        float3 H;
        H.x = (cos(phi) * sinTheta);
        H.y = (sin(phi) * sinTheta);
        H.z = cosTheta;

        float3 up = ((abs(N.z) < (float)(0.9990000))?(float3(0.0, 0.0, 1.0)):(float3(1.0, 0.0, 0.0)));
        float3 tangent = normalize(cross(up, N));
        float3 bitangent = cross(N, tangent);

        float3 sampleVec = ((tangent * (float3)(H.x)) + (bitangent * (float3)(H.y))) + (N * (float3)(H.z));
        return normalize(sampleVec);
    };

    void main(uint3 DTid)
    {
        uint3 threadPos = DTid;

        float mipRoughness = RootConstant.roughness;
        uint mipSize = RootConstant.mipSize;

        if ((threadPos.x >= mipSize) || (threadPos.y >= mipSize))
        {

            return;
        }
        float2 texcoords = float2(float((half)(threadPos.x) + 0.5) / (float)(mipSize), float((half)(threadPos.y) + 0.5) / (float)(mipSize));

        float3 sphereDir;
        if (threadPos.z <= (uint)(0))
        {

            sphereDir = normalize(float3(0.5, (-(texcoords.y - (float)(0.5))), (-(texcoords.x - (float)(0.5)))));
        }
        else if (threadPos.z <= (uint)(1))
        {

            sphereDir = normalize(float3((-0.5), (-(texcoords.y - (float)(0.5))), texcoords.x - (float)(0.5)));
        }
        else if (threadPos.z <= (uint)(2))
        {

            sphereDir = normalize(float3(texcoords.x - (float)(0.5), 0.5, texcoords.y - (float)(0.5)));
        }
        else if (threadPos.z <= (uint)(3))
        {

            sphereDir = normalize(float3(texcoords.x - (float)(0.5), (-0.5), (-(texcoords.y - (float)(0.5)))));
        }
        else if (threadPos.z <= (uint)(4))
        {

            sphereDir = normalize(float3(texcoords.x - (float)(0.5), (-(texcoords.y - (float)(0.5))), 0.5));
        }
        else
        {

            sphereDir = normalize(float3((-(texcoords.x - (float)(0.5))), (-(texcoords.y - (float)(0.5))), (-0.5)));
        }

        float3 N = sphereDir;
        float3 R = N;
        float3 V = R;

        float totalWeight = 0.0;
        float4 prefilteredColor = float4(0.0, 0.0, 0.0, 0.0);
	    float srcTextureSize = float(srcTexture.get_width());

        for (int i = 0; i < SampleCount; (++i)) {

            float2 Xi = Hammersley(i, SampleCount);
            float3 H = ImportanceSampleGGX(Xi, N, mipRoughness);
            float3 L = normalize(((float3)(((float)(2.0) * dot(V, H))) * H) - V);

            float NdotL = max(dot(N, L), 0.0);
            if (NdotL > (float)(0.0))
            {

                float D = DistributionGGX(N, H, mipRoughness);
                float NdotH = max(dot(N, H), 0.0);
                float HdotV = max(dot(H, V), 0.0);
                float pdf = ((D * NdotH) / ((float)(4.0) * HdotV)) + (float)(0.00010000000);

                float saTexel = ((float)(4.0) * Pi) / (float)(((6.0 * (half)(srcTextureSize)) * (half)(srcTextureSize)));
                float saSample = (float)(1.0) / (float(SampleCount) * pdf + 0.0001);

			    float mipLevel = mipRoughness == 0.0 ? 0.0 : max(0.5 * log2(saSample / saTexel) + 1.0f, 0.0f);

                prefilteredColor += ( srcTexture.sample(skyboxSampler, L, level(mipLevel)) * (float4)(NdotL));

                totalWeight += NdotL;
            }
        }

        prefilteredColor = (prefilteredColor / (float4)(totalWeight));
        dstTexture.write(prefilteredColor, uint3(threadPos).xy, uint3(threadPos).z);
    };

    Compute_Shader(constant Uniforms_RootConstant & RootConstant,
        texturecube<float> srcTexture,
        texture2d_array<float, access::read_write> dstTexture,
    sampler skyboxSampler) : RootConstant(RootConstant),
    srcTexture(srcTexture),
    dstTexture(dstTexture),
    skyboxSampler(skyboxSampler) {}
};

//[numthreads(16, 16, 1)]
kernel void stageMain(uint3 DTid [[thread_position_in_grid]],
constant     Compute_Shader::Uniforms_RootConstant & RootConstant [[buffer(1)]],
    texturecube<float> srcTexture [[texture(2)]],
    texture2d_array<float, access::read_write> dstTexture [[texture(3)]],
    sampler skyboxSampler [[sampler(4)]]) {
    uint3 DTid0;
    DTid0 = DTid;
    Compute_Shader main(RootConstant, srcTexture, dstTexture, skyboxSampler);
        return main.main(DTid0);
}
