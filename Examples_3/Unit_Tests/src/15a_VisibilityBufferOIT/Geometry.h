/*
 * Copyright (c) 2017-2022 The Forge Interactive Inc.
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

#include "../../../../Common_3/Utilities/ThirdParty/OpenSource/EASTL/vector.h"
#include "../../../../Common_3/Graphics/Interfaces/IGraphics.h"
#include "../../../../Common_3/Resources/ResourceLoader/Interfaces/IResourceLoader.h"

#define NO_FSL_DEFINITIONS
#include "Shaders/FSL/shader_defs.h.fsl"

// Type definitions

typedef struct SceneVertexPos
{
	float x, y, z;
} SceneVertexPos;

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

typedef struct ClusterContainer
{
	uint32_t        clusterCount;
	ClusterCompact* clusterCompacts;
	Cluster*        clusters;
} ClusterContainer;

typedef struct Material
{
	bool twoSided;
	bool alphaTested;
	bool transparent;
} Material;

typedef struct Scene
{
	Geometry*                          geom;
	Material*                          materials;
	char**                             textures;
	char**                             normalMaps;
	char**                             specularMaps;
} Scene;

typedef struct FilterBatchData
{
#if 0 //defined(METAL)
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
	uint32_t         currentBatchCount;
	uint32_t         currentDrawCallCount;
} FilterBatchChunk;

// Exposed functions

Scene* loadScene(const char* pFileName, float scale, float offsetX, float offsetY, float offsetZ);
void   removeScene(Scene* scene);
void   createClusters(bool twoSided, const Scene* pScene, IndirectDrawIndexArguments* draw, ClusterContainer* mesh);
void   destroyClusters(ClusterContainer* mesh);

void addClusterToBatchChunk(
	const ClusterCompact* cluster, uint batchStart, uint accumDrawCount, uint accumNumTriangles, int meshIndex,
	FilterBatchChunk* batchChunk, FilterBatchData* batches);
void createCubeBuffers(Renderer* pRenderer, Buffer** outVertexBuffer, Buffer** outIndexBuffer);

#endif
