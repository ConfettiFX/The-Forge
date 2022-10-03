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

#define MAX_MESH_NAME_LEN 128

#include "../../../../Common_3/Utilities/Math/MathTypes.h"
//EA stl
#include "../../../../Common_3/Utilities/ThirdParty/OpenSource/EASTL/string.h"
#include "../../../../Common_3/Utilities/ThirdParty/OpenSource/EASTL/vector.h"
#include "../../../../Common_3/Utilities/ThirdParty/OpenSource/EASTL/unordered_set.h"
#include "../../../../Common_3/Utilities/ThirdParty/OpenSource/EASTL/unordered_map.h"

#include "../../../../Common_3/Graphics/Interfaces/IGraphics.h"
#include "../../../../Common_3/Resources/ResourceLoader/Interfaces/IResourceLoader.h"

#define NO_FSL_DEFINITIONS
#include "Shaders/FSL/Shader_Defs.h.fsl"
#include "Shaders/FSL/ASMShader_Defs.h.fsl"
#include "Shaders/FSL/SDF_Constant.h.fsl"

namespace eastl
{
	template <>
	struct has_equality<vec3> : eastl::false_type {};
}

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

struct FilterBatchData
{
	uint meshIndex; // Index into meshConstants
	uint indexOffset; // Index relative to meshConstants[meshIndex].indexOffset
	uint faceCount; // Number of faces in this small batch
	uint outputIndexOffset; // Offset into the output index buffer
	uint drawBatchStart; // First slot for the current draw call
	uint accumDrawIndex;
	uint _pad0;
	uint _pad1;
};

struct FilterBatchChunk
{
	uint32_t			currentBatchCount;
	uint32_t			currentDrawCallCount;
};

/************************************************************************/
// Meshes
/************************************************************************/
struct SDFVolumeData;
struct MeshInfo;

typedef uint32_t MaterialFlags;
typedef eastl::vector<SDFVolumeData*> BakedSDFVolumeInstances;
typedef bool(*GenerateVolumeDataFromFileFunc) (SDFVolumeData**, MeshInfo*);

enum MaterialFlagBits
{
	MATERIAL_FLAG_NONE              = 0,
	MATERIAL_FLAG_TWO_SIDED         = (1 << 0),
	MATERIAL_FLAG_ALPHA_TESTED      = (1 << 1),
	MATERIAL_FLAG_DOUBLE_VOXEL_SIZE = (1 << 2),
	MATERIAL_FLAG_ALL = MATERIAL_FLAG_TWO_SIDED | MATERIAL_FLAG_ALPHA_TESTED | MATERIAL_FLAG_DOUBLE_VOXEL_SIZE
};

struct MeshInfo
{
	const char*   name = NULL;
	const char*   password = NULL;
	MaterialFlags materialFlags = MATERIAL_FLAG_NONE;
	float         twoSidedWorldSpaceBias = 0.0f;
	bool          sdfGenerated = false;

    MeshInfo() {}
    MeshInfo(const char* name, const char* password, MaterialFlags materialFlags, float twoSidedWorldSpaceBias) :
        name(name),
        password(password),
        materialFlags(materialFlags),
        twoSidedWorldSpaceBias(twoSidedWorldSpaceBias),
        sdfGenerated(false) 
    {}
};

typedef struct Scene
{
	Geometry*      geom;
	MaterialFlags* materialFlags;
	char**         textures;
	char**         normalMaps;
	char**         specularMaps;
} Scene;

struct SDFMesh
{
	Geometry* pGeometry = NULL;
	MeshInfo* pSubMeshesInfo = NULL;
	uint32_t* pSubMeshesGroupsSizes = NULL;
	uint32_t* pSubMeshesIndices = NULL;
	uint32_t  numSubMeshesGroups = 0;
	uint32_t  numGeneratedSDFMeshes = 0;
};

void adjustAABB(AABB* ownerAABB, const vec3& point);
void adjustAABB(AABB* ownerAABB, const AABB& otherAABB);
void alignAABB(AABB* ownerAABB, float alignment);
vec3 calculateAABBSize(const AABB* ownerAABB);
vec3 calculateAABBExtent(const AABB* ownerAABB);
vec3 calculateAABBCenter(const AABB* ownerAABB);

void createClusters(bool twoSided, const Scene* scene, IndirectDrawIndexArguments* draw, ClusterContainer* subMesh);
void destroyClusters(ClusterContainer* pMesh);
void addClusterToBatchChunk(const ClusterCompact* cluster, uint batchStart, uint accumDrawCount, uint accumNumTriangles, int meshIndex, 
	FilterBatchChunk* batchChunk, FilterBatchData* batches);

Scene* loadScene(const char* fileName, SyncToken* token, float scale, float offsetX, float offsetY, float offsetZ);
void removeScene(Scene* scene);
	
void loadBakedSDFData(SDFMesh* outMesh, uint32_t startIdx, bool generateSDFVolumeData, BakedSDFVolumeInstances& sdfVolumeInstances,
	GenerateVolumeDataFromFileFunc generateVolumeDataFromFileFunc);

#endif
