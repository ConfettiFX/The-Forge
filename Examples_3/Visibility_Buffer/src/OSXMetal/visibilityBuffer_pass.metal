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

// This shader performs the Visibility Buffer pass: store draw / triangle IDs per pixel.

#include <metal_stdlib>
using namespace metal;

struct PackedVertexPosData {
    packed_float3 position;
};

struct PackedVertexAttrData {
    packed_float2 texCoord;
};

struct VSOutput {
	float4 position [[position]];
    float2 texCoord;
    uint triangleID;
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
vertex VSOutput VSMain(constant PerBatchUniforms& perBatch [[buffer(0)]],
                       constant PackedVertexPosData* vertexPos [[buffer(1)]],
                       constant PerFrameConstants& uniforms [[buffer(2)]],
                       constant uint* filteredTriangles [[buffer(3)]],
                       constant IndirectDrawArguments* indirectDrawArgs [[buffer(4)]],
                       constant PackedVertexAttrData* vertexAttrs [[buffer(5)]],
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
    PackedVertexAttrData vertexAttr = vertexAttrs[vId];
    
    // Output data to the pixel shader
	VSOutput result;
    result.position = uniforms.transform[VIEW_CAMERA].mvp * float4(vertPos.position, 1.0f);
    result.texCoord = vertexAttr.texCoord;
    result.triangleID = triangleId;
	return result;
}

uint packVisBufData(bool opaque, uint drawId, uint triangleId)
{
    uint packed = ((drawId << 23) & 0x7F800000) | (triangleId & 0x007FFFFF);
    return (opaque ? packed : (1 << 31) | packed);
}

// Pixel shader for opaque geometry
fragment [[early_fragment_tests]] float4 PSMainOpaque(VSOutput input [[stage_in]],
                             constant PerBatchUniforms& perBatch [[buffer(0)]])
{
    // Pack draw / triangle Id data into a 32-bit uint and store it in a RGBA8 texture
    return unpack_unorm4x8_to_float(packVisBufData(true,perBatch.drawId,input.triangleID));
}

// Pixel shader for alpha tested geometry
fragment float4 PSMainAlphaTested(VSOutput input [[stage_in]],
                                  constant PerBatchUniforms& perBatch [[buffer(0)]],
                                  texture2d<float> diffuseMap [[texture(1)]])
{
    // Perform alpha testing: sample the texture and discard the fragment if alpha is under a threshold
    constexpr sampler s(address::repeat);
    float4 color = diffuseMap.sample(s,input.texCoord);
    if (color.a < 0.5)
    {
        discard_fragment();
    }
    
    // Pack draw / triangle Id data into a 32-bit uint and store it in a RGBA8 texture
    return unpack_unorm4x8_to_float(packVisBufData(false,perBatch.drawId,input.triangleID));
}
