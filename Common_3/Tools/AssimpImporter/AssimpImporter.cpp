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

#include "../../ThirdParty/OpenSource/assimp/3.3.1/include/assimp/Importer.hpp"
#include "../../ThirdParty/OpenSource/assimp/3.3.1/include/assimp/Exporter.hpp"
#include "../../ThirdParty/OpenSource/assimp/3.3.1/include/assimp/scene.h"
#include "../../ThirdParty/OpenSource/assimp/3.3.1/include/assimp/postprocess.h"
#include "../../ThirdParty/OpenSource/assimp/3.3.1/include/assimp/metadata.h"
#include "../../ThirdParty/OpenSource/assimp/3.3.1/include/assimp/config.h"
#include "../../ThirdParty/OpenSource/assimp/3.3.1/include/assimp/cimport.h"
#include "../../ThirdParty/OpenSource/assimp/3.3.1/include/assimp/scene.h"
#include "../../ThirdParty/OpenSource/assimp/3.3.1/include/assimp/postprocess.h"
#include "../../ThirdParty/OpenSource/assimp/3.3.1/include/assimp/DefaultLogger.hpp"

#include "AssimpImporter.h"
#include "../../OS/Interfaces/ILogManager.h" //NOTE: this should be the last include in a .cpp
#include "../../OS/Interfaces/IMemoryManager.h" //NOTE: this should be the last include in a .cpp

static inline mat4 AssimpMat4ToMatrix(const aiMatrix4x4& mat)
{
	mat4 result;
	memcpy(&result, &mat, sizeof(float) * 16);
	return transpose(result);
}

static tinystl::string ExtractSceneNameFromFileName(const char* input)
{
	tinystl::string in(input);
	unsigned int lastSlash = -1;
	if (!in.rfind('/', -1, &lastSlash))
		in.rfind('\\', -1, &lastSlash);

	unsigned int lastperiod = 0;
	in.rfind('.', -1, &lastperiod);

	tinystl::string shortName(&in[lastSlash + 1], lastperiod - lastSlash - 1);
	shortName = shortName.to_lower();
	shortName.replace(' ', '_');

	return shortName;
}

static void GetNameFromAiString(tinystl::unordered_map<tinystl::string, size_t>* pMap, const aiString& originalName, tinystl::string& meshName, const tinystl::string& defaultPrefix /*= "entity"*/)
{
	meshName = originalName.C_Str();
	meshName.replace(' ', '_');
	if (meshName.size() == 0)
	{
		meshName = defaultPrefix + "_";
		if (pMap->find(meshName) == pMap->end())
		{
			pMap->insert(tinystl::pair<tinystl::string, size_t>(meshName, 0));
		}
		meshName += String::format("%d", (int)((*pMap)[meshName])++);
	}
}

static void CalculateBoundingBox(const aiMesh* mesh, float& minX, float& minY, float& minZ, float& maxX, float& maxY, float& maxZ)
{
#define aisgl_min(x,y) (x<y?x:y)
#define aisgl_max(x,y) (y>x?y:x)
	for (size_t t = 0; t < mesh->mNumVertices; ++t)
	{
		aiVector3D tmp = mesh->mVertices[t];
		minX = aisgl_min(minX, tmp.x);
		minY = aisgl_min(minY, tmp.y);
		minZ = aisgl_min(minZ, tmp.z);

		maxX = aisgl_max(maxX, tmp.x);
		maxY = aisgl_max(maxY, tmp.y);
		maxZ = aisgl_max(maxZ, tmp.z);
	}
#undef aisgl_min
#undef aisgl_max
}

static void CreateGeom(const aiMesh* mesh, const char* name, Mesh* pMesh)
{
	float minX = 0.0f, minY = 0.0f, minZ = 0.0f, maxX = 0.0f, maxY = 0.0f, maxZ = 0.0f;
	CalculateBoundingBox(mesh, minX, minY, minZ, maxX, maxY, maxZ);

	//transform.bLocked = false;
	////calculate an offset to keep the mesh centered around the transformation to make adjustments easier.
	float offsetX = 0.0f;//(maxX - minX) * 0.5f;
	float offsetY = 0.0f;//(maxY - minY) * 0.5f;
	float offsetZ = 0.0f;//(maxZ - minZ) * 0.5f;
						 //transform.mLocalTransform.setTranslation(vec3(offsetX, offsetY, offsetZ));

	BoundingBox BB; //Corrected Bounding Box
	BB.vMin = float3(minX - offsetX, minY - offsetY, minZ - offsetZ);
	BB.vMax = float3(maxX - offsetX, maxY - offsetY, maxZ - offsetZ);

	const int sizeVerts = mesh->mNumVertices * 3;
	const int sizeNorm = mesh->mNumVertices * 3;
	const int sizeTang = mesh->mNumVertices * 3;
	const int sizeBino = mesh->mNumVertices * 3;
	const int sizeUV = mesh->HasTextureCoords(0) ? mesh->mNumVertices * 2 : 0;
	const int sizeInde = mesh->mNumFaces * 3;

	//generate non-congruant buffers first
	//INDICES/////////////////////////////////////////////////////////////////
	unsigned int* indexBuffer = (unsigned int*)conf_calloc(sizeInde, sizeof(unsigned int));
	if (sizeInde > 0)
	{
		for (unsigned int i = 0; i < mesh->mNumFaces; i++)
		{
			const aiFace& face = mesh->mFaces[i];
			unsigned int j = i * 3;
			memcpy(&indexBuffer[j], face.mIndices, sizeof(unsigned int) * 3);
		}
	}
	//TEXTURE COORDS//////////////////////////////////////////////////////////
	float* uvBuffer = (float*)conf_calloc(sizeUV, sizeof(float));
	if (sizeUV > 0)
	{
		for (unsigned int i = 0; i < mesh->mNumVertices; i++)
		{
			aiVector3D uv = mesh->mTextureCoords[0][i];
			unsigned int j = i * 2;
			memcpy(&uvBuffer[j], &uv, sizeof(float) * 2);
		}
	}

	pMesh->mPositions.resize(mesh->mNumVertices);
	for (uint32_t i = 0; i < mesh->mNumVertices; ++i)
	{
		pMesh->mPositions[i] = float3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);
	}

	if (mesh->HasNormals())
	{
		pMesh->mNormals.resize(mesh->mNumVertices);
		for (uint32_t i = 0; i < mesh->mNumVertices; ++i)
		{
			pMesh->mNormals[i] = float3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
		}
	}

	if (mesh->HasTangentsAndBitangents())
	{
		pMesh->mTangents.resize(mesh->mNumVertices);
		pMesh->mBitangents.resize(mesh->mNumVertices);

		for (uint32_t i = 0; i < mesh->mNumVertices; ++i)
		{
			pMesh->mTangents[i] = float3(mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z);
			pMesh->mBitangents[i] = float3(mesh->mBitangents[i].x, mesh->mBitangents[i].y, mesh->mBitangents[i].z);
		}
	}

	if (mesh->HasTextureCoords(0))
	{
		pMesh->mUvs.resize(mesh->mNumVertices);
		memcpy(pMesh->mUvs.data(), uvBuffer, mesh->mNumVertices * sizeof(float2));
	}

	pMesh->mIndices.resize(sizeInde);
	memcpy(pMesh->mIndices.data(), indexBuffer, sizeInde * sizeof(unsigned int));

	pMesh->mBounds = BB;

	if (indexBuffer)
	{
		conf_free(indexBuffer);
	}
	if (uvBuffer)
	{
		conf_free(uvBuffer);
	}
}

static void CollectMeshes(const aiScene* pScene, Model* pModel, tinystl::unordered_map<tinystl::string, size_t>* pMap)
{
	//Set the size of the geometryList
	pModel->mGeometryNameList.resize(pScene->mNumMeshes);

	for (uint32_t i = 0; i < pScene->mNumMeshes; i++)
	{
		pModel->mMeshArray.push_back(Mesh());
		//Get the mesh name
		tinystl::string meshName = "";
		GetNameFromAiString(pMap, pScene->mMeshes[i]->mName, meshName, pModel->mSceneName + "_mesh");

		//parse geometry information
		CreateGeom(pScene->mMeshes[i], (char*)meshName.c_str(), &pModel->mMeshArray.back());
		pModel->mMeshArray.back().mMaterialId = pScene->mMeshes[i]->mMaterialIndex;

		//Add the geometry component in the geometryList
		pModel->mGeometryNameList[i] = meshName;
	}
}

static void CollectMaterials(const aiScene* pScene, Model* pModel, tinystl::unordered_map<tinystl::string, size_t>* pMap)
{
	if (pScene->mNumMaterials > 0)
	{
		pModel->mMaterialList.resize(pScene->mNumMaterials);

		for (uint32_t matIndex = 0; matIndex < pScene->mNumMaterials; ++matIndex)
		{
			aiMaterial* aiMaterial = pScene->mMaterials[matIndex];
			for (uint32_t propIndex = 0; propIndex < aiMaterial->mNumProperties; ++propIndex)
			{
				const aiMaterialProperty* pProp = aiMaterial->mProperties[propIndex];
				MaterialProperty prop = {};
				prop.mDataSize = min(pProp->mDataLength, MAX_ELEMENTS_PER_PROPERTY * sizeof(float));

				switch (pProp->mType)
				{
				case aiPropertyTypeInfo::aiPTI_Integer:
					prop.mIntVal = (*((int*)pProp->mData));
					break;
				case aiPropertyTypeInfo::aiPTI_Float:
				{
					float* pFloat = (float*)pProp->mData;
					for (uint32_t v = 0; v < prop.mDataSize / sizeof(float); ++v)
						prop.mFloatVal[v] = pFloat[v];
					break;
				}
				}

				pModel->mMaterialList[matIndex].mProperties.insert({ String(pProp->mKey.C_Str()), prop });
			}

			for (uint32_t textrureType = 0; textrureType < AI_TEXTURE_TYPE_MAX; ++textrureType)
			{
				aiString name;
				aiGetMaterialTexture(aiMaterial, (aiTextureType)textrureType, 0, &name);
				pModel->mMaterialList[matIndex].mTextureMaps[textrureType] = name.C_Str();
			}

			//Ge the material name
			aiString name;
			aiGetMaterialString(aiMaterial, AI_MATKEY_NAME, &name);
			tinystl::string materialName = "";
			GetNameFromAiString(pMap, name, materialName, "material");

			if (materialName == AI_DEFAULT_MATERIAL_NAME)
			{
				pModel->mMaterialList[matIndex].mName = "default";
				continue;
			}

			pModel->mMaterialList[matIndex].mName = materialName;
		}
	}
}

bool AssimpImporter::ImportModel(const char* filename, Model* pModel)
{
	aiPropertyStore* propertyStore = aiCreatePropertyStore();
	tinystl::unordered_map<tinystl::string, size_t> uniqueNameMap;

	//Prefer fast loading over high quality loading
	aiSetImportPropertyInteger(propertyStore, AI_CONFIG_FAVOUR_SPEED, 1);

	//Tell Assimp to not import a bunch of useless layers of objects
	aiSetImportPropertyInteger(propertyStore, AI_CONFIG_IMPORT_FBX_READ_ALL_GEOMETRY_LAYERS, 0);

	const unsigned int chunkSize = 65535;
	aiSetImportPropertyInteger(propertyStore, AI_CONFIG_PP_SLM_TRIANGLE_LIMIT, chunkSize);
	aiSetImportPropertyInteger(propertyStore, AI_CONFIG_PP_SBP_REMOVE, aiPrimitiveType_LINE | aiPrimitiveType_POINT);
	aiSetImportPropertyInteger(propertyStore, AI_CONFIG_PP_PTV_NORMALIZE, 1);

	unsigned int flags = aiProcess_CalcTangentSpace | // calculate tangents and bitangents if possible
		aiProcess_ValidateDataStructure;

	const aiScene* pScene = aiImportFileExWithProperties(filename, flags, nullptr, propertyStore);

	if (pScene == NULL)
		return false;

	pModel->mSceneName = ExtractSceneNameFromFileName(filename);

	CollectMaterials(pScene, pModel, &uniqueNameMap);
	CollectMeshes(pScene, pModel, &uniqueNameMap);

	if (pScene)
	{
		aiReleaseImport(pScene);
	}

	return true;
}
