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

#ifndef _SHADER_DEFS_H
#define _SHADER_DEFS_H

#define LIGHT_COUNT 128
#define LIGHT_SIZE 150.0f

#define LIGHT_CLUSTER_WIDTH 8
#define LIGHT_CLUSTER_HEIGHT 8

#define LIGHT_CLUSTER_COUNT_POS(ix, iy) ( (iy*LIGHT_CLUSTER_WIDTH)+ix )
#define LIGHT_CLUSTER_DATA_POS(il, ix, iy) ( LIGHT_CLUSTER_COUNT_POS(ix, iy)*LIGHT_COUNT + il )

// This defines the amount of triangles that will be processed in parallel by the
// compute shader in the triangle filtering process.
// Should be a multiple of the wavefront size
#define CLUSTER_SIZE 256

// BATCH_COUNT limits the amount of triangle batches we can process on the GPU at the same time.
#define BATCH_COUNT 2048

// This defines the amount of viewports that are going to be culled in parallel.
#define NUM_CULLING_VIEWPORTS 2
#define VIEW_SHADOW 0
#define VIEW_CAMERA 1

// This value defines the amount of threads per group that will be used to clear the
// indirect draw buffers.
#define CLEAR_THREAD_COUNT 256

// The following value defines the maximum amount of indirect draw calls that will be
// drawn at once. This value depends on the number of submeshes or individual objects
// in the scene. Changing a scene will require to change this value accordingly.
#define MAX_DRAWS_INDIRECT 256

// Size for the material buffer assuming each draw call uses one material index.
#define MATERIAL_BUFFER_SIZE (MAX_DRAWS_INDIRECT * 2 * NUM_CULLING_VIEWPORTS)

// The following values point to the position in the indirect draw buffer that holds the
// number of draw calls to draw after triangle filtering and batch compaction.
// This value number is stored in the last position of the indirect draw buffer.
// So it depends on MAX_DRAWS_INDIRECT
#define INDIRECT_DRAW_ARGUMENTS_STRUCT_NUM_ELEMENTS 8
#define DRAW_COUNTER_SLOT_POS 				((MAX_DRAWS_INDIRECT-1)*INDIRECT_DRAW_ARGUMENTS_STRUCT_NUM_ELEMENTS)
#define DRAW_COUNTER_SLOT_OFFSET_IN_BYTES	(DRAW_COUNTER_SLOT_POS*sizeof(uint))

// compute units
#define UNIT_UNCOMPACTED_ARGS             0
#define UNIT_MATERIAL_PROPS               1
#define UNIT_VERTEX_DATA                  2
#define UNIT_INDEX_DATA                   3
#define UNIT_MESH_CONSTANTS               4
#define UNIT_BATCH_DATA_CBV               5
#define UNIT_UNIFORMS_CBV                 6
#define UNIT_INDIRECT_MATERIAL_RW         7
#define UNIT_INDIRECT_DRAW_ARGS_ALPHA_RW  8
#define UNIT_INDIRECT_DRAW_ARGS_RW        9
#define UNIT_UNCOMPACTED_ARGS_RW          10
#define UNIT_INDEX_DATA_RW                11
#define UNIT_ICB_RW                       12
#define UNIT_DRAWID_RW                    13
#define UNIT_POSITION                     14
#define UNIT_TEXCOORD                     15
#define UNIT_DIFFUSEMAPS                  16

// icb units
#define UNIT_VBPASS_POSITION              30
#define UNIT_VBPASS_TEXCOORD              29
#define UNIT_VBPASS_NORMAL                28
#define UNIT_VBPASS_TANGENT               27
#define UNIT_VBPASS_TEXTURES              0
#define UNIT_VBPASS_UNIFORMS              1
#define UINT_VBPASS_DRAWID                2
#define UINT_VBPASS_MAX                   7

#if MTL_SHADER
typedef packed_float2 shader_packed_float2;
typedef packed_float3 shader_packed_float3;
typedef packed_float4 shader_packed_float4;
typedef float4x4 mat4;
#define INLINE

uint NonUniformResourceIndex(uint index) {
    while (true) {
        uint currentIndex = simd_broadcast_first(index);
        if (currentIndex == index)
            return index;
    }
    return 0;
}
#else
typedef float2 shader_packed_float2;
typedef float3 shader_packed_float3;
typedef float4 shader_packed_float4;
#define INLINE inline
#endif

INLINE uint BaseMaterialBuffer(bool alpha, uint viewID)
{
    return (viewID * 2 + (alpha ? 0 : 1)) * MAX_DRAWS_INDIRECT;
}

struct CullingViewPort
{
    shader_packed_float2  windowSize;
    uint                  sampleCount;
    uint                  _pad0;
};

struct Transform
{
    mat4 mvp;
    mat4 invVP;
    mat4 vp;
    mat4 projection;
};

struct PerFrameConstants {
    Transform       transform[NUM_CULLING_VIEWPORTS];
    CullingViewPort cullingViewports[NUM_CULLING_VIEWPORTS];
    //========================================
    shader_packed_float4 camPos;
    //========================================
    shader_packed_float4 lightDir;
    //========================================
    shader_packed_float4 lightColor;
    //========================================
    shader_packed_float2 CameraPlane; //x : near, y : far
    uint lightingMode;
    uint outputMode;
    //========================================
    shader_packed_float2 twoOverRes;
    float   esmControl;
    //========================================
    uint    _pad0;
};

struct LightData
{
    shader_packed_float3  position;
    shader_packed_float3  color;
    uint    _pad0;
    uint    _pad1;
};

struct UncompactedDrawArguments
{
    uint numIndices;
    uint startIndex;
    uint materialID;
    uint pad_;
};

struct SmallBatchData
{
    uint meshIndex;      // Index into meshConstants
    uint indexOffset;      // Index relative to the meshConstants[meshIndex].indexOffset
    uint faceCount;      // Number of faces in this small batch
    uint outputIndexOffset; // Offset into the output index buffer
    uint drawBatchStart;    // First slot for the current draw call
    uint accumDrawIndex;
    uint _pad0;
    uint _pad1;
};

struct MeshConstants
{
    uint    faceCount;
    uint    indexOffset;
    uint    materialID;
    uint    twoSided; //0 or 1
};

struct RootConstant
{
	uint drawId;
};

#endif
