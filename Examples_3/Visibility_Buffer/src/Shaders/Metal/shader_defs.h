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
#define BATCH_COUNT 16384

// SCENE_BATCHES specifies the amount of batches needed to filter all the triangles in the scene.
// Different scenes might need different SCENE_BATCHES values.
#define SCENE_BATCHES 32768 // San Miguel values.
#define NUM_BATCHES (SCENE_BATCHES / BATCH_COUNT)

// This defines the amount of viewports that are going to be culled in parallel.
#define NUM_CULLING_VIEWPORTS 2
#define VIEW_SHADOW 0
#define VIEW_CAMERA 1

// This value defines the amount of threads per group that will be used to clear the
// indirect draw buffers.
#define CLEAR_THREAD_COUNT 256

// Size for the material buffer assuming each draw call uses one material index.
#define MATERIAL_BUFFER_SIZE 256

#if MTL_SHADER

struct CullingViewPort
{
	metal::packed_float2 windowSize;
	metal::uint sampleCount;
	metal::uint _pad0;
};

struct Transform
{
	metal::float4x4 mvp;
	metal::float4x4 invVP;
	metal::float4x4 vp;
	metal::float4x4 projection;
};

struct PerFrameConstants {
	Transform transform[NUM_CULLING_VIEWPORTS];
	CullingViewPort cullingViewports[NUM_CULLING_VIEWPORTS];
	//========================================
	metal::packed_float4 camPos;
	//========================================
	metal::packed_float4 lightDir;
	//========================================
	metal::packed_float2 twoOverRes;
	float esmControl;
	metal::uint _pad0;
	//========================================
};

struct LightData
{
	metal::packed_float3 position;
	metal::packed_float3 color;
	metal::uint _pad0;
	metal::uint _pad1;
};

#else

struct CullingViewPort
{
	float2 windowSize;
	uint sampleCount;
	uint _pad0;
};

struct Transform
{
	mat4 mvp;
	mat4 invVP;
	mat4 vp;
	mat4 projection;
};

struct PerFrameConstants {
	Transform transform[NUM_CULLING_VIEWPORTS];
	CullingViewPort cullingViewports[NUM_CULLING_VIEWPORTS];
	//========================================
	float4 camPos;
	//========================================
	float4 lightDir;
	//========================================
	float2 twoOverRes;
	float esmControl;
	uint _pad0;
	//========================================
};

struct LightData
{
	float3 position;
	float3 color;
	uint _pad0;
	uint _pad1;
};

#endif

#endif
