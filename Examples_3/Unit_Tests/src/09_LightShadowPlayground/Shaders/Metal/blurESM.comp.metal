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
#include <metal_compute>

using namespace metal;

#define PI 3.1415926535897932384626433832795
#define PI2 (2*3.1415926535897932384626433832795)

// just because local workgroup size is x=32 does not mean the shader is working along x dimension in the image.
#define MAX_WORKGROUP_SIZE 32

struct Uniforms_ESMInputConstants
{
    float Near;
    float Far;
    float FarNearDiff;
    float Near_Over_FarNearDiff;
    float Exponent;
	int   BlurWidth;
};
	
struct Uniforms_RootConstants
{
	uint FirstPass;
};

// 1 is a box blur, 2 is a good looking approximation to the gaussian visually, 3 is a very good numerical approximation
#define CENTRAL_LIMIT_ITERATIONS 2

#define BARRIER_AND_VISIBILITY   threadgroup_barrier(mem_flags::mem_threadgroup)

#define EMULATE_EXTRA_INVOCATIONS for (int virtualInvocationID=int(localInvocationID.x),emulationMacroID=0; virtualInvocationID<ESM_SHADOWMAP_RES; virtualInvocationID+=MAX_WORKGROUP_SIZE,emulationMacroID++)

float exponentiateDepth(float normalizedDepth, constant Uniforms_ESMInputConstants &esmConsts)
{
	return metal::exp2(esmConsts.Exponent * normalizedDepth-metal::log2(float(2*esmConsts.BlurWidth+1))*2.0*float(CENTRAL_LIMIT_ITERATIONS)-metal::log2(float(ESM_MSAA_SAMPLES)));
}

float mapDepth(texture2d_ms<float> shadowExpMap, int2 i2uv, int ix, thread int& foregroundCount, constant Uniforms_ESMInputConstants &esmConsts)
{
    float nonLinearValue = shadowExpMap.read(uint2(i2uv), ix).x;
    float linearZ = (esmConsts.Far / (esmConsts.Near + nonLinearValue * esmConsts.FarNearDiff) - 1.0f) * esmConsts.Near_Over_FarNearDiff;
    if (linearZ > 0.99999f)
        return 0.0f;
    else
    {
        foregroundCount++;
        return exponentiateDepth(linearZ, esmConsts);
    }
}

float resolveDepth(texture2d_ms<float> shadowExpMap, int2 i2uv, constant Uniforms_ESMInputConstants &esmConsts)
{
    // this is not part of the original ESM algorithm, it is a workaround for the spheres being all unique vertices 
    int foregroundCount = 0;
	float resolvedMapped = 0.0f;

    for (int i = 0; i < ESM_MSAA_SAMPLES; i++)
        resolvedMapped += mapDepth(shadowExpMap, i2uv, i, foregroundCount, esmConsts);
    if (foregroundCount >= (ESM_MSAA_SAMPLES*3)/4)
        return resolvedMapped / float(foregroundCount);
    else
        return resolvedMapped + (exponentiateDepth(1.0f, esmConsts) * float(ESM_MSAA_SAMPLES - foregroundCount));
}

void naiveBlur(thread float* blurredVal, threadgroup float* blurAxisValues, thread uint3& localInvocationID,
			   constant Uniforms_ESMInputConstants &esmConsts)
{
    EMULATE_EXTRA_INVOCATIONS
        blurAxisValues[virtualInvocationID] = blurredVal[emulationMacroID];
        
    BARRIER_AND_VISIBILITY;
    EMULATE_EXTRA_INVOCATIONS
    {
        // it would be better if `BlurWidth` was a specialization constant
        int i=virtualInvocationID-esmConsts.BlurWidth;
        for (; i<virtualInvocationID; i++)
            blurredVal[emulationMacroID] += blurAxisValues[max(i,0)];
        i++;
        for (; i<=virtualInvocationID+esmConsts.BlurWidth; i++)
            blurredVal[emulationMacroID] += blurAxisValues[min(i,ESM_SHADOWMAP_RES-1)];
    }
    
    BARRIER_AND_VISIBILITY;
}
	
//[numthreads(32, 1, 1)]
kernel void stageMain(
	uint3 globalInvocationID [[thread_position_in_grid]],
	uint3 localInvocationID [[thread_position_in_threadgroup]],
    texture2d_ms<float, access::read> shadowExpMap [[texture(0)]],
    texture2d<float, access::write> blurredESM [[texture(1)]],
    sampler clampMiplessSampler [[sampler(0)]],
	device float* IntermediateResult [[buffer(0)]],
	constant Uniforms_ESMInputConstants & ESMInputConstants [[buffer(1)]],
	constant Uniforms_RootConstants & RootConstants [[buffer(2)]]
	)
{
	threadgroup float blurAxisValues[ESM_SHADOWMAP_RES];

    float blurredVal[ESM_SHADOWMAP_RES/MAX_WORKGROUP_SIZE];
    if (RootConstants.FirstPass!=0u)
    {
        EMULATE_EXTRA_INVOCATIONS
            blurredVal[emulationMacroID] = IntermediateResult[virtualInvocationID+globalInvocationID.y*ESM_SHADOWMAP_RES];
    }
    else
    {
        EMULATE_EXTRA_INVOCATIONS
            blurredVal[emulationMacroID] = resolveDepth(shadowExpMap, int2(virtualInvocationID,globalInvocationID.y),
														ESMInputConstants);
    }

    // box-blur many times over for the repeated box-convolution to become a Gaussian by the central limit theorem
    for (uint i=0; i<CENTRAL_LIMIT_ITERATIONS; i++)
        naiveBlur(blurredVal, blurAxisValues, localInvocationID, ESMInputConstants);

    if (RootConstants.FirstPass != 0u)
    {
        EMULATE_EXTRA_INVOCATIONS
            blurredESM.write(float4(blurredVal[emulationMacroID] * 8.0f, 0.0f, 0.0f, 1.0f), uint2(uint(globalInvocationID.y), virtualInvocationID));
    }
    else
    {
        EMULATE_EXTRA_INVOCATIONS
            IntermediateResult[uint(globalInvocationID.y) + as_type<uint>(virtualInvocationID * ESM_SHADOWMAP_RES)] = blurredVal[emulationMacroID];
    }
}
