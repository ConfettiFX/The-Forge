/*
 * Copyright (c) 2017-2022 The Forge Interactive Inc.
 *
 * This is a part of Aura.
 * 
 * This file(code) is licensed under a 
 * Creative Commons Attribution-NonCommercial 4.0 International License 
 *
 *   (https://creativecommons.org/licenses/by-nc/4.0/legalcode) 
 *
 * Based on a work at https://github.com/ConfettiFX/The-Forge.
 * You may not use the material for commercial purposes.
 *
*/

#ifndef Geometry_h
#define Geometry_h

#include "../../../Common_3/Utilities/ThirdParty/OpenSource/EASTL/vector.h"
#include "../../../Common_3/Graphics/Interfaces/IGraphics.h"
#include "../../../Common_3/Resources/ResourceLoader/Interfaces/IResourceLoader.h"

#define NO_FSL_DEFINITIONS
#include "Shaders/FSL/shader_defs.h.fsl"

#define MAX_PATH 260

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
void createCubeBuffers(Renderer* pRenderer, CmdPool* cmdPool, Buffer** outVertexBuffer, Buffer** outIndexBuffer);

#endif
