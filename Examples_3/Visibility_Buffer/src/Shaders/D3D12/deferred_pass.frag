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

#include "shader_defs.h"
#include "packing.h"

struct PsIn
{
    float4 position : SV_Position;
    float2 texCoord : TEXCOORD0;
    float3 normal : TEXCOORD1;
    float3 tangent : TEXCOORD2;
};

ConstantBuffer<RootConstant> indirectRootConstant : register(b1);

Texture2D diffuseMaps[] : register(t0, space1);
Texture2D normalMaps[] : register(t0, space2);
Texture2D specularMaps[] : register(t0, space3);

StructuredBuffer<uint> indirectMaterialBuffer: register(t0);
StructuredBuffer<MeshConstants> meshConstantsBuffer : register(t1);

SamplerState textureFilter : register(s0);

struct PSOut
{
    float4 albedo : SV_Target0;
    float4 normal : SV_Target1;
    float4 specular : SV_Target2;
    float4 simulation : SV_Target3;
};

PSOut main(PsIn In)
{
    PSOut Out = (PSOut)0;

	 uint matBaseSlot = BaseMaterialBuffer(false, 1); //1 is camera view, 0 is shadow map view
     uint materialID = indirectMaterialBuffer[matBaseSlot + indirectRootConstant.drawId];

	 Out.albedo = diffuseMaps[NonUniformResourceIndex(materialID)].Sample(textureFilter, In.texCoord);


   

    // CALCULATE PIXEL COLOR USING INTERPOLATED ATTRIBUTES
    // Reconstruct normal map Z from X and Y
    float4 normalData = normalMaps[NonUniformResourceIndex(materialID)].Sample(textureFilter, In.texCoord);

	float2 normalMapRG = normalData.ga;

    float3 reconstructedNormalMap;
    reconstructedNormalMap.xy = normalMapRG * 2 - 1;
    reconstructedNormalMap.z = sqrt(1 - dot(reconstructedNormalMap.xy, reconstructedNormalMap.xy));

	float3 normal = normalize(In.normal);
	float3 tangent = normalize(In.tangent);
    // Calculate vertex binormal from normal and tangent
    float3 binormal = normalize(cross(tangent, normal));
    // Calculate pixel normal using the normal map and the tangent space vectors
    Out.normal = float4((reconstructedNormalMap.x * tangent + reconstructedNormalMap.y * binormal + reconstructedNormalMap.z * normal) * 0.5 + 0.5, 0.0);
	Out.albedo.a = 0.0;
    Out.specular = specularMaps[NonUniformResourceIndex(materialID)].Sample(textureFilter, In.texCoord);
	
    return Out;
}
