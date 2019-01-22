/*
 * Copyright (c) 2018-2019 Confetti Interactive Inc.
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

// These are the tests performed per triangle. They can be toggled on/off setting this define macros to 0/1.
#define ENABLE_CULL_BACKFACE         1
#define ENABLE_CULL_FRUSTUM          1
#define ENABLE_CULL_SMALL_PRIMITIVES 1
#define ENABLE_GUARD_BAND				0

struct SceneVertexPos
{
    packed_float3 position;
};

struct SceneVertexAttr
{
    packed_float2 texCoord;
    packed_float3 normal;
    packed_float3 tangents;
};

struct BatchData
{
    uint triangleCount;
    uint triangleOffset;
    uint drawId;
    uint twoSided;
};

struct PerFrameUniforms {
    float4x4 mvp;
    float4x4 projection;
    float4x4 invVP;
    uint numBatches;
    uint numLights;
    packed_float2 windowSize;
    packed_float2 shadowMapSize;
    float4x4 lightMVP;
    packed_float3 normalizedDirToLight;
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
bool FilterTriangle(float4 vertices[3], float2 windowSize, uint samples, uint twoSided)
{
#if ENABLE_CULL_BACKFACE
    if (twoSided==0)
    {
        // Culling in homogeneus coordinates.
        // Read: "Triangle Scan Conversion using 2D Homogeneus Coordinates"
        //       by Marc Olano, Trey Greer
        float3x3 m = float3x3(vertices[0].xyw, vertices[1].xyw, vertices[2].xyw);
        if (determinant(m) > 0)
            return true;
    }
#endif
    
#if ENABLE_CULL_FRUSTUM || ENABLE_CULL_SMALL_PRIMITIVES
    int verticesInFrontOfNearPlane = 0;
 
    for (uint i=0; i<3; i++)
    {
        if (vertices[i].w < 0)
        {
            ++verticesInFrontOfNearPlane;
            
            // Flip the w so that any triangle that stradles the plane won't be projected onto
            // two sides of the screen
            vertices[i].w *= (-1.0);
        }
		// Transform vertices[i].xy into the normalized 0..1 screen space
		// this is for the following stages ...
        vertices[i].xy /= vertices[i].w * 2;
        vertices[i].xy += float2(0.5,0.5);
    }
#endif
    
#if ENABLE_CULL_FRUSTUM
    if (verticesInFrontOfNearPlane == 3)
        return true;
    
    float minx = min(min(vertices[0].x, vertices[1].x), vertices[2].x);
    float miny = min(min(vertices[0].y, vertices[1].y), vertices[2].y);
    float maxx = max(max(vertices[0].x, vertices[1].x), vertices[2].x);
    float maxy = max(max(vertices[0].y, vertices[1].y), vertices[2].y);
    
    if ((maxx < 0) || (maxy < 0) || (minx > 1) || (miny > 1))
        return true;
#endif
    
// not precise enough to handle more than 4 msaa samples
#if ENABLE_CULL_SMALL_PRIMITIVES
    if (verticesInFrontOfNearPlane == 0)
    {
        const uint SUBPIXEL_BITS = 8;
        const uint SUBPIXEL_MASK = 0xFF;
        const uint SUBPIXEL_SAMPLES = 1 << SUBPIXEL_BITS;
        
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
        for (uint i=0; i<3; i++)
        {
            float2 screenSpacePositionFP = vertices[i].xy * windowSize;
#if ENABLE_GUARD_BAND			
            // Check if we should overflow after conversion
            if (screenSpacePositionFP.x < -(1<<23) ||
                screenSpacePositionFP.x >  (1<<23) ||
                screenSpacePositionFP.y < -(1<<23) ||
                screenSpacePositionFP.y >  (1<<23))
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
            const uint SUBPIXEL_SAMPLE_CENTER = SUBPIXEL_SAMPLES / 2;
            const uint SUBPIXEL_SAMPLE_SIZE = SUBPIXEL_SAMPLES - 1;
            /* Test is:
             Is the minimum of the bounding box right or above the sample
             point and is the width less than the pixel width in samples in
             one direction.
             
             This will also cull very long triagles which fall between
             multiple samples.
             */
            
            if (any( ((minBB & SUBPIXEL_MASK) > SUBPIXEL_SAMPLE_CENTER) &&
                     ((maxBB - ((minBB & ~SUBPIXEL_MASK) + SUBPIXEL_SAMPLE_CENTER)) < (SUBPIXEL_SAMPLE_SIZE))))
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
    }
}

//[numthreads(256, 1, 1)]
kernel void stageMain(uint inGroupId [[thread_position_in_threadgroup]],
                      uint groupId [[threadgroup_position_in_grid]],
                      constant SceneVertexPos* vertexPos [[buffer(0)]],
                      constant BatchData* perBatch [[buffer(1)]],
                      device uint* filteredTrianglesCamera [[buffer(2)]],
                      device uint* filteredTrianglesShadow [[buffer(3)]],
                      device IndirectDrawArguments* indirectDrawArgsCamera [[buffer(4)]],
                      device IndirectDrawArguments* indirectDrawArgsShadow [[buffer(5)]],
                      constant PerFrameConstants& uniforms [[buffer(6)]])
{
    // Don't run anything if we run out of triangles
    if (inGroupId >= perBatch[groupId].triangleCount)
        return;
    
    // Starting triangle to start reading triangles from
    uint inputTriangleOffset = perBatch[groupId].triangleOffset;
    uint drawId = perBatch[groupId].drawId;
    uint twoSided = perBatch[groupId].twoSided;
    
    uint triangleIdGlobal = inGroupId + inputTriangleOffset;
    
    // Since we are not using indexed geometry, vertexId = triangleId x 3
    uint vertexIdGlobal = triangleIdGlobal*3;
    
    // Load triangle vertex data from the vertex buffer
    SceneVertexPos v0 = vertexPos[vertexIdGlobal];
    SceneVertexPos v1 = vertexPos[vertexIdGlobal+1];
    SceneVertexPos v2 = vertexPos[vertexIdGlobal+2];
    
    // Perform culling on all the views
    DoViewCulling(triangleIdGlobal, v0.position, v1.position, v2.position, uniforms.transform[VIEW_CAMERA].mvp, uniforms.cullingViewports[VIEW_CAMERA].windowSize, drawId, twoSided, indirectDrawArgsCamera, filteredTrianglesCamera);
    DoViewCulling(triangleIdGlobal, v0.position, v1.position, v2.position, uniforms.transform[VIEW_SHADOW].mvp, uniforms.cullingViewports[VIEW_SHADOW].windowSize, drawId, twoSided, indirectDrawArgsShadow, filteredTrianglesShadow);
}

