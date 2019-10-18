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

#define CGLTF_IMPLEMENTATION
//#define FAST_OBJ_IMPLEMENTATION
#include "../../../../Common_3/ThirdParty/OpenSource/EASTL/string.h"
#include "../../../../Common_3/ThirdParty/OpenSource/EASTL/unordered_map.h"

#include "AssetLoader.h"


// OZZ
#include "../../../ThirdParty/OpenSource/ozz-animation/include/ozz/base/io/stream.h"
#include "../../../ThirdParty/OpenSource/ozz-animation/include/ozz/base/io/archive.h"

// TFX
#include "TressFXAsset.h"

#include "../../../OS/Interfaces/IOperatingSystem.h"
#include "../../../OS/Interfaces/IMemory.h"    //NOTE: this should be the last include in a .cpp

bool AssetLoader::LoadSkeleton(const Path* path, ozz::animation::Skeleton* skeleton)
{
	// Load skeleton from disk
	ozz::io::File file(path, FM_READ_BINARY);
    
	if (!file.opened())
		return false;
	ozz::io::IArchive archive(&file);
	archive >> *skeleton;
	if (!file.CloseOzzFile())
		return false;


	return true;
}

bool AssetLoader::LoadAnimation(const Path* path, ozz::animation::Animation* animation)
{
    // Load animation from disk
    ozz::io::File file(path, FM_READ_BINARY);

	if (!file.opened())
		return false;
	ozz::io::IArchive archive(&file);
	archive >> *animation;
	if (!file.CloseOzzFile())
		return false;

	return true;
}

void generateNormals(Mesh& mesh)
{
	size_t total_vertices = mesh.streams[0].data.size();
	size_t total_indices = mesh.indices.size();

	Stream normals;
	normals.type = cgltf_attribute_type::cgltf_attribute_type_normal;
	normals.data.resize(total_vertices);

	for (size_t i = 0; i < total_vertices; ++i)
	{
		normals.data[i] = {0.0f, 0.0f, 0.0f, 0.0f};
	}

	for (size_t i = 0; i < total_indices; i += 3)
	{
		Attr A = mesh.streams[0].data[mesh.indices[i]];
		Attr B = mesh.streams[0].data[mesh.indices[i + 1]];
		Attr C = mesh.streams[0].data[mesh.indices[i + 2]];

		//e1 = a-b
		vec3 e1;
		e1[0] = A.f[0] - B.f[0];
		e1[1] = A.f[1] - B.f[1];
		e1[2] = A.f[2] - B.f[2];
		//e2 = c-b
		vec3 e2;
		e2[0] = C.f[0] - B.f[0];
		e2[1] = C.f[1] - B.f[1];
		e2[2] = C.f[2] - B.f[2];
		//n = cross(e1,e2)
		vec3 n = cross(e2, e1);

		Attr& An = normals.data[mesh.indices[i]];
		Attr& Bn = normals.data[mesh.indices[i + 1]];
		Attr& Cn = normals.data[mesh.indices[i + 2]];

		An.f[0] += n[0];
		An.f[1] += n[1];
		An.f[2] += n[2];

		Bn.f[0] += n[0];
		Bn.f[1] += n[1];
		Bn.f[2] += n[2];

		Cn.f[0] += n[0];
		Cn.f[1] += n[1];
		Cn.f[2] += n[2];
	}

	for (size_t i = 0; i < total_vertices; ++i)
	{
		vec3 n;
		n[0] = normals.data[i].f[0];
		n[1] = normals.data[i].f[1];
		n[2] = normals.data[i].f[2];

		n = normalize(n);


		normals.data[i].f[0] = n[0];
		normals.data[i].f[1] = n[1];
		normals.data[i].f[2] = n[2];
	}

	mesh.streams.push_back(normals);
}

void processMeshes(eastl::vector<Mesh> & meshes, unsigned int flags)
{
	eastl::unordered_map<char*, uint32_t> materialIDMap;
	eastl::vector<uint32_t> gTextureIndexforMaterial;

	size_t meshCount = meshes.size();

	for (size_t i = 0; i < meshCount; i++)
	{
		Mesh & subMesh = meshes[i];

		// Apply submesh transform
		transformMesh(subMesh, subMesh.node);

		// In case indicies were not properly generated;
		size_t vertexCount = subMesh.streams[0].data.size();
		size_t indexCount = subMesh.indices.size();
		if (indexCount < vertexCount)
		{
			subMesh.indices.resize(vertexCount);
			if (indexCount == 0)
			{
				for (size_t j = 0; j < vertexCount; ++j)
				{
					subMesh.indices[j] = (unsigned int)j;
				}
			}
			reindexMesh(subMesh);
		}

		size_t NoofVertices = 0;
		size_t NoofNormals = 0;
		for (size_t j = 0; j < subMesh.streams.size(); ++j)
		{
			Stream & stream = subMesh.streams[j];

			if (stream.type == cgltf_attribute_type::cgltf_attribute_type_position)
			{
				NoofVertices = stream.data.size();

				if (flags & alIS_QUANTIZED)
				{
					for (size_t k = 0; k < stream.data.size(); ++k)
					{
						stream.data[k].f[0] *= float((1 << 16) - 1);
						stream.data[k].f[1] *= float((1 << 16) - 1);
						stream.data[k].f[2] *= float((1 << 16) - 1);
						stream.data[k].f[3] *= float((1 << 16) - 1);
					}
				}

        if (flags & alMAKE_LEFT_HANDED)
				{
					for (size_t k = 0; k < stream.data.size(); ++k)
					{
						stream.data[k].f[2] *= -1;
					}
				}
			}
			else if (stream.type == cgltf_attribute_type::cgltf_attribute_type_normal)
			{
				NoofNormals = stream.data.size();
				if (flags & alMAKE_LEFT_HANDED)
				{
					for (size_t k = 0; k < stream.data.size(); ++k)
					{
						stream.data[k].f[2] *= -1;
					}
				}
			}
		}

		// generate vertex normals if they are missing
		if (flags & AssetProcessFlags::alGEN_NORMALS && NoofNormals != NoofVertices)
		{
			generateNormals(subMesh);
		}

		// Generate material IDs
		if (flags & AssetProcessFlags::alGEN_MATERIAL_ID)
		{
			uint32_t materialIndex = (uint32_t)gTextureIndexforMaterial.size();
			subMesh.materialID = materialIndex;

			cgltf_material * mat = subMesh.material;
			char* materialName = (char*)"NoMaterial";
			if (mat &&
				mat->pbr_metallic_roughness.base_color_texture.texture &&
				mat->pbr_metallic_roughness.base_color_texture.texture->image->uri)
			{
				materialName = mat->pbr_metallic_roughness.base_color_texture.texture->image->uri;
			}

			if (materialIDMap.find(materialName) == materialIDMap.end())
			{
				materialIDMap[materialName] = materialIndex;

				gTextureIndexforMaterial.push_back(materialIndex);
				gTextureIndexforMaterial.push_back(materialIndex + 1);
				gTextureIndexforMaterial.push_back(materialIndex + 2);
				gTextureIndexforMaterial.push_back(materialIndex + 3);
				gTextureIndexforMaterial.push_back(materialIndex + 4);
			}
			else
			{
				if (materialName)
					subMesh.materialID = materialIDMap[materialName];
				else
					subMesh.materialID = 0;
			}
		}

		// Optimize Mesh
		if (flags & AssetProcessFlags::alOPTIMIZE)
		{
			reindexMesh(subMesh);
			//simplifyMesh(subMesh);
			optimizeMesh(subMesh);
		}
		if (flags & AssetProcessFlags::alSTRIPIFY)
		{
			// stripify should be called after a vertex fetch optimization
			stripifyMesh(subMesh);
		}
	}
}

bool AssetLoader::LoadModel(const Path* sceneFullPath, Model* model, unsigned int flags)
{
	cgltf_data* & data = model->data;
	eastl::vector<Mesh> & meshes = model->mMeshArray;

    const char* iext = fsGetPathExtension(sceneFullPath).buffer;

	if (iext && (strcmp(iext, "gltf") == 0 || strcmp(iext, "GLTF") == 0 || strcmp(iext, "glb") == 0 || strcmp(iext, "GLB") == 0))
	{
		cgltf_options options = {};
		cgltf_result result = parse_gltf_file(&options, sceneFullPath, &data);
		result = (result == cgltf_result_success) ? cgltf_validate(data) : result;
		result = (result == cgltf_result_success) ? load_gltf_buffers(&options, data, sceneFullPath) : result;

		if (result != cgltf_result_success)
		{
			fprintf(stderr, "Error loading %s: %s\n", fsGetPathAsNativeString(sceneFullPath), getError(result));
			cgltf_free(data);
			return false;
		}

		parseMeshesGltf(data, meshes);

		processMeshes(meshes, flags);
	}
	else
	{
		fprintf(stderr, "Error loading %s: unknown extension (expected .gltf or .glb)\n", fsGetPathAsNativeString(sceneFullPath));
		return false;
	}

	return true;
}