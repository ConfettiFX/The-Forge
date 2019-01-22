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

#include <algorithm>    // std::min

#include "assimp/Importer.hpp"
#include "assimp/Exporter.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"
#include "assimp/metadata.h"
#include "assimp/config.h"
#include "assimp/cimport.h"
#include "assimp/scene.h"
#include "assimp/postprocess.h"
#include "assimp/DefaultLogger.hpp"
#include "assimp/pbrmaterial.h"

#include "AssimpImporter.h"

#include "../../OS/Interfaces/ILogManager.h"       //NOTE: this should be the last include in a .cpp
#include "../../OS/Interfaces/IMemoryManager.h"    // NOTE: this should be the last include in a .cpp

static inline mat4 AssimpMat4ToMatrix(const aiMatrix4x4& mat)
{
	mat4 result;
	memcpy(&result, &mat, sizeof(float) * 16);
	return transpose(result);
}

static tinystl::string ExtractSceneNameFromFileName(const char* input)
{
	tinystl::string in(input);
	unsigned int    lastSlash = -1;
	if (!in.rfind('/', -1, &lastSlash))
		in.rfind('\\', -1, &lastSlash);

	unsigned int lastperiod = 0;
	in.rfind('.', -1, &lastperiod);

	tinystl::string shortName(&in[lastSlash + 1], lastperiod - lastSlash - 1);
	shortName = shortName.to_lower();
	shortName.replace(' ', '_');

	return shortName;
}

static IModelImporter::ModelSourceType ExtractSourceTypeFromFileName(const char* input)
{
	tinystl::string in(input);
	unsigned int    lastPeriod = 0;
	if (!in.rfind('.', -1, &lastPeriod))
		return IModelImporter::MODEL_SOURCE_TYPE_UNKNOWN;

	tinystl::string extension(&in[lastPeriod + 1]);
	extension = extension.to_lower();

	if (extension == "obj")
		return IModelImporter::MODEL_SOURCE_TYPE_OBJ;
	if (extension == "fbx")
		return IModelImporter::MODEL_SOURCE_TYPE_FBX;
	if (extension == "gltf" || extension == "glb")
		return IModelImporter::MODEL_SOURCE_TYPE_GLTF;

	return IModelImporter::MODEL_SOURCE_TYPE_UNKNOWN;
}

static bool SetTextureMapTilingMode(IModelImporter::TextureMap* textureMap, const aiMaterialProperty* prop, char channel)
{
	aiTextureMapMode   mapping = *(aiTextureMapMode*)prop->mData;
	TextureTilingMode* tilingMode = NULL;
	if (channel == 'u' || channel == 'U')
		tilingMode = &textureMap->mTilingModeU;
	else if (channel == 'v' || channel == 'V')
		tilingMode = &textureMap->mTilingModeV;
	else
		return false;

	switch (mapping)
	{
		case aiTextureMapMode_Wrap: *tilingMode = TEXTURE_TILING_MODE_WRAP; break;
		case aiTextureMapMode_Clamp: *tilingMode = TEXTURE_TILING_MODE_CLAMP; break;
		case aiTextureMapMode_Decal: *tilingMode = TEXTURE_TILING_MODE_BORDER; break;
		case aiTextureMapMode_Mirror: *tilingMode = TEXTURE_TILING_MODE_MIRROR; break;
		default: return false;
	}

	return true;
}

static void GetNameFromAiString(
	tinystl::unordered_map<tinystl::string, size_t>* pMap, const aiString& originalName, tinystl::string& meshName,
	const tinystl::string& defaultPrefix /*= "entity"*/)
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
		meshName += tinystl::string::format("%d", (int)((*pMap)[meshName])++);
	}
}

static void CalculateBoundingBox(const aiMesh* mesh, float& minX, float& minY, float& minZ, float& maxX, float& maxY, float& maxZ)
{
#define aisgl_min(x, y) (x < y ? x : y)
#define aisgl_max(x, y) (y > x ? y : x)
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

static void CreateGeom(const aiMesh* mesh, const char* name, AssimpImporter::Mesh* pMesh)
{
	float minX = 0.0f, minY = 0.0f, minZ = 0.0f, maxX = 0.0f, maxY = 0.0f, maxZ = 0.0f;
	CalculateBoundingBox(mesh, minX, minY, minZ, maxX, maxY, maxZ);

	//transform.bLocked = false;
	////calculate an offset to keep the mesh centered around the transformation to make adjustments easier.
	float offsetX = 0.0f;    //(maxX - minX) * 0.5f;
	float offsetY = 0.0f;    //(maxY - minY) * 0.5f;
	float offsetZ = 0.0f;    //(maxZ - minZ) * 0.5f;
							 //transform.mLocalTransform.setTranslation(vec3(offsetX, offsetY, offsetZ));

	BoundingBox BB;    //Corrected Bounding Box
	BB.vMin = float3(minX - offsetX, minY - offsetY, minZ - offsetZ);
	BB.vMax = float3(maxX - offsetX, maxY - offsetY, maxZ - offsetZ);

	//const int sizeVerts = mesh->mNumVertices * 3;
	//const int sizeNorm = mesh->mNumVertices * 3;
	//const int sizeTang = mesh->mNumVertices * 3;
	//const int sizeBino = mesh->mNumVertices * 3;
	const int sizeUV = mesh->HasTextureCoords(0) ? mesh->mNumVertices * 2 : 0;
	const int sizeInde = mesh->mNumFaces * 3;

	//generate non-congruant buffers first
	//INDICES/////////////////////////////////////////////////////////////////
	unsigned int* indexBuffer = (unsigned int*)conf_calloc(sizeInde, sizeof(unsigned int));
	ASSERT(indexBuffer);
	if (sizeInde > 0)
	{
		for (unsigned int i = 0; i < mesh->mNumFaces; i++)
		{
			const aiFace& face = mesh->mFaces[i];
			unsigned int  j = i * 3;
			memcpy(&indexBuffer[j], face.mIndices, sizeof(unsigned int) * 3);
		}
	}
	//TEXTURE COORDS//////////////////////////////////////////////////////////
	float* uvBuffer = (float*)conf_calloc(sizeUV, sizeof(float));
	ASSERT(uvBuffer);
	if (sizeUV > 0)
	{
		for (unsigned int i = 0; i < mesh->mNumVertices; i++)
		{
			aiVector3D   uv = mesh->mTextureCoords[0][i];
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

	if (mesh->HasBones())
	{
		pMesh->mBoneWeights.resize(mesh->mNumVertices);
		pMesh->mBoneNames.resize(mesh->mNumVertices);
		pMesh->mBones.resize(mesh->mNumBones);
		tinystl::vector<uint> vertexBoneCount(mesh->mNumVertices, 0);

		for (uint32_t i = 0; i < mesh->mNumBones; ++i)
		{
			aiBone* bone = mesh->mBones[i];
			for (uint32_t j = 0; j < bone->mNumWeights; ++j)
			{
				aiVertexWeight weight = bone->mWeights[j];
				uint           index = vertexBoneCount[weight.mVertexId];
				ASSERT(index < 4);

				pMesh->mBoneWeights[weight.mVertexId][index] = weight.mWeight;
				pMesh->mBoneNames[weight.mVertexId].mNames[index] = bone->mName.C_Str();

				++vertexBoneCount[weight.mVertexId];
			}

			aiMatrix4x4 mat = bone->mOffsetMatrix;
			mat4        offsetMat = *(mat4*)&mat;
			pMesh->mBones[i] = { bone->mName.C_Str(), transpose(offsetMat) };
		}
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

static bool FindGeometry(const tinystl::string& src, const tinystl::string* pData, uint32_t count)
{
	for (uint32_t i = 0; i < count; ++i)
		if (src == pData[i])
			return true;

	return false;
}

static void CollectMeshes(const aiScene* pScene, AssimpImporter::Model* pModel, tinystl::unordered_map<tinystl::string, size_t>* pMap)
{
	//Set the size of the geometryList
	pModel->mGeometryNameList.resize(pScene->mNumMeshes);

	for (uint32_t i = 0; i < pScene->mNumMeshes; i++)
	{
		pModel->mMeshArray.push_back(AssimpImporter::Mesh());
		//Get the mesh name
		tinystl::string meshName = tinystl::string::format("%s_Mesh_%u", pModel->mSceneName.c_str(), i);

		//parse geometry information
		CreateGeom(pScene->mMeshes[i], (char*)meshName.c_str(), &pModel->mMeshArray.back());
		pModel->mMeshArray.back().mMaterialId = pScene->mMeshes[i]->mMaterialIndex;

		if (!FindGeometry(meshName, pModel->mGeometryNameList.data(), i + 1))
		{
			//Add the geometry component in the geometryList
			pModel->mGeometryNameList[i] = meshName;
		}
		else
		{
			pModel->mGeometryNameList[i] = meshName + tinystl::string::format("%u", i);
		}
	}
}

static void CollectMaterials(const aiScene* pScene, AssimpImporter::Model* pModel, tinystl::unordered_map<tinystl::string, size_t>* pMap)
{
	if (pScene->mNumMaterials > 0)
	{
		pModel->mMaterialList.resize(pScene->mNumMaterials);

		for (uint32_t matIndex = 0; matIndex < pScene->mNumMaterials; ++matIndex)
		{
			aiMaterial* aiMaterial = pScene->mMaterials[matIndex];
			for (uint32_t propIndex = 0; propIndex < aiMaterial->mNumProperties; ++propIndex)
			{
				const aiMaterialProperty*        pProp = aiMaterial->mProperties[propIndex];
				AssimpImporter::MaterialProperty prop = {};
				prop.mDataSize = min((uint32_t)pProp->mDataLength, (uint32_t)(MAX_ELEMENTS_PER_PROPERTY * sizeof(float)));

				if (strncmp(pProp->mKey.C_Str(), "$tex", 4) == 0)
					continue;

				switch (pProp->mType)
				{
					case aiPropertyTypeInfo::aiPTI_Integer:
					{
						int* pInt = (int*)pProp->mData;
						for (uint32_t v = 0; v < prop.mDataSize / sizeof(int); ++v)
							prop.mIntVal[v] = pInt[v];
						break;
					}
					case aiPropertyTypeInfo::aiPTI_Float:
					{
						float* pFloat = (float*)pProp->mData;
						for (uint32_t v = 0; v < prop.mDataSize / sizeof(float); ++v)
							prop.mFloatVal[v] = pFloat[v];
						break;
					}
					case aiPropertyTypeInfo::aiPTI_String:
					{
						char* pChar = (char*)pProp->mData;
						for (uint32_t v = 0; v < prop.mDataSize; ++v)
							prop.mStringVal[v] = pChar[v];
						break;
					}
					case aiPropertyTypeInfo::aiPTI_Buffer:
					{
						unsigned char* pByte = (unsigned char*)pProp->mData;
						for (uint32_t v = 0; v < prop.mDataSize; ++v)
							prop.mBufferVal[v] = pByte[v];
						break;
					}
					case aiPropertyTypeInfo::aiPTI_Double:
					{
						double* pDouble = (double*)pProp->mData;
						for (uint32_t v = 0; v < prop.mDataSize / sizeof(double); ++v)
							prop.mFloatVal[v] = (float)pDouble[v];
						break;
					}
					default: break;
				}

				pModel->mMaterialList[matIndex].mProperties.insert({ tinystl::string(pProp->mKey.C_Str()), prop });
			}

			for (uint32_t textureType = 0; textureType <= AI_TEXTURE_TYPE_MAX; ++textureType)
			{
				pModel->mMaterialList[matIndex].mTextureMaps[textureType].resize(
					aiGetMaterialTextureCount(aiMaterial, (aiTextureType)textureType));
				for (uint32_t tex = 0; tex < (uint32_t)pModel->mMaterialList[matIndex].mTextureMaps[textureType].size(); ++tex)
				{
					aiString name;
					aiGetMaterialTexture(aiMaterial, (aiTextureType)textureType, tex, &name);

					IModelImporter::TextureMap textureOptions = {};
					textureOptions.mName = name.C_Str();
					textureOptions.mTilingModeU = TEXTURE_TILING_MODE_WRAP;
					textureOptions.mTilingModeV = TEXTURE_TILING_MODE_WRAP;
					textureOptions.mFilterMode = TEXTURE_FILTERING_MODE_ANISOTROPIC;
					textureOptions.mScale = 1.0f;

					const aiMaterialProperty* pPropMappingMode = NULL;
					if (aiGetMaterialProperty(aiMaterial, AI_MATKEY_MAPPINGMODE_U(textureType, tex), &pPropMappingMode) == aiReturn_SUCCESS)
						SetTextureMapTilingMode(&textureOptions, pPropMappingMode, 'U');
					if (aiGetMaterialProperty(aiMaterial, AI_MATKEY_MAPPINGMODE_V(textureType, tex), &pPropMappingMode) == aiReturn_SUCCESS)
						SetTextureMapTilingMode(&textureOptions, pPropMappingMode, 'V');

					if (pModel->mSourceType == IModelImporter::MODEL_SOURCE_TYPE_GLTF)
					{
						const aiMaterialProperty* pPropFilter = NULL;
						if (aiGetMaterialProperty(aiMaterial, AI_MATKEY_GLTF_MAPPINGFILTER_MIN(textureType, tex), &pPropFilter) ==
							aiReturn_SUCCESS)
						{
							uint filter = *(uint*)pPropFilter->mData;
							// Values based on SamplerMinFilter in <assimp>/code/glTF2Asset.h
							switch (filter)
							{
								case 9728:    // SamplerMinFilter_Nearest
									textureOptions.mFilterMode = TEXTURE_FILTERING_MODE_POINT;
									break;
								case 9729:    // SamplerMinFilter_Linear
									textureOptions.mFilterMode = TEXTURE_FILTERING_MODE_BILINEAR;
									break;
								case 9984:    // SamplerMinFilter_Nearest_Mipmap_Nearest
								case 9985:    // SamplerMinFilter_Linear_Mipmap_Nearest
								case 9986:    // SamplerMinFilter_Nearest_Mipmap_Linear
								case 9987:    // SamplerMinFilter_Linear_Mipmap_Linear
									textureOptions.mFilterMode = TEXTURE_FILTERING_MODE_TRILINEAR;
									break;
								default: textureOptions.mFilterMode = TEXTURE_FILTERING_MODE_ANISOTROPIC; break;
							}
						}

						const aiMaterialProperty* pPropScale = NULL;
						if (aiGetMaterialProperty(aiMaterial, AI_MATKEY_GLTF_TEXTURE_SCALE(textureType, tex), &pPropScale) ==
							aiReturn_SUCCESS)
							textureOptions.mScale = *(float*)pPropScale->mData;
					}

					pModel->mMaterialList[matIndex].mTextureMaps[textureType][tex] = textureOptions;
				}
			}

			//Get the material name
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

			//Get the source type of the material
			switch (pModel->mSourceType)
			{
				case IModelImporter::MODEL_SOURCE_TYPE_OBJ:
					pModel->mMaterialList[matIndex].mSourceType = IModelImporter::MATERIAL_SOURCE_TYPE_OBJ;
					break;
				case IModelImporter::MODEL_SOURCE_TYPE_FBX:
					pModel->mMaterialList[matIndex].mSourceType = IModelImporter::MATERIAL_SOURCE_TYPE_FBX;
					break;
				case IModelImporter::MODEL_SOURCE_TYPE_GLTF:
				{
					const aiMaterialProperty* prop = NULL;
					if (aiGetMaterialProperty(aiMaterial, AI_MATKEY_GLTF_PBRSPECULARGLOSSINESS, &prop) == aiReturn_SUCCESS)
						pModel->mMaterialList[matIndex].mSourceType = IModelImporter::MATERIAL_SOURCE_TYPE_GLTF_SPEC_GLOSS;
					else
						pModel->mMaterialList[matIndex].mSourceType = IModelImporter::MATERIAL_SOURCE_TYPE_GLTF_METAL_ROUGH;
					break;
				}
				default: break;
			}
		}
	}
}

static void CollectTextures(const aiScene* pScene, AssimpImporter::Model* pModel)
{
	pModel->mEmbeddedTextureList.resize(pScene->mNumTextures);
	for (uint32_t i = 0; i < (uint32_t)pModel->mEmbeddedTextureList.size(); ++i)
	{
		const aiTexture*                     pTex = pScene->mTextures[i];
		AssimpImporter::EmbeddedTextureData& tex = pModel->mEmbeddedTextureList[i];
		tex.mName = pTex->mFilename.C_Str();
		tex.mFormat = pTex->achFormatHint;
		tex.mWidth = pTex->mWidth;
		tex.mHeight = pTex->mHeight;
		tex.mData.resize(tex.mWidth * max(1U, tex.mHeight));
		memcpy(tex.mData.data(), pTex->pcData, tex.mData.size() * sizeof(unsigned char));
	}
}

static void CollectNodes(const aiNode* pIn, AssimpImporter::Node* pNode)
{
	pNode->mChildren.resize(pIn->mNumChildren);
	pNode->mMeshIndices.resize(pIn->mNumMeshes);
	pNode->mName = pIn->mName.C_Str();
	pNode->mTransform = AssimpMat4ToMatrix(pIn->mTransformation);

	for (uint32_t i = 0; i < (uint32_t)pNode->mMeshIndices.size(); ++i)
	{
		pNode->mMeshIndices[i] = pIn->mMeshes[i];
	}

	for (uint32_t i = 0; i < (uint32_t)pNode->mChildren.size(); ++i)
	{
		pNode->mChildren[i].pParent = pNode;
		CollectNodes(pIn->mChildren[i], &pNode->mChildren[i]);
	}
}

static void CollectNodes(const aiScene* pScene, AssimpImporter::Model* pModel)
{
	CollectNodes(pScene->mRootNode, &pModel->mRootNode);
	pModel->mRootNode.mName = pModel->mSceneName;

	// Scale down meshes loaded from fbx files.
	if (pModel->mSourceType == IModelImporter::MODEL_SOURCE_TYPE_FBX)
	{
		for (uint i = 0; i < (uint)pModel->mRootNode.mChildren.size(); ++i)
			pModel->mRootNode.mChildren[i].mTransform = mat4::scale(vec3(0.01f)) * pModel->mRootNode.mChildren[i].mTransform;
	}
}

bool AssimpImporter::ImportModel(const char* filename, Model* pModel)
{
	tinystl::unordered_map<tinystl::string, size_t> uniqueNameMap;

	aiPropertyStore* propertyStore = aiCreatePropertyStore();

	////Tell Assimp to not import a bunch of useless layers of objects
	aiSetImportPropertyInteger(propertyStore, AI_CONFIG_IMPORT_FBX_READ_ALL_GEOMETRY_LAYERS, 0);
	aiSetImportPropertyInteger(propertyStore, AI_CONFIG_IMPORT_FBX_READ_TEXTURES, 1);
	aiSetImportPropertyFloat(propertyStore, AI_CONFIG_PP_GSN_MAX_SMOOTHING_ANGLE, 66.0f);

	unsigned int flags = aiProcessPreset_TargetRealtime_MaxQuality;
	flags |= aiProcess_GenSmoothNormals | aiProcess_CalcTangentSpace | aiProcess_ImproveCacheLocality | aiProcess_FindDegenerates |
			 aiProcess_FindInvalidData | aiProcess_FixInfacingNormals | aiProcess_JoinIdenticalVertices | aiProcess_LimitBoneWeights |
			 aiProcess_ConvertToLeftHanded;
	flags &= ~aiProcess_SortByPType;
	flags &= ~aiProcess_FindInstances;

	const aiScene* pScene = aiImportFileExWithProperties(filename, flags, nullptr, propertyStore);
	if (!pScene)
	{
		//LOGERRORF("Assimp could not import file: %s", filename);
		return false;
	}

	pModel->mSceneName = ExtractSceneNameFromFileName(filename);
	pModel->mSourceType = ExtractSourceTypeFromFileName(filename);

	CollectTextures(pScene, pModel);
	CollectMaterials(pScene, pModel, &uniqueNameMap);
	CollectMeshes(pScene, pModel, &uniqueNameMap);
	CollectNodes(pScene, pModel);

	if (pScene)
	{
		aiReleaseImport(pScene);
	}

	return true;
}
