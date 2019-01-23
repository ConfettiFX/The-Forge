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

#ifndef Geometry_h
#define Geometry_h

#include "../../../Common_3/Renderer/IRenderer.h"
#include "../../../Common_3/Renderer/ResourceLoader.h"

#if defined(METAL)
#include "Shaders/Metal/shader_defs.h"
#elif defined(DIRECT3D12) || defined(_DURANGO)
#define NO_HLSL_DEFINITIONS
#include "Shaders/D3D12/shader_defs.h"
#elif defined(VULKAN)
#define NO_GLSL_DEFINITIONS
#include "Shaders/Vulkan/shader_defs.h"
#endif

#define MAX_PATH 260

// Type definitions

typedef struct SceneVertexPos
{
	float x, y, z;
} SceneVertexPos;

struct Vertex
{
	float3 mPos;
	float3 mNormal;
	float2 mUv;
};

typedef struct SceneVertexTexCoord
{
#if defined(METAL) || defined(__linux__)
	float u, v;    // texture coords
#else
	uint32_t texCoord;
#endif
} SceneVertexTexCoord;

typedef struct SceneVertexNormal
{
#if defined(METAL) || defined(__linux__)
	float nx, ny, nz;    // normals
#else
	uint32_t normal;
#endif
} SceneVertexNormal;

typedef struct SceneVertexTangent
{
#if defined(METAL) || defined(__linux__)
	float tx, ty, tz;    // tangents
#else
	uint32_t tangent;
#endif
} SceneVertexTangent;

typedef struct ClusterCompact
{
	uint32_t triangleCount;
	uint32_t clusterStart;
} ClusterCompact;

typedef struct Cluster
{
	float3 aabbMin, aabbMax;
	float3 coneCenter, coneAxis;
	float  coneAngleCosine;
	float  distanceFromCamera;
	bool   valid;
} Cluster;

typedef struct AABoundingBox
{
	float4 Center;     // Center of the box.
	float4 Extents;    // Distance from the center to each side.

	float4 minPt;
	float4 maxPt;

	float4 corners[8];
} AABoundingBox;

typedef struct MeshIn
{
#if defined(METAL)
	uint32_t startVertex;
	uint32_t triangleCount;
#else
	uint32_t startIndex;
	uint32_t indexCount;
#endif
	uint32_t        vertexCount;
	float3          minBBox, maxBBox;
	uint32_t        clusterCount;
	ClusterCompact* clusterCompacts;
	Cluster*        clusters;
	uint32_t        materialId;

	AABoundingBox AABB;

	Buffer* pVertexBuffer;
	Buffer* pIndexBuffer;

} MeshIn;

typedef struct Material
{
	bool twoSided;
	bool alphaTested;
} Material;

typedef struct Scene
{
	uint32_t                             numMeshes;
	uint32_t                             numMaterials;
	uint32_t                             totalTriangles;
	uint32_t                             totalVertices;
	MeshIn*                              meshes;
	Material*                            materials;
	tinystl::vector<SceneVertexPos>      positions;
	tinystl::vector<SceneVertexTexCoord> texCoords;
	tinystl::vector<SceneVertexNormal>   normals;
	tinystl::vector<SceneVertexTangent>  tangents;
	char**                               textures;
	char**                               normalMaps;
	char**                               specularMaps;

	tinystl::vector<uint32_t> indices;
} Scene;

typedef struct FilterBatchData
{
#if defined(METAL)
	uint32_t triangleCount;
	uint32_t triangleOffset;
	uint32_t meshIdx;
	uint32_t twoSided;
#else
	uint     meshIndex;            // Index into meshConstants
	uint     indexOffset;          // Index relative to the meshConstants[meshIndex].indexOffset
	uint     faceCount;            // Number of faces in this small batch
	uint     outputIndexOffset;    // Offset into the output index buffer
	uint     drawBatchStart;       // First slot for the current draw call
	uint     accumDrawIndex;
	uint     _pad0;
	uint     _pad1;
#endif
} FilterBatchData;

typedef struct FilterBatchChunk
{
	FilterBatchData* batches;
	uint32_t         currentBatchCount;
	uint32_t         currentDrawCallCount;
#if defined(METAL)
	Buffer* batchDataBuffer;    // GPU buffer containing all batch data
#else
#endif
} FilterBatchChunk;

// Exposed functions

Scene* loadScene(const char* fileName, float scale, float offsetX, float offsetY, float offsetZ);
void   removeScene(Scene* scene);
void   CreateAABB(const Scene* pScene, MeshIn* mesh);
void   CreateClusters(bool twoSided, const Scene* pScene, MeshIn* mesh);

void loadModel(const tinystl::string& FileName, Buffer*& pVertexBuffer, uint& vertexCount, Buffer*& IndexBuffer, uint& indexCount);

#if defined(METAL)
void addClusterToBatchChunk(
	const ClusterCompact* cluster, const MeshIn* mesh, uint32_t meshIdx, bool isTwoSided, FilterBatchChunk* batchChunk);
#else
void addClusterToBatchChunk(
	const ClusterCompact* cluster, uint batchStart, uint accumDrawCount, uint accumNumTriangles, int meshIndex,
	FilterBatchChunk* batchChunk);
#endif
void createCubeBuffers(Renderer* pRenderer, CmdPool* cmdPool, Buffer** outVertexBuffer, Buffer** outIndexBuffer);
void destroyBuffers(Renderer* pRenderer, Buffer* outVertexBuffer, Buffer* outIndexBuffer);

#endif
