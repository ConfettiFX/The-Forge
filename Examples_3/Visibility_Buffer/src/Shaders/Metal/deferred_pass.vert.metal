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
vertex VSOutput stageMain(uint vertexId                                     [[vertex_id]],
                          constant PackedVertexPosData* vertexPos           [[buffer(0)]],
                          constant PerFrameConstants& uniforms              [[buffer(1)]],
                          constant uint* filteredTriangles                  [[buffer(2)]],
                          constant PerBatchUniforms& perBatch               [[buffer(3)]],
                          constant IndirectDrawArguments* indirectDrawArgs  [[buffer(4)]],
                          constant PackedVertexTexcoord* vertexTexcoord     [[buffer(5)]],
                          constant PackedVertexNormal* vertexNormal         [[buffer(6)]],
                          constant PackedVertexTangent* vertexTangent       [[buffer(7)]])
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
    result.position = uniforms.transform[VIEW_CAMERA].mvp * float4(vertPos.position, 1.0f);
    result.texCoord = vertTexcoord.texCoord;
    result.normal = vertNormal.normal;
    result.tangent = vertTangent.tangent;
    result.twoSided = perBatch.twoSided;
	return result;
}
