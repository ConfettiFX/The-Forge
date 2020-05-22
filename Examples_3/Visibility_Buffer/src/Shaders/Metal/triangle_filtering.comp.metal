/*
 * Copyright (c) 2018-2020 The Forge Interactive Inc.
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

// This compute shader performs triangle filtering by executing different visibility tests for every triangle.
// Triangles that pass all tests are appended into the filteredTriangles buffer.
// Triangles that fail any of the tests are just ignored and hence discarded.
// For every triangle that passes the tests, the vertexCount property of IndirectDrawArguments is incremented in 3. This
// property is used later during rendering the scene geometry to determine the amount of triangles to draw per batch.

#include <metal_stdlib>
#include <metal_atomic>
#include <metal_compute>
using namespace metal;

#include "shader_defs.h"
#include "cull_argument_buffers.h"

// These are the tests performed per triangle. They can be toggled on/off setting this define macros to 0/1.
#define ENABLE_CULL_INDEX                1
#define ENABLE_CULL_BACKFACE            1
#define ENABLE_CULL_FRUSTUM                1
#define ENABLE_CULL_SMALL_PRIMITIVES    1
#define ENABLE_GUARD_BAND                0

struct BatchData
{
    uint triangleCount;
    uint triangleOffset;
    uint drawId;
    uint twoSided;
};

// This is the struct Metal uses to specify an indirect draw call.
struct IndirectDrawArguments
{
    atomic_uint vertexCount;
    uint instanceCount;
    uint startVertex;
    uint startInstance;
};

// Performs all the culling tests given 3 vertices
bool FilterTriangle(uint indices[3], float4 vertices[3], bool cullBackFace, float2 windowSize, uint samples)
{
#if ENABLE_CULL_INDEX
    if (indices[0] == indices[1]
        || indices[1] == indices[2]
        || indices[0] == indices[2])
    {
        return true;
    }
#endif
#if ENABLE_CULL_BACKFACE
    if (cullBackFace)
    {
        // Culling in homogeneus coordinates.
        // Read: "Triangle Scan Conversion using 2D Homogeneus Coordinates"
        //       by Marc Olano, Trey Greer
        float3x3 m = float3x3(vertices[0].xyw, vertices[1].xyw, vertices[2].xyw);
        if (determinant(m) > 0.0f)
            return true;
    }
#endif
    
#if ENABLE_CULL_FRUSTUM || ENABLE_CULL_SMALL_PRIMITIVES
    int verticesInFrontOfNearPlane = 0;
    
    for (uint i = 0U; i < 3U; i++)
    {
        if (vertices[i].w < 0.0f)
        {
            ++verticesInFrontOfNearPlane;
            
            // Flip the w so that any triangle that straddles the plane won't be projected onto
            // two sides of the screen
            vertices[i].w *= (-1.0f);
        }
        // Transform vertices[i].xy into the normalized 0..1 screen space
        // this is for the following stages ...
        vertices[i].xy /= vertices[i].w * 2.0f;
        vertices[i].xy += float2(0.5f, 0.5f);
    }
#endif
    
#if ENABLE_CULL_FRUSTUM
    if (verticesInFrontOfNearPlane == 3)
        return true;
    
    float minx = min(min(vertices[0].x, vertices[1].x), vertices[2].x);
    float miny = min(min(vertices[0].y, vertices[1].y), vertices[2].y);
    float maxx = max(max(vertices[0].x, vertices[1].x), vertices[2].x);
    float maxy = max(max(vertices[0].y, vertices[1].y), vertices[2].y);
    
    if ((maxx < 0.0f) || (maxy < 0.0f) || (minx > 1.0f) || (miny > 1.0f))
        return true;
#endif
    
    // not precise enough to handle more than 4 msaa samples
#if ENABLE_CULL_SMALL_PRIMITIVES
    if (verticesInFrontOfNearPlane == 0)
    {
        const uint SUBPIXEL_BITS = 8U;
        const uint SUBPIXEL_MASK = 0xFFU;
        const uint SUBPIXEL_SAMPLES = 1U << SUBPIXEL_BITS;
        
        /*
         Computing this in float-point is not precise enough.
         We switch to a 23.8 representation here which shold match the
         HW subpixel resolution.
         We use a 8-bit wide guard-band to avoid clipping. If
         a triangle is outside the guard-band, it will be ignored.
         That is, the actual viewport supported here is 31 bit, one bit is
         unused, and the guard band is 1 << 23 bit large (8388608 pixels)
         */
        
        int2 minBB = int2(1 << 30, 1 << 30);
        int2 maxBB = -minBB;
#if ENABLE_GUARD_BAND
        bool insideGuardBand = true;
#endif
        for (uint i = 0; i<3; i++)
        {
            float2 screenSpacePositionFP = vertices[i].xy * windowSize;
#if ENABLE_GUARD_BAND
            // Check if we should overflow after conversion
            if (screenSpacePositionFP.x < -(1 << 23) ||
                screenSpacePositionFP.x >(1 << 23) ||
                screenSpacePositionFP.y < -(1 << 23) ||
                screenSpacePositionFP.y >(1 << 23))
            {
                insideGuardBand = false;
            }
#endif
            // Scale based on distance from center to msaa sample point
            int2 screenSpacePosition = int2(screenSpacePositionFP * (SUBPIXEL_SAMPLES * samples));
            minBB = min(screenSpacePosition, minBB);
            maxBB = max(screenSpacePosition, maxBB);
        }
#if ENABLE_GUARD_BAND
        if (insideGuardBand)
#endif
        {
            const int SUBPIXEL_SAMPLE_CENTER = int(SUBPIXEL_SAMPLES / 2);
            const int SUBPIXEL_SAMPLE_SIZE = int(SUBPIXEL_SAMPLES - 1);
            /* Test is:
             Is the minimum of the bounding box right or above the sample
             point and is the width less than the pixel width in samples in
             one direction.
             
             This will also cull very long triagles which fall between
             multiple samples.
             */
            
            if ((((minBB.x & SUBPIXEL_MASK) > SUBPIXEL_SAMPLE_CENTER) &&
                 ((maxBB.x - ((minBB.x & ~SUBPIXEL_MASK) + SUBPIXEL_SAMPLE_CENTER)) < (SUBPIXEL_SAMPLE_SIZE))) ||
                (((minBB.y & SUBPIXEL_MASK) > SUBPIXEL_SAMPLE_CENTER) &&
                 ((maxBB.y - ((minBB.y & ~SUBPIXEL_MASK) + SUBPIXEL_SAMPLE_CENTER)) < (SUBPIXEL_SAMPLE_SIZE))))
            {
                return true;
            }
        }
    }
#endif
    
    return false;
}

void DoViewCulling(uint triangleIdGlobal, float3 pos0, float3 pos1, float3 pos2, float4x4 mvp, float2 viewSize, uint drawId, uint twoSided, device IndirectDrawArguments* indirectDrawArgs, device uint* filteredTriangles)
{
    /*

    // Apply model view projection transformation to vertices to clip space
    float4 vertexArray[3] = {
        mvp * float4(pos0,1),
        mvp * float4(pos1,1),
        mvp * float4(pos2,1)
    };
    
    // Perform visibility culling tests for this triangle
    bool cull = FilterTriangle(vertexArray, viewSize, 1, twoSided);
    
    // If the triangle passes all tests (not culled) then:
    if (!cull)
    {
        // Increase vertexCount for the current batch by 3
        uint prevVertexCount = atomic_fetch_add_explicit(&indirectDrawArgs[drawId].vertexCount, 3, memory_order_relaxed);
        uint groupOutputSlot = prevVertexCount / 3;
        uint startTriangle = indirectDrawArgs[drawId].startVertex / 3;
        
        // Store triangle ID in buffer for render
        filteredTriangles[startTriangle + groupOutputSlot] = triangleIdGlobal - startTriangle;
    }*/
}

/*
struct FilteredIndicesBufferData {
    device uint* data[NUM_CULLING_VIEWPORTS];
};

struct UncompactedDrawArgsData {
    device UncompactedDrawArguments* data[NUM_CULLING_VIEWPORTS];
};
*/

//[numthreads(256, 1, 1)]
kernel void stageMain(
    uint3 inGroupId                             [[thread_position_in_threadgroup]],
    uint3 groupId                               [[threadgroup_position_in_grid]],
    constant CSData& csData                     [[buffer(UPDATE_FREQ_NONE)]],
    constant CSDataPerFrame& csDataPerFrame     [[buffer(UPDATE_FREQ_PER_FRAME)]],
    constant SmallBatchData* batchData_rootcbv  [[buffer(UPDATE_FREQ_USER)]]
)
{
    threadgroup atomic_uint workGroupOutputSlot[NUM_CULLING_VIEWPORTS];
    threadgroup atomic_uint workGroupIndexCount[NUM_CULLING_VIEWPORTS];
    
    if (inGroupId.x == 0)
    {
        for (uint i = 0; i < NUM_CULLING_VIEWPORTS; ++i)
            atomic_store_explicit(&workGroupIndexCount[i], 0, memory_order_relaxed);
    }

    threadgroup_barrier(mem_flags::mem_threadgroup);
    
    bool cull[NUM_CULLING_VIEWPORTS];
    uint threadOutputSlot[NUM_CULLING_VIEWPORTS];

    for (uint i = 0; i < NUM_CULLING_VIEWPORTS; ++i)
    {
        threadOutputSlot[i] = 0;
        cull[i] = true;
    }
    
    uint batchMeshIndex = batchData_rootcbv[groupId.x].meshIndex;
    uint batchInputIndexOffset = (csData.meshConstantsBuffer[batchMeshIndex].indexOffset + batchData_rootcbv[groupId.x].indexOffset);
    bool twoSided = (csData.meshConstantsBuffer[batchMeshIndex].twoSided == 1);

    uint indices[3] = { 0, 0, 0 };
    if (inGroupId.x < batchData_rootcbv[groupId.x].faceCount)
    {
        indices[0] = csData.indexDataBuffer[inGroupId.x * 3 + 0 + batchInputIndexOffset];
        indices[1] = csData.indexDataBuffer[inGroupId.x * 3 + 1 + batchInputIndexOffset];
        indices[2] = csData.indexDataBuffer[inGroupId.x * 3 + 2 + batchInputIndexOffset];
        
        float4 vert[3] =
        {
            float4(csData.vertexDataBuffer[indices[0]].position, 1),
            float4(csData.vertexDataBuffer[indices[1]].position, 1),
            float4(csData.vertexDataBuffer[indices[2]].position, 1)
        };
        
        for (uint i = 0; i < NUM_CULLING_VIEWPORTS; ++i)
        {
            float4x4 worldViewProjection = csDataPerFrame.uniforms.transform[i].mvp;
            float4 vertices[3] =
            {
                worldViewProjection * vert[0],
                worldViewProjection * vert[1],
                worldViewProjection * vert[2]
            };
            
            CullingViewPort viewport = csDataPerFrame.uniforms.cullingViewports[i];
            cull[i] = FilterTriangle(indices, vertices, !twoSided, viewport.windowSize, viewport.sampleCount);
            if (!cull[i])
                threadOutputSlot[i] = atomic_fetch_add_explicit(&workGroupIndexCount[i], 3, memory_order_relaxed);
        }
    }

    threadgroup_barrier(mem_flags::mem_threadgroup);
    
    uint accumBatchDrawIndex = batchData_rootcbv[groupId.x].accumDrawIndex;
    
    if (inGroupId.x == 0)
    {
        for (uint i = 0; i < NUM_CULLING_VIEWPORTS; ++i)
        {
            uint index = atomic_load_explicit(&workGroupIndexCount[i], memory_order_relaxed);
            atomic_store_explicit(&workGroupOutputSlot[i],
                                  atomic_fetch_add_explicit((device atomic_uint*)&csDataPerFrame.uncompactedDrawArgsRW[i][accumBatchDrawIndex].numIndices, index, memory_order_relaxed),
                                  memory_order_relaxed);
        }
    }

	threadgroup_barrier(mem_flags::mem_device | mem_flags::mem_threadgroup);
    
    for (uint i = 0; i < NUM_CULLING_VIEWPORTS; ++i)
    {
        if (!cull[i])
        {
            uint index = atomic_load_explicit(&workGroupOutputSlot[i], memory_order_relaxed);
            
            csDataPerFrame.filteredIndicesBuffer[i][index + batchData_rootcbv[groupId.x].outputIndexOffset + threadOutputSlot[i] + 0] = indices[0];
            csDataPerFrame.filteredIndicesBuffer[i][index + batchData_rootcbv[groupId.x].outputIndexOffset + threadOutputSlot[i] + 1] = indices[1];
            csDataPerFrame.filteredIndicesBuffer[i][index + batchData_rootcbv[groupId.x].outputIndexOffset + threadOutputSlot[i] + 2] = indices[2];
        }
    }
    
    if (inGroupId.x == 0 && groupId.x == batchData_rootcbv[groupId.x].drawBatchStart)
    {
        uint outIndexOffset = batchData_rootcbv[groupId.x].outputIndexOffset;
        
        for (uint i = 0; i < NUM_CULLING_VIEWPORTS; ++i)
        {
            csDataPerFrame.uncompactedDrawArgsRW[i][accumBatchDrawIndex].startIndex = outIndexOffset;
            csDataPerFrame.uncompactedDrawArgsRW[i][accumBatchDrawIndex].materialID = csData.meshConstantsBuffer[batchMeshIndex].materialID;
        }
    }
}
