/*
 * Copyright (c) 2018 Kostas Anagnostou (https://twitter.com/KostasAAA).
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
#include <metal_atomic>
#include <metal_compute>
using namespace metal;

inline float3x3 matrix_ctor(float4x4 m) {
        return float3x3(m[0].xyz, m[1].xyz, m[2].xyz);
}
struct Compute_Shader
{

    struct Uniforms_cbPerPass {

        float4x4 projView;
        float4x4 invProjView;
        float4 rtSize;
        float4 lightDir;
        float4 cameraPos;
    };
    constant Uniforms_cbPerPass & cbPerPass;
	texture2d<float, access::read> normalbuffer;
    texture2d<float, access::read> shadowbuffer;
    texture2d<float, access::read_write> outputRT;

    void main(uint3 DTid)
    {
        const float lightIntensity = 5;
        const float3 lightColour = float3(1, 1, 0.5);

        float3 normal = normalbuffer.read(uint2(DTid.xy)).xyz;
        float NdotL = saturate(dot(normal, cbPerPass.lightDir.xyz));

        float shadowFactor = ( shadowbuffer.read(uint2(float2(DTid.xy)), 0)).x;

        float ambient = mix(0.5, 0.2, (normal.y * (float)(0.5)) + (float)(0.5));

        float3 diffuse = ((float3)(((lightIntensity * shadowFactor) * NdotL)) * lightColour) + (float3)(ambient);
		
        outputRT.write(float4(diffuse, 1), uint2(DTid.xy));
    };

    Compute_Shader(
		constant Uniforms_cbPerPass & cbPerPass,
        texture2d<float, access::read> normalbuffer,
        texture2d<float, access::read> shadowbuffer,
        texture2d<float, access::read_write> outputRT) : cbPerPass(cbPerPass),
    normalbuffer(normalbuffer),
    shadowbuffer(shadowbuffer),
    outputRT(outputRT) {}
};

//[numthreads(16,16,1)]
kernel void stageMain(
					uint3 								DTid [[thread_position_in_grid]],
					constant     							Compute_Shader::Uniforms_cbPerPass & cbPerPass [[buffer(1)]],
					texture2d<float, access::read> 			normalbuffer [[texture(0)]],
					texture2d<float, access::read> 			shadowbuffer [[texture(1)]],
					texture2d<float, access::read_write> 	outputRT [[texture(2)]])
{
    uint3 DTid0;
    DTid0 = DTid;
    Compute_Shader main(cbPerPass, normalbuffer, shadowbuffer, outputRT);
        return main.main(DTid0);
}
