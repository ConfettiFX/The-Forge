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

// This shader performs the Deferred rendering pass: store per pixel geometry data.

#include <metal_stdlib>
using namespace metal;

struct PackedVertexPosData {
    packed_float3 position;
};

struct PackedVertexTexcoord {
    packed_float2 texCoord;
};

struct PackedVertexNormal {
    packed_float3 normal;
};

struct PackedVertexTangent {
    packed_float3 tangent;
};

struct VSOutput {
	float4 position [[position]];
    float2 texCoord;
    float3 normal;
    float3 tangent;
    uint twoSided;
};

struct PSOutput
{
    float4 albedo       [[color(0)]];
    float4 normal       [[color(1)]];
    float4 specular     [[color(2)]];
    float4 simulation   [[color(3)]];
};

// Pixel shader for alpha tested geometry
fragment PSOutput stageMain(VSOutput input                  [[stage_in]],
                            sampler textureSampler          [[sampler(0)]],
                            texture2d<float> diffuseMap     [[texture(0)]],
                            texture2d<float> normalMap      [[texture(1)]],
                            texture2d<float> specularMap    [[texture(2)]])
{
    PSOutput Out;
    
    float4 albedo = diffuseMap.sample(textureSampler,input.texCoord);
	uint twoSided = input.twoSided;
    if (albedo.a < 0.5) discard_fragment();
    
    float4 normalMapRG = normalMap.sample(textureSampler,input.texCoord);
    
    float3 reconstructedNormalMap;
    reconstructedNormalMap.xy = normalMapRG.ga * 2 - 1;
    reconstructedNormalMap.z = sqrt(1 - dot(reconstructedNormalMap.xy, reconstructedNormalMap.xy));
    
    float3 normal = input.normal;
    float3 tangent = input.tangent;
    float3 binormal = cross(tangent, normal);
    
	Out.normal = float4((reconstructedNormalMap.x * tangent + reconstructedNormalMap.y * binormal + reconstructedNormalMap.z * normal) * 0.5 + 0.5, 0.0);
	Out.albedo = albedo;
	Out.albedo.a = twoSided > 0 ? 1.0f : 0.0f;
    Out.specular = specularMap.sample(textureSampler, input.texCoord);
    Out.simulation = float4(0.0f);
    return Out;
}
