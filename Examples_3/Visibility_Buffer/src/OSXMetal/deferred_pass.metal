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

struct PerBatchUniforms {
    uint drawId;
    uint twoSided;  // possible values: 0/1
};

struct IndirectDrawArguments
{
    uint vertexCount;
    uint instanceCount;
    uint startVertex;
    uint startInstance;
};

// Vertex shader
vertex VSOutput VSMain(constant PackedVertexPosData* vertexPos [[buffer(0)]],
                       constant PerFrameConstants& uniforms [[buffer(1)]],
                       constant uint* filteredTriangles [[buffer(2)]],
                       constant PerBatchUniforms& perBatch [[buffer(3)]],
                       constant IndirectDrawArguments* indirectDrawArgs [[buffer(4)]],
                       constant PackedVertexTexcoord* vertexTexcoord [[buffer(5)]],
                       constant PackedVertexNormal* vertexNormal [[buffer(6)]],
                       constant PackedVertexTangent* vertexTangent [[buffer(7)]],
                       uint vertexId [[vertex_id]])
{
    // Get the indirect draw arguments data for this batch
    IndirectDrawArguments batchData = indirectDrawArgs[perBatch.drawId];
    
    // Calculate the current triangleID from vertexId
    uint startTriangle = indirectDrawArgs[perBatch.drawId].startVertex / 3;
    uint vertexInBatch = vertexId - batchData.startVertex;
    uint triangleInBatch = vertexInBatch/3;
    
    // Load the triangleId from the filteredTriangles buffer which contains all triangles that passed the culling tests
    uint triangleId = filteredTriangles[startTriangle + triangleInBatch];
    
    // Calculate global vertexId
    uint vertInTri = vertexInBatch % 3;
    uint vId = batchData.startVertex + triangleId*3 + vertInTri;
    
    // Load vertex data from vertex bfufer using global vertexId
    PackedVertexPosData vertPos = vertexPos[vId];
    PackedVertexTexcoord vertTexcoord = vertexTexcoord[vId];
    PackedVertexNormal vertNormal = vertexNormal[vId];
    PackedVertexTangent vertTangent = vertexTangent[vId];
    
    // Output data to the pixel shader
	VSOutput result;
    result.position = uniforms.transform[VIEW_CAMERA].mvp * float4(vertPos.position,1);
    result.texCoord = vertTexcoord.texCoord;
    result.normal = vertNormal.normal; // TODO: multiply by normal matrix
    result.tangent = vertTangent.tangent;   // TODO: multiply by normal matrix
    result.twoSided = perBatch.twoSided;
	return result;
}

struct PSOutput
{
    float4 color0 [[color(0)]];
    float4 color1 [[color(1)]];
    float4 color2 [[color(2)]];
    float4 color3 [[color(3)]];
};

// Pixel shader for opaque geometry
fragment [[early_fragment_tests]] PSOutput PSMainOpaque(VSOutput input [[stage_in]],
                                                        sampler textureSampler [[sampler(0)]],
                                                        texture2d<float> diffuseMap [[texture(1)]],
                                                        texture2d<float> normalMap [[texture(2)]],
                                                        texture2d<float> specularMap [[texture(3)]])
{
    float3 vertexNormal = input.normal;
    float3 vertexTangent = input.tangent;
    float3 vertexBinormal = cross(vertexTangent, vertexNormal);
    
    float2 normalMapRG = normalMap.sample(textureSampler,input.texCoord).rg;
    float3 reconstructedNormalMap;
    reconstructedNormalMap.xy = normalMapRG*2-1;
    reconstructedNormalMap.z = sqrt(1-dot(reconstructedNormalMap.xy, reconstructedNormalMap.xy));
    float3 normal = reconstructedNormalMap.x * vertexTangent + reconstructedNormalMap.y * vertexBinormal + reconstructedNormalMap.z * vertexNormal;
        
    PSOutput output;
    output.color0 = diffuseMap.sample(textureSampler, input.texCoord);
    output.color1 = float4(normal*0.5+0.5,0);
    output.color2 = specularMap.sample(textureSampler, input.texCoord);
    output.color3 = float4(0.0f);
    return output;
}

// Pixel shader for alpha tested geometry
fragment PSOutput PSMainAlphaTested(VSOutput input [[stage_in]],
                                    sampler textureSampler [[sampler(0)]],
                                    texture2d<float> diffuseMap [[texture(1)]],
                                    texture2d<float> normalMap [[texture(2)]],
                                    texture2d<float> specularMap [[texture(3)]])
{
    float3 vertexNormal = input.normal;
    float3 vertexTangent = input.tangent;
    float3 vertexBinormal = cross(vertexTangent, vertexNormal);
    
    float2 normalMapRG = normalMap.sample(textureSampler,input.texCoord).rg;
    float3 reconstructedNormalMap;
    reconstructedNormalMap.xy = normalMapRG*2-1;
    reconstructedNormalMap.z = sqrt(1-dot(reconstructedNormalMap.xy, reconstructedNormalMap.xy));    
    float3 normal = reconstructedNormalMap.x * vertexTangent + reconstructedNormalMap.y * vertexBinormal + reconstructedNormalMap.z * vertexNormal;
    
    PSOutput output;
    // Perform alpha testing: sample the texture and discard the fragment if alpha is under a threshold
    float4 color = diffuseMap.sample(textureSampler,input.texCoord);
    if (color.a < 0.5)
    {
        discard_fragment();
    }
   
    output.color0 = diffuseMap.sample(textureSampler, input.texCoord);
    output.color1 = float4(normal*0.5+0.5,input.twoSided);
    output.color2 = specularMap.sample(textureSampler, input.texCoord);
    output.color3 = float4(0.0f);
    return output;
}
