/*
 * Copyright (c) 2018-2020 The Forge Interactive Inc.
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

#include "shader_defs.h"

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

struct Textures {
    sampler textureFilter;
    array<texture2d<float>,MATERIAL_BUFFER_SIZE> diffuseMaps;
    array<texture2d<float>,MATERIAL_BUFFER_SIZE> normalMaps;
    array<texture2d<float>,MATERIAL_BUFFER_SIZE> specularMaps;
    constant MeshConstants* meshConstantsBuffer;
};

// Pixel shader for alpha tested geometry
fragment PSOutput stageMain(
    VSOutput input                                     [[stage_in]],
    constant Textures& textures                        [[buffer(UNIT_VBPASS_TEXTURES)]],
    constant uint* indirectMaterialBuffer              [[buffer(UNIT_INDIRECT_MATERIAL_RW)]],
    constant uint& drawID                              [[buffer(UINT_VBPASS_DRAWID)]]
)
{
	PSOutput Out;

	uint matBaseSlot = BaseMaterialBuffer(true, VIEW_CAMERA); //1 is camera view, 0 is shadow map view
	uint materialID = indirectMaterialBuffer[matBaseSlot + drawID];

	float4 albedo = textures.diffuseMaps[materialID].sample(textures.textureFilter, input.texCoord);
	if(albedo.a < 0.5) discard_fragment();
	
	uint twoSided = textures.meshConstantsBuffer[materialID].twoSided;
	
	// CALCULATE PIXEL COLOR USING INTERPOLATED ATTRIBUTES
	// Reconstruct normal map Z from X and Y
	float4 normalMapRG = textures.normalMaps[materialID].sample(textures.textureFilter, input.texCoord);
	float3 reconstructedNormalMap;
	reconstructedNormalMap.xy = normalMapRG.ga * 2 - 1;
	reconstructedNormalMap.z = sqrt(1 - dot(reconstructedNormalMap.xy, reconstructedNormalMap.xy));
	
	float3 normal = normalize(input.normal);
	float3 tangent = normalize(input.tangent);
	// Calculate vertex binormal from normal and tangent
	float3 binormal = normalize(cross(tangent, normal));
	// Calculate pixel normal using the normal map and the tangent space vectors
	Out.normal = float4((reconstructedNormalMap.x * tangent + reconstructedNormalMap.y * binormal + reconstructedNormalMap.z * normal) * 0.5 + 0.5, 0.0);
	Out.albedo = albedo;
	Out.albedo.a = twoSided > 0 ? 1.0f : 0.0f;
	Out.specular = textures.specularMaps[materialID].sample(textures.textureFilter, input.texCoord);
	Out.simulation = 0.0f;
	
	return Out;
}
