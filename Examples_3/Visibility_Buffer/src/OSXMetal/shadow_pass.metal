/*
 * Copyright (c) 2018 Confetti Interactive Inc.
 *
 * This file is part of TheForge
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

struct PackedVertexPosData {
    packed_float3 position;
};

struct VSOut {
    float4 position [[position]];
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

vertex VSOut VSMain(constant PackedVertexPosData* vertexPos [[buffer(0)]],
                    constant PerFrameConstants& uniforms [[buffer(1)]],
                    constant PerBatchUniforms& perBatch [[buffer(2)]],
                    constant IndirectDrawArguments* indirectDrawArgs [[buffer(3)]],
                    constant uint* filteredTriangles [[buffer(4)]],
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
    
    // Load vertex data from vertex buffer using global vertexId
    PackedVertexPosData vert = vertexPos[vId];
    
    VSOut output;
    output.position = uniforms.transform[VIEW_SHADOW].mvp * float4(vert.position, 1.0f);
    return output;
}

fragment float4 PSMain(VSOut input [[stage_in]])
{    
    return float4(0,0,0,0);
}
