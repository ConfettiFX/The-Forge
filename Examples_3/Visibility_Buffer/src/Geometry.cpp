/*
 * Copyright (c) 2018 Confetti Interactive Inc.
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

#include "Geometry.h"

#include "../../../Common_3/ThirdParty/OpenSource/TinySTL/unordered_set.h"

#include "../../../Common_3/ThirdParty/OpenSource/assimp/3.3.1/include/assimp/cimport.h"
#include "../../../Common_3/ThirdParty/OpenSource/assimp/3.3.1/include/assimp/scene.h"
#include "../../../Common_3/ThirdParty/OpenSource/assimp/3.3.1/include/assimp/postprocess.h"
#include "../../../Common_3/OS/Interfaces/IFileSystem.h"
#include "../../../Common_3/OS/Interfaces/ILogManager.h"
#include "../../../Common_3/OS/Interfaces/IMemoryManager.h"
#include "../../../Common_3/OS/Core/Compiler.h"

static void SetAlphaTestMaterials(tinystl::unordered_set<String>& mats)
{
	// San Miguel
	mats.insert("aglaonema_Leaf");
	mats.insert("brevipedunculata_Leaf1");
	mats.insert("Azalea_1_blattg1");
	mats.insert("Azalea_1_Blutenb");
	mats.insert("Azalea_1_leafcal");
	mats.insert("Azalea_2_blattg1");
	mats.insert("Azalea_2_Blutenb");
	mats.insert("Chusan_Palm_1_Leaf");
	mats.insert("Fern_1_Fan");
	mats.insert("Fern_3_Leaf");
	mats.insert("Ficus_1_Leaf1");
	mats.insert("Geranium_1_Leaf");
	mats.insert("Geranium_1_blbl1");
	mats.insert("Geranium_1_blbl2");
	mats.insert("Geranium_1_Kelchbl");
	mats.insert("Hoja_Seca_2A");
	mats.insert("Hoja_Seca_2B");
	mats.insert("Hoja_Seca_2C");
	mats.insert("Hoja_Verde_A");
	mats.insert("Hoja_Verde_B");
	mats.insert("Hojas_Rojas_top");
	mats.insert("hybrids_blossom");
	mats.insert("hybrids_Leaf");
	mats.insert("Ivy_1_Leaf");
	mats.insert("Leave_A_a");
	mats.insert("Leave_A_b");
	mats.insert("Leave_A_c");
	mats.insert("Mona_Lisa_1_Leaf1");
	mats.insert("Mona_Lisa_1_Leaf2");
	mats.insert("Mona_Lisa_2_Leaf1");
	mats.insert("Mona_Lisa_1_petal11");
	mats.insert("Mona_Lisa_1_petal12");
	mats.insert("paniceum_Leaf");
	mats.insert("Pansy_1_blblbac");
	mats.insert("Pansy_1_Leaf");
	mats.insert("Pansy_1_Leafcop");
	mats.insert("Pansy_1_Leafsma");
	mats.insert("Poinsettia_1_Leaf");
	mats.insert("Poinsettia_1_redleaf");
	mats.insert("Poinsettia_1_smallre");
	mats.insert("Rose_1_Blatt2");
	mats.insert("Rose_1_Blutenb");
	mats.insert("Rose_1_Blatt1_");
	mats.insert("Rose_1_Kelchbl");
	mats.insert("Rose_2__Blutenb");
	mats.insert("Rose_2__Kelchbl");
	mats.insert("Rose_2_Blatt1_");
	mats.insert("Rose_3_Blutenb");
	mats.insert("Rose_3_Blatt2");
	mats.insert("zebrina_Leaf");
}

static void SetTwoSidedMaterials(tinystl::unordered_set<String>& mats)
{
	// San Miguel
	mats.insert("aglaonema_Leaf");
	mats.insert("brevipedunculata_Leaf1");
	mats.insert("Azalea_1_blattg1");
	mats.insert("Azalea_1_Blutenb");
	mats.insert("Azalea_1_leafcal");
	mats.insert("Azalea_2_blattg1");
	mats.insert("Azalea_2_Blutenb");
	mats.insert("Chusan_Palm_1_Leaf");
	mats.insert("Fern_1_Fan");
	mats.insert("Fern_3_Leaf");
	mats.insert("Ficus_1_Leaf1");
	mats.insert("Geranium_1_Leaf");
	mats.insert("Geranium_1_blbl1");
	mats.insert("Geranium_1_blbl2");
	mats.insert("Geranium_1_Kelchbl");
	mats.insert("Hoja_Seca_2A");
	mats.insert("Hoja_Seca_2B");
	mats.insert("Hoja_Seca_2C");
	mats.insert("Hoja_Verde_A");
	mats.insert("Hoja_Verde_B");
	mats.insert("Hojas_Rojas_top");
	mats.insert("hybrids_blossom");
	mats.insert("hybrids_Leaf");
	mats.insert("Ivy_1_Leaf");
	mats.insert("Leave_A_a");
	mats.insert("Leave_A_b");
	mats.insert("Leave_A_c");
	mats.insert("Mona_Lisa_1_Leaf1");
	mats.insert("Mona_Lisa_1_Leaf2");
	mats.insert("Mona_Lisa_2_Leaf1");
	mats.insert("Mona_Lisa_1_petal11");
	mats.insert("Mona_Lisa_1_petal12");
	mats.insert("paniceum_Leaf");
	mats.insert("Pansy_1_blblbac");
	mats.insert("Pansy_1_Leaf");
	mats.insert("Pansy_1_Leafcop");
	mats.insert("Pansy_1_Leafsma");
	mats.insert("Poinsettia_1_Leaf");
	mats.insert("Poinsettia_1_redleaf");
	mats.insert("Poinsettia_1_smallre");
	mats.insert("Rose_1_Blatt2");
	mats.insert("Rose_1_Blutenb");
	mats.insert("Rose_1_Blatt1_");
	mats.insert("Rose_1_Kelchbl");
	mats.insert("Rose_2__Blutenb");
	mats.insert("Rose_2__Kelchbl");
	mats.insert("Rose_2_Blatt1_");
	mats.insert("Rose_3_Blutenb");
	mats.insert("Rose_3_Blatt2");
	mats.insert("zebrina_Leaf");
	mats.insert("Tronco");
	mats.insert("Muros");
	mats.insert("techos");
	mats.insert("Azotea");
	mats.insert("Pared_SanMiguel_N");
	mats.insert("Pared_SanMiguel_H");
	mats.insert("Pared_SanMiguel_B");
	mats.insert("Pared_SanMiguel_G");
	mats.insert("Barandal_Detalle_Extremos");
	mats.insert("Madera_Silla");
	mats.insert("Forja_Macetas");
	mats.insert("Muro_Naranja_Escalera");
	mats.insert("Tela_Mesa_D_2");
	mats.insert("Tela_Mesa_D");
}

#if !defined(METAL)
static inline float2 abs(const float2& v)
{
	return float2(fabsf(v.getX()), fabsf(v.getY()));
}
static inline float2 subtract(const float2& v, const float2& w)
{
	return float2(v.getX() - w.getX(), v.getY() - w.getY());
}
static inline float2 step(const float2& y, const float2& x)
{
	return float2(x.getX() >= y.getX() ? 1.f : 0.f,
		x.getY() >= y.getY() ? 1.f : 0.f);
}
static inline float2 mulPerElem(const float2 &v, float f)
{
	return float2(v.getX()*f, v.getY()*f);
}
static inline float2 mulPerElem(const float2 &v, const float2& w)
{
	return float2(v.getX()*w.getX(), v.getY()*w.getY());
}
static inline float2 sumPerElem(const float2 &v, const float2& w)
{
	return float2(v.getX() + w.getX(), v.getY() + w.getY());
}
static inline float2 sign_not_zero(const float2& v)
{
	return subtract(mulPerElem(step(float2(0, 0), v), 2.0), float2(1, 1));
}
static inline uint packSnorm2x16(const float2& v)
{
	uint x = (uint)round(clamp(v.getX(), -1, 1) * 32767.0f);
	uint y = (uint)round(clamp(v.getY(), -1, 1) * 32767.0f);
	return ((uint)0x0000FFFF & x) | ((y << 16) & (uint)0xFFFF0000);
}
static inline uint packUnorm2x16(const float2& v)
{
	uint x = (uint)round(clamp(v.getX(), 0, 1) * 65535.0f);
	uint y = (uint)round(clamp(v.getY(), 0, 1) * 65535.0f);
	return ((uint)0x0000FFFF & x) | ((y << 16) & (uint)0xFFFF0000);
}

#define F16_EXPONENT_BITS 0x1F
#define F16_EXPONENT_SHIFT 10
#define F16_EXPONENT_BIAS 15
#define F16_MANTISSA_BITS 0x3ff
#define F16_MANTISSA_SHIFT (23 - F16_EXPONENT_SHIFT)
#define F16_MAX_EXPONENT (F16_EXPONENT_BITS << F16_EXPONENT_SHIFT)

static inline unsigned short F32toF16(float val)
{
	uint f32 = (*(uint *)&val);
	unsigned short f16 = 0;
	/* Decode IEEE 754 little-endian 32-bit floating-point value */
	int sign = (f32 >> 16) & 0x8000;
	/* Map exponent to the range [-127,128] */
	int exponent = ((f32 >> 23) & 0xff) - 127;
	int mantissa = f32 & 0x007fffff;
	if (exponent == 128)
	{ /* Infinity or NaN */
		f16 = (unsigned short)(sign | F16_MAX_EXPONENT);
		if (mantissa) f16 |= (mantissa & F16_MANTISSA_BITS);

	}
	else if (exponent > 15)
	{ /* Overflow - flush to Infinity */
		f16 = (unsigned short)(sign | F16_MAX_EXPONENT);
	}
	else if (exponent > -15)
	{ /* Representable value */
		exponent += F16_EXPONENT_BIAS;
		mantissa >>= F16_MANTISSA_SHIFT;
		f16 = (unsigned short)(sign | exponent << F16_EXPONENT_SHIFT | mantissa);
	}
	else
	{
		f16 = (unsigned short)sign;
	}
	return f16;
}
static inline uint pack2Floats(float2 f)
{
	return (F32toF16(f.getX()) & 0x0000FFFF) | ((F32toF16(f.getY()) << 16) & 0xFFFF0000);
}

static inline float2 normalize(const float2 & vec)
{
	float lenSqr = vec.getX()*vec.getX() + vec.getY()*vec.getY();
	float lenInv = (1.0f / sqrtf(lenSqr));
	return float2(vec.getX() * lenInv, vec.getY() * lenInv);
}

static inline float OctWrap(float v, float w)
{
	return (1.0f - abs(w)) * (v >= 0.0f ? 1.0f : -1.0f);
}

static inline uint encodeDir(const float3& n)
{
	float absLength = (abs(n.getX()) + abs(n.getY()) + abs(n.getZ()));
	float3 enc;
	enc.setX(n.getX() / absLength);
	enc.setY(n.getY() / absLength);
	enc.setZ(n.getZ() / absLength);

	if (enc.getZ() < 0)
	{
		float oldX = enc.getX();
		enc.setX(OctWrap(enc.getX(), enc.getY()));
		enc.setY(OctWrap(enc.getY(), oldX));
	}
	enc.setX(enc.getX() * 0.5f + 0.5f);
	enc.setY(enc.getY() * 0.5f + 0.5f);

	return packUnorm2x16(float2(enc.getX(), enc.getY()));
}
#endif

// Loads a scene using ASSIMP and returns a Scene object with scene information
Scene* loadScene(const char* fileName)
{
#if TARGET_IOS
	NSString *fileUrl = [[NSBundle mainBundle] pathForResource:[NSString stringWithUTF8String : fileName] ofType : @""];
	fileName = [fileUrl fileSystemRepresentation];
#endif

	Scene* scene = (Scene*)conf_calloc(1, sizeof(Scene));
	File assimpScene = {};
	assimpScene.Open(fileName, FileMode::FM_ReadBinary, FSRoot::FSR_Absolute);
	if (!assimpScene.IsOpen())
	{
		ErrorMsg("Could not open scene %s.\nPlease make sure you have downloaded the art assets by using the PRE_BUILD command in the root directory", fileName);
		return NULL;
	}
	ASSERT(assimpScene.IsOpen());

	assimpScene.Read(&scene->numMeshes, sizeof(uint32_t));
	assimpScene.Read(&scene->totalVertices, sizeof(uint32_t));
	assimpScene.Read(&scene->totalTriangles, sizeof(uint32_t));

	scene->meshes = (Mesh*)conf_calloc(scene->numMeshes, sizeof(Mesh));
    scene->indices = tinystl::vector<uint32>(scene->totalTriangles, uint32_t(0));
	scene->positions = tinystl::vector<SceneVertexPos>(scene->totalVertices, SceneVertexPos{ 0 });
	scene->texCoords = tinystl::vector<SceneVertexTexCoord>(scene->totalVertices, SceneVertexTexCoord{ 0 });
	scene->normals = tinystl::vector<SceneVertexNormal>(scene->totalVertices, SceneVertexNormal{ 0 });
    scene->tangents = tinystl::vector<SceneVertexTangent>(scene->totalVertices, SceneVertexTangent{ 0 });

    tinystl::vector<float2> texcoords(scene->totalVertices);
    tinystl::vector<float3> normals(scene->totalVertices);
    tinystl::vector<float3> tangents(scene->totalVertices);

    assimpScene.Read(scene->indices.getArray(), sizeof(uint32_t) * scene->totalTriangles);
    assimpScene.Read(scene->positions.getArray(), sizeof(float3) * scene->totalVertices);
    assimpScene.Read(texcoords.getArray(), sizeof(float2) * scene->totalVertices);
    assimpScene.Read(normals.getArray(), sizeof(float3) * scene->totalVertices);
    assimpScene.Read(tangents.getArray(), sizeof(float3) * scene->totalVertices);

    for (uint32_t v = 0; v < scene->totalVertices; v++)
    {
        const float3& normal = normals[v];
        const float3& tangent = tangents[v];
        const float2& tc = texcoords[v];
        
#ifndef METAL
        scene->normals[v].normal = encodeDir(normal);
        scene->tangents[v].tangent = encodeDir(tangent);
        scene->texCoords[v].texCoord = pack2Floats(float2(tc.x, 1.0f - tc.y));
#else
        scene->normals[v].nx = normal.x;
        scene->normals[v].ny = normal.y;
        scene->normals[v].nz = normal.z;
        
        scene->tangents[v].tx = tangent.x;
        scene->tangents[v].ty = tangent.y;
        scene->tangents[v].tz = tangent.z;
        
        scene->texCoords[v].u = tc.x;
        scene->texCoords[v].v = 1.0f - tc.y;
#endif
    }

	for (uint32_t i = 0; i < scene->numMeshes; ++i)
	{
		Mesh& batch = scene->meshes[i];

		assimpScene.Read(&batch.materialId, sizeof(uint32_t));
        assimpScene.Read(&batch.vertexCount, sizeof(uint32_t));
#ifndef METAL
		assimpScene.Read(&batch.startIndex, sizeof(uint32_t));
        assimpScene.Read(&batch.indexCount, sizeof(uint32_t));
#else
        assimpScene.Read(&batch.startVertex, sizeof(uint32_t));
        assimpScene.Read(&batch.vertexCount, sizeof(uint32_t));
#endif
	}

	tinystl::unordered_set<String> twoSidedMaterials;
	SetTwoSidedMaterials(twoSidedMaterials);

	tinystl::unordered_set<String> alphaTestMaterials;
	SetAlphaTestMaterials(alphaTestMaterials);

	assimpScene.Read(&scene->numMaterials, sizeof(uint32_t));
	scene->materials = (Material*)conf_calloc(scene->numMaterials, sizeof(Material));
	scene->textures = (char**)conf_calloc(scene->numMaterials, sizeof(char*));
	scene->normalMaps = (char**)conf_calloc(scene->numMaterials, sizeof(char*));
	scene->specularMaps = (char**)conf_calloc(scene->numMaterials, sizeof(char*));

#ifdef ORBIS
#define DEFAULT_ALBEDO "default.gnf"
#define DEFAULT_NORMAL "default_nrm.gnf"
#define DEFAULT_SPEC   "default_spec.gnf"
#else
#define DEFAULT_ALBEDO "default.dds"
#define DEFAULT_NORMAL "default_nrm.dds"
#define DEFAULT_SPEC "default.dds"
#endif

	for (uint32_t i = 0; i < scene->numMaterials; i++)
	{
		Material& m = scene->materials[i];
		m.twoSided = false;

		uint32_t matNameLength = 0;
		assimpScene.Read(&matNameLength, sizeof(uint32_t));

		tinystl::vector<char> matName(matNameLength);
		assimpScene.Read(matName.getArray(), sizeof(char) * matNameLength);

		uint32_t albedoNameLength = 0;
		assimpScene.Read(&albedoNameLength, sizeof(uint32_t));

		tinystl::vector<char> albedoName(albedoNameLength);
		assimpScene.Read(albedoName.getArray(), sizeof(char)*albedoNameLength);

		if (albedoName[0] != '\0')
		{
			String path(albedoName.getArray());
			uint dotPos = 0;
#ifdef ORBIS
			// try to load the GNF version instead: change extension to GNF
			path.rfind('.', -1, &dotPos);
			path.resize(dotPos);
			path[dotPos] = '\0';
			path.append(".gnf", 4);
#endif
			String base_filename = FileSystem::GetFileNameAndExtension(path);
			scene->textures[i] = (char*)conf_calloc(base_filename.size() + 1, sizeof(char));
			strcpy(scene->textures[i], base_filename.c_str());

			// try load the associated normal map 
			String normalMap(base_filename);
			normalMap.rfind('.', -1, &dotPos);
			normalMap.insert(dotPos, "_NRM", 4);

			if (!FileSystem::FileExists(normalMap, FSR_Textures))
				normalMap = DEFAULT_NORMAL;

			scene->normalMaps[i] = (char*)conf_calloc(normalMap.size() + 1, sizeof(char));
			strcpy(scene->normalMaps[i], normalMap.c_str());

			// try load the associated spec map 
			String specMap(base_filename);
			dotPos = 0;
			specMap.rfind('.', -1, &dotPos);
			specMap.insert(dotPos, "_SPEC", 5);

			if (!FileSystem::FileExists(specMap, FSR_Textures))
				specMap = DEFAULT_SPEC;

			scene->specularMaps[i] = (char*)conf_calloc(specMap.size() + 1, sizeof(char));
			strcpy(scene->specularMaps[i], specMap.c_str());
		}
		else
		{
			// default textures
			scene->textures[i] = (char*)conf_calloc(strlen(DEFAULT_ALBEDO) + 1, sizeof(char));
			strcpy(scene->textures[i], DEFAULT_ALBEDO);

			scene->normalMaps[i] = (char*)conf_calloc(strlen(DEFAULT_NORMAL) + 1, sizeof(char));
			strcpy(scene->normalMaps[i], DEFAULT_NORMAL);

			scene->specularMaps[i] = (char*)conf_calloc(strlen(DEFAULT_SPEC) + 1, sizeof(char));
			strcpy(scene->specularMaps[i], DEFAULT_SPEC);
		}

		float ns = 0.0f;
		assimpScene.Read(&ns, sizeof(float));  // load shininess

		int twoSided = 0;
		assimpScene.Read(&twoSided, sizeof(float));  // load two sided
		m.twoSided = (twoSided != 0);

		String tinyMatName(matName.getArray());
		if (twoSidedMaterials.find(tinyMatName) != twoSidedMaterials.end())
			m.twoSided = true;

		m.alphaTested = (alphaTestMaterials.find(tinyMatName) != alphaTestMaterials.end());
	}

	assimpScene.Close();
    
#ifdef METAL
    // Once we have read all the geometry from the original asset, expand indices into vertices so the models are compatible with Metal implementation.
    Scene originalScene = *scene;
    
    scene->totalTriangles = 0;
    scene->totalVertices = 0;
    scene->positions.clear();
    scene->texCoords.clear();
    scene->normals.clear();
    scene->tangents.clear();
    
    uint32_t originalIdx = 0;
    for (uint32_t i = 0; i < scene->numMeshes; i++)
    {
        scene->meshes[i].startVertex = scene->positions.size();
        
        uint32_t idxCount = originalScene.meshes[i].vertexCount; // Index count is stored in the vertex count member when reading the mesh on Metal.
        for (uint32_t j = 0; j < idxCount; j++)
        {
            uint32_t idx = originalScene.indices[originalIdx++];
            scene->positions.push_back(originalScene.positions[idx]);
            scene->texCoords.push_back(originalScene.texCoords[idx]);
            scene->normals.push_back(originalScene.normals[idx]);
            scene->tangents.push_back(originalScene.tangents[idx]);
        }
        scene->meshes[i].vertexCount = (uint32_t)scene->positions.size() - scene->meshes[i].startVertex;
        scene->meshes[i].triangleCount = scene->meshes[i].vertexCount / 3;
        scene->totalTriangles += scene->meshes[i].triangleCount;
        scene->totalVertices += scene->meshes[i].vertexCount;
    }
#endif

	return scene;
}

void removeScene(Scene* scene)
{
	for (uint32_t i = 0; i < scene->numMaterials; ++i)
	{
		conf_free(scene->textures[i]);
		conf_free(scene->normalMaps[i]);
		conf_free(scene->specularMaps[i]);
	}

	scene->positions.~vector();
	scene->texCoords.~vector();
	scene->normals.~vector();
	scene->tangents.~vector();
#ifndef METAL
	scene->indices.~vector();
#endif

	conf_free(scene->textures);
	conf_free(scene->normalMaps);
	conf_free(scene->specularMaps);
	conf_free(scene->meshes);
	conf_free(scene->materials);
	conf_free(scene);
}

vec3 makeVec3(const SceneVertexPos& v)
{
	return vec3(v.x, v.y, v.z);
}

// Compute an array of clusters from the mesh vertices. Clusters are sub batches of the original mesh limited in number
// for more efficient CPU / GPU culling. CPU culling operates per cluster, while GPU culling operates per triangle for
// all the clusters that passed the CPU test.
void CreateClusters(bool twoSided, const Scene* pScene, Mesh* mesh)
{
#if defined(METAL)
	struct Triangle
	{
		vec3 vtx[3];
	};

	Triangle triangleCache[CLUSTER_SIZE * 3];

	mesh->clusterCount = (mesh->triangleCount + CLUSTER_SIZE - 1) / CLUSTER_SIZE;
	mesh->clusters = (Cluster*)conf_malloc(mesh->clusterCount * sizeof(Cluster));
    mesh->clusterCompacts = (ClusterCompact*)conf_calloc(mesh->clusterCount, sizeof(ClusterCompact));
	memset(mesh->clusters, 0, mesh->clusterCount * sizeof(Cluster));

	const uint32_t triangleStart = mesh->startVertex / 3;  // Assumes that we have no indices and every 3 vertices are a triangle (due to Metal limitation).

	for (uint32_t i = 0; i < mesh->clusterCount; ++i)
	{
		const int clusterStart = i * CLUSTER_SIZE;
		const uint32_t clusterEnd = min<uint32_t>(clusterStart + CLUSTER_SIZE, mesh->triangleCount);

		const int clusterTriangleCount = clusterEnd - clusterStart;

		// Load all triangles into our local cache
		for (uint32_t triangleIndex = clusterStart; triangleIndex < clusterEnd; ++triangleIndex)
		{
			triangleCache[triangleIndex - clusterStart].vtx[0] = makeVec3(pScene->positions[triangleStart + triangleIndex * 3]);
			triangleCache[triangleIndex - clusterStart].vtx[1] = makeVec3(pScene->positions[triangleStart + triangleIndex * 3 + 1]);
			triangleCache[triangleIndex - clusterStart].vtx[2] = makeVec3(pScene->positions[triangleStart + triangleIndex * 3 + 2]);
		}

		vec3 aabbMin = vec3(INFINITY, INFINITY, INFINITY);
		vec3 aabbMax = -aabbMin;

		vec3 coneAxis = vec3(0, 0, 0);

		for (int triangleIndex = 0; triangleIndex < clusterTriangleCount; ++triangleIndex)
		{
			const Triangle& triangle = triangleCache[triangleIndex];
			for (int j = 0; j < 3; ++j)
			{
				aabbMin = minPerElem(aabbMin, triangle.vtx[j]);
				aabbMax = maxPerElem(aabbMax, triangle.vtx[j]);
			}

			vec3 triangleNormal = cross(triangle.vtx[1] - triangle.vtx[0],
				triangle.vtx[2] - triangle.vtx[0]);
			//if(!(triangleNormal == vec3(0,0,0)))
			if (lengthSqr(triangleNormal) > 0.01f)
				triangleNormal = normalize(triangleNormal);

			coneAxis = coneAxis - triangleNormal;
		}

		// This is the cosine of the cone opening angle - 1 means it's 0??,
		// we're minimizing this value (at 0, it would mean the cone is 90?? open)
		float coneOpening = 1;

		// dont cull two sided meshes
		bool validCluster = !twoSided;

		vec3 center = (aabbMin + aabbMax) / 2;
		// if the axis is 0 then we have a invalid cluster
		if (coneAxis == vec3(0, 0, 0))
			validCluster = false;

		coneAxis = normalize(coneAxis);

		float t = -INFINITY;

		// cant find a cluster for 2 sided objects
		if (validCluster)
		{
			// We nee a second pass to find the intersection of the line center + t * coneAxis with the plane defined by each triangle
			for (int triangleIndex = 0; triangleIndex < clusterTriangleCount; ++triangleIndex)
			{
				const Triangle& triangle = triangleCache[triangleIndex];
				// Compute the triangle plane from the three vertices

				const vec3 triangleNormal = normalize(cross(triangle.vtx[1] - triangle.vtx[0],
					triangle.vtx[2] - triangle.vtx[0]));
				const float directionalPart = dot(coneAxis, -triangleNormal);

				if (directionalPart <= 0.0f)   //AMD BUG?: changed to <= 0 because directionalPart is used to divide a quantity
				{
					// No solution for this cluster - at least two triangles are facing each other
					validCluster = false;
					break;
				}

				// We need to intersect the plane with our cone ray which is center + t * coneAxis, and find the max
				// t along the cone ray (which points into the empty space) See: https://en.wikipedia.org/wiki/Line%E2%80%93plane_intersection
				const float td = dot(center - triangle.vtx[0], triangleNormal) / -directionalPart;

				t = max(t, td);

				coneOpening = min(coneOpening, directionalPart);
			}
		}

		mesh->clusters[i].aabbMax = v3ToF3(aabbMax);
		mesh->clusters[i].aabbMin = v3ToF3(aabbMin);

		mesh->clusters[i].coneAngleCosine = sqrtf(1 - coneOpening * coneOpening);
		mesh->clusters[i].coneCenter = v3ToF3(center + coneAxis*t);
		mesh->clusters[i].coneAxis = v3ToF3(coneAxis);

		mesh->clusterCompacts[i].triangleCount = clusterTriangleCount;
		mesh->clusterCompacts[i].clusterStart = clusterStart;

		//#if AMD_GEOMETRY_FX_ENABLE_CLUSTER_CENTER_SAFETY_CHECK
		// If distance of coneCenter to the bounding box center is more than 16x the bounding box extent, the cluster is also invalid
		// This is mostly a safety measure - if triangles are nearly parallel to coneAxis, t may become very large and unstable
		if (validCluster)
		{
			const float aabbSize = length(aabbMax - aabbMin);
			const float coneCenterToCenterDistance = length(f3Tov3(mesh->clusters[i].coneCenter - v3ToF3(center)));

			if (coneCenterToCenterDistance > (16 * aabbSize))
				validCluster = false;
		}
		//#endif

		mesh->clusters[i].valid = validCluster;
	}
#else
	// 12 KiB stack space
	struct Triangle
	{
		vec3 vtx[3];
	};

	Triangle triangleCache[CLUSTER_SIZE * 3];

	const int triangleCount = mesh->indexCount / 3;
	const int clusterCount = (triangleCount + CLUSTER_SIZE - 1) / CLUSTER_SIZE;

	mesh->clusterCount = clusterCount;
	mesh->clusterCompacts = (ClusterCompact*)conf_calloc(mesh->clusterCount, sizeof(ClusterCompact));
	mesh->clusters = (Cluster*)conf_calloc(mesh->clusterCount, sizeof(Cluster));

	for (int i = 0; i < clusterCount; ++i)
	{
		const int clusterStart = i * CLUSTER_SIZE;
		const int clusterEnd = min(clusterStart + CLUSTER_SIZE, triangleCount);

		const int clusterTriangleCount = clusterEnd - clusterStart;

		// Load all triangles into our local cache
		for (int triangleIndex = clusterStart; triangleIndex < clusterEnd; ++triangleIndex)
		{
			triangleCache[triangleIndex - clusterStart].vtx[0] = makeVec3(pScene->positions[pScene->indices[mesh->startIndex + triangleIndex * 3]]);
			triangleCache[triangleIndex - clusterStart].vtx[1] = makeVec3(pScene->positions[pScene->indices[mesh->startIndex + triangleIndex * 3 + 1]]);
			triangleCache[triangleIndex - clusterStart].vtx[2] = makeVec3(pScene->positions[pScene->indices[mesh->startIndex + triangleIndex * 3 + 2]]);
		}

		vec3 aabbMin = vec3(INFINITY, INFINITY, INFINITY);
		vec3 aabbMax = -aabbMin;

		vec3 coneAxis = vec3(0, 0, 0);

		for (int triangleIndex = 0; triangleIndex < clusterTriangleCount; ++triangleIndex)
		{
			const auto& triangle = triangleCache[triangleIndex];
			for (int j = 0; j < 3; ++j)
			{
				aabbMin = minPerElem(aabbMin, triangle.vtx[j]);
				aabbMax = maxPerElem(aabbMax, triangle.vtx[j]);
			}

			vec3 triangleNormal = cross(
				triangle.vtx[1] - triangle.vtx[0],
				triangle.vtx[2] - triangle.vtx[0]);

			if (!(triangleNormal == vec3(0, 0, 0)))
				triangleNormal = normalize(triangleNormal);

			//coneAxis = DirectX::XMVectorAdd(coneAxis, DirectX::XMVectorNegate(triangleNormal));
			coneAxis = coneAxis - triangleNormal;
		}

		// This is the cosine of the cone opening angle - 1 means it's 0?,
		// we're minimizing this value (at 0, it would mean the cone is 90?
		// open)
		float coneOpening = 1;
		// dont cull two sided meshes
		bool validCluster = !twoSided;

		const vec3 center = (aabbMin + aabbMax) / 2;
		// if the axis is 0 then we have a invalid cluster
		if (coneAxis == vec3(0, 0, 0))
			validCluster = false;

		coneAxis = normalize(coneAxis);

		float t = -INFINITY;

		// cant find a cluster for 2 sided objects
		if (validCluster)
		{
			// We nee a second pass to find the intersection of the line center + t * coneAxis with the plane defined by each triangle
			for (int triangleIndex = 0; triangleIndex < clusterTriangleCount; ++triangleIndex)
			{
				const Triangle& triangle = triangleCache[triangleIndex];
				// Compute the triangle plane from the three vertices

				const vec3 triangleNormal = normalize(
					cross(
						triangle.vtx[1] - triangle.vtx[0],
						triangle.vtx[2] - triangle.vtx[0]));

				const float directionalPart = dot(coneAxis, -triangleNormal);

				if (directionalPart <= 0)   //AMD BUG?: changed to <= 0 because directionalPart is used to divide a quantity
				{
					// No solution for this cluster - at least two triangles are facing each other
					validCluster = false;
					break;
				}

				// We need to intersect the plane with our cone ray which is center + t * coneAxis, and find the max
				// t along the cone ray (which points into the empty space) See: https://en.wikipedia.org/wiki/Line%E2%80%93plane_intersection
				const float td = dot(center - triangle.vtx[0], triangleNormal) / -directionalPart;

				t = max(t, td);

				coneOpening = min(coneOpening, directionalPart);
			}
		}

		mesh->clusters[i].aabbMax = v3ToF3(aabbMax);
		mesh->clusters[i].aabbMin = v3ToF3(aabbMin);

		mesh->clusters[i].coneAngleCosine = sqrtf(1 - coneOpening * coneOpening);
		mesh->clusters[i].coneCenter = v3ToF3(center + coneAxis * t);
		mesh->clusters[i].coneAxis = v3ToF3(coneAxis);

		mesh->clusterCompacts[i].triangleCount = clusterTriangleCount;
		mesh->clusterCompacts[i].clusterStart = clusterStart;

		//#if AMD_GEOMETRY_FX_ENABLE_CLUSTER_CENTER_SAFETY_CHECK
		// If distance of coneCenter to the bounding box center is more than 16x the bounding box extent, the cluster is also invalid
		// This is mostly a safety measure - if triangles are nearly parallel to coneAxis, t may become very large and unstable
		const float aabbSize = length(aabbMax - aabbMin);
		const float coneCenterToCenterDistance = length(f3Tov3(mesh->clusters[i].coneCenter) - center);

		if (coneCenterToCenterDistance > (16 * aabbSize))
			validCluster = false;

		mesh->clusters[i].valid = validCluster;
	}
#endif
}

#if defined(METAL)
void addClusterToBatchChunk(const ClusterCompact* cluster, const Mesh* mesh, uint32_t meshIdx, bool isTwoSided, FilterBatchChunk* batchChunk)
{
	FilterBatchData* batchData = &batchChunk->batches[batchChunk->currentBatchCount++];

	batchData->triangleCount = cluster->triangleCount;
	batchData->triangleOffset = cluster->clusterStart + mesh->startVertex / 3; // each 3 vertices form a triangle
    batchData->meshIdx = meshIdx;
    batchData->twoSided = (isTwoSided ? 1 : 0);
}
#else
void addClusterToBatchChunk(const ClusterCompact* cluster, uint batchStart, uint accumDrawCount, uint accumNumTriangles, int meshIndex, FilterBatchChunk* batchChunk)
{
	const int filteredIndexBufferStartOffset = accumNumTriangles * 3;

	FilterBatchData* smallBatchData = &batchChunk->batches[batchChunk->currentBatchCount++];

	smallBatchData->accumDrawIndex = accumDrawCount;
	smallBatchData->faceCount = cluster->triangleCount;
	smallBatchData->meshIndex = meshIndex;

	// Offset relative to the start of the mesh
	smallBatchData->indexOffset = cluster->clusterStart * 3;
	smallBatchData->outputIndexOffset = filteredIndexBufferStartOffset;
	smallBatchData->drawBatchStart = batchStart;
}
#endif

void createCubeBuffers(Renderer* pRenderer, CmdPool* cmdPool, Buffer** ppVertexBuffer, Buffer** ppIndexBuffer)
{
	UNREF_PARAM(pRenderer);
	UNREF_PARAM(cmdPool);
	// Create vertex buffer
	float vertexData[] = {
		-1, -1, -1, 1,
		1, -1, -1, 1,
		1, 1, -1, 1,
		-1, 1, -1, 1,
		-1, -1, 1, 1,
		1, -1, 1, 1,
		1, 1, 1, 1,
		-1, 1, 1, 1,
	};

	BufferLoadDesc vbDesc = {};
	vbDesc.mDesc.mUsage = BUFFER_USAGE_VERTEX;
	vbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
	vbDesc.mDesc.mSize = sizeof(vertexData);
	vbDesc.mDesc.mVertexStride = sizeof(float) * 4;
	vbDesc.pData = vertexData;
	vbDesc.ppBuffer = ppVertexBuffer;
	addResource(&vbDesc);

	// Create index buffer
	uint16_t indices[6 * 6] =
	{
		0, 1, 3, 3, 1, 2,
		1, 5, 2, 2, 5, 6,
		5, 4, 6, 6, 4, 7,
		4, 0, 7, 7, 0, 3,
		3, 2, 7, 7, 2, 6,
		4, 5, 0, 0, 5, 1
	};

	BufferLoadDesc ibDesc = {};
	ibDesc.mDesc.mUsage = BUFFER_USAGE_INDEX;
	ibDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
	ibDesc.mDesc.mSize = sizeof(indices);
	ibDesc.mDesc.mIndexType = INDEX_TYPE_UINT16;
	ibDesc.pData = indices;
	ibDesc.ppBuffer = ppIndexBuffer;
	addResource(&ibDesc);
}

void destroyBuffers(Renderer* pRenderer, Buffer* outVertexBuffer, Buffer* outIndexBuffer)
{
	UNREF_PARAM(pRenderer);
	removeResource(outVertexBuffer);
	removeResource(outIndexBuffer);
}
