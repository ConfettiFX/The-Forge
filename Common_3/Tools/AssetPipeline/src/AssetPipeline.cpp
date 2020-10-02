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

#include "AssetPipeline.h"

// Math
#include "../../../ThirdParty/OpenSource/ModifiedSonyMath/vectormath.hpp"

// Tiny stl
#include "../../../ThirdParty/OpenSource/EASTL/string.h"
#include "../../../ThirdParty/OpenSource/EASTL/vector.h"
#include "../../../ThirdParty/OpenSource/EASTL/unordered_map.h"

// OZZ
//#include "../../../ThirdParty/OpenSource/ozz-animation/include/ozz/base/io/stream.h"
#include "../../../ThirdParty/OpenSource/ozz-animation/include/ozz/base/io/archive.h"
#include "../../../ThirdParty/OpenSource/ozz-animation/include/ozz/animation/offline/raw_skeleton.h"
#include "../../../ThirdParty/OpenSource/ozz-animation/include/ozz/animation/offline/raw_animation.h"
#include "../../../ThirdParty/OpenSource/ozz-animation/include/ozz/animation/offline/skeleton_builder.h"
#include "../../../ThirdParty/OpenSource/ozz-animation/include/ozz/animation/offline/animation_builder.h"

#include "../../../ThirdParty/OpenSource/tinyimageformat/tinyimageformat_base.h"

// TressFX
#include "../../../ThirdParty/OpenSource/TressFX/TressFXAsset.h"

#define CGLTF_WRITE_IMPLEMENTATION
#define CGLTF_IMPLEMENTATION
#include "../../../ThirdParty/OpenSource/cgltf/cgltf_write.h"

#define TINYKTX_IMPLEMENTATION
#include "../../../OS/Core/TextureContainers.h"

#include "../../../OS/Interfaces/IOperatingSystem.h"
#include "../../../OS/Interfaces/IFileSystem.h"
#include "../../../OS/Interfaces/ILog.h"

#include "../../FileSystem/IToolFileSystem.h"

#include "../../../OS/Interfaces/IMemory.h"    //NOTE: this should be the last include in a .cpp

typedef eastl::unordered_map<eastl::string, eastl::vector<eastl::string>> AnimationAssetMap;
//extern eastl::vector<PathHandle> fsGetSubDirectories(const Path* directory);

ResourceDirectory RD_INPUT = RD_MIDDLEWARE_1;
ResourceDirectory RD_OUTPUT = RD_MIDDLEWARE_2;

struct NodeInfo
{
	eastl::string       mName;
	int                 pParentNodeIndex;
	eastl::vector<int>  mChildNodeIndices;
	bool                mUsedInSkeleton;
	cgltf_node*         pNode;
};

struct BoneInfo
{
	int                                          mNodeIndex;
	int                                          mParentNodeIndex;
	ozz::animation::offline::RawSkeleton::Joint* pParentJoint;
};

cgltf_result cgltf_parse_and_load(const char* skeletonAsset, cgltf_data** ppData, void** ppFileData)
{
	// Import the glTF with the animation
	FileStream file = {};
	if (!fsOpenStreamFromPath(RD_INPUT, skeletonAsset, FM_READ_BINARY, &file))
	{
		LOGF(eERROR, "Failed to open gltf file %s", skeletonAsset);
		ASSERT(false);
		return cgltf_result_file_not_found;
	}

	ssize_t fileSize = fsGetStreamFileSize(&file);
	void* fileData = tf_malloc(fileSize);
	cgltf_result result = cgltf_result_invalid_gltf;

	fsReadFromStream(&file, fileData, fileSize);

	cgltf_data* data = NULL;
	cgltf_options options = {};
	options.memory_alloc = [](void* user, cgltf_size size) { return tf_malloc(size); };
	options.memory_free = [](void* user, void* ptr) { tf_free(ptr); };
	result = cgltf_parse(&options, fileData, fileSize, &data);
	fsCloseStream(&file);


	if (cgltf_result_success != result)
	{
		LOGF(eERROR, "Failed to parse gltf file %s with error %u", skeletonAsset, (uint32_t)result);
		ASSERT(false);
		tf_free(fileData);
		return result;
	}

	result = cgltf_validate(data);
	if (cgltf_result_success != result)
	{
		LOGF(eWARNING, "GLTF validation finished with error %u for file %s", (uint32_t)result, skeletonAsset);
	}

	// Load buffers located in separate files (.bin) using our file system
	for (uint32_t i = 0; i < data->buffers_count; ++i)
	{
		const char* uri = data->buffers[i].uri;

		if (!uri || data->buffers[i].data)
		{
			continue;
		}

		if (strncmp(uri, "data:", 5) != 0 && !strstr(uri, "://"))
		{
			char parent[FS_MAX_PATH] = { 0 };
			fsGetParentPath(skeletonAsset, parent);
			char path[FS_MAX_PATH] = { 0 };
			fsAppendPathComponent(parent, uri, path);
			FileStream fs = {};
			if (fsOpenStreamFromPath(RD_INPUT, path, FM_READ_BINARY, &fs))
			{
				ASSERT(fsGetStreamFileSize(&fs) >= (ssize_t)data->buffers[i].size);
				data->buffers[i].data = tf_malloc(data->buffers[i].size);
				fsReadFromStream(&fs, data->buffers[i].data, data->buffers[i].size);
			}
			fsCloseStream(&fs);
		}
	}

	result = cgltf_load_buffers(&options, data, skeletonAsset);
	if (cgltf_result_success != result)
	{
		LOGF(eERROR, "Failed to load buffers from gltf file %s with error %u", skeletonAsset, (uint32_t)result);
		ASSERT(false);
		tf_free(fileData);
		return result;
	}

	*ppData = data;
	*ppFileData = fileData;

	return result;
}

cgltf_result cgltf_write(const char* skeletonAsset, cgltf_data* data)
{
	cgltf_options options = {};
	options.memory_alloc = [](void* user, cgltf_size size) { return tf_malloc(size); };
	options.memory_free = [](void* user, void* ptr) { tf_free(ptr); };
	cgltf_size expected = cgltf_write(&options, NULL, 0, data);
	char* writeBuffer = (char*)tf_malloc(expected);
	cgltf_size actual = cgltf_write(&options, writeBuffer, expected, data);
	if (expected != actual)
	{
		LOGF(eERROR, "Error: expected %zu bytes but wrote %zu bytes.\n", expected, actual);
		return cgltf_result_invalid_gltf;
	}

	FileStream file = {};
	fsOpenStreamFromPath(RD_INPUT, skeletonAsset, FM_WRITE, &file);
	fsWriteToStream(&file, writeBuffer, actual - 1);
	fsCloseStream(&file);
	tf_free(writeBuffer);

	return cgltf_result_success;
}

bool AssetPipeline::ProcessAnimations(ProcessAssetsSettings* settings)
{
	// Check for assets containing animations in animationDirectory
	AnimationAssetMap                animationAssets;

	eastl::vector<eastl::string> subDirectories;
	fsGetSubDirectories(RD_INPUT, "", subDirectories);
	for (const eastl::string& subDir : subDirectories)
	{
		// Get all glTF files
		eastl::vector<eastl::string> filesInDirectory;
		fsGetFilesWithExtension(RD_INPUT, subDir.c_str(), ".gltf", filesInDirectory);

		for (const eastl::string& file : filesInDirectory)
		{
			char fileName[FS_MAX_PATH] = {};
			fsGetPathFileName(file.c_str(), fileName);
			if (!stricmp(fileName, "riggedmesh"))
			{
				// Add new animation asset named after the folder it is in
				char assetName[FS_MAX_PATH] = {};
				fsGetPathFileName(subDir.c_str(), assetName);
				animationAssets[eastl::string(assetName)].push_back(file);

				// Find sub directory called animations
				eastl::vector<eastl::string> assetSubDirectories;
				fsGetSubDirectories(RD_INPUT, subDir.c_str(), assetSubDirectories);
				for (const eastl::string& assetSubDir : assetSubDirectories)
				{
					char subDir[FS_MAX_PATH] = {};
					fsGetPathFileName(assetSubDir.c_str(), subDir);
					if (!stricmp(subDir, "animations"))
					{
						// Add all files in animations to the animation asset
						filesInDirectory.clear();
						fsGetFilesWithExtension(RD_INPUT, assetSubDir.c_str(), ".gltf", filesInDirectory);
						animationAssets[assetName].insert(
							animationAssets[assetName].end(), filesInDirectory.begin(), filesInDirectory.end());
						break;
					}
				}

				break;
			}
		}
	}

	// Do some checks
	if (!settings->quiet)
	{
		if (animationAssets.empty())
			LOGF(LogLevel::eWARNING, "%s does not contain any animation files.", fsGetResourceDirectory(RD_INPUT));

		for (AnimationAssetMap::iterator it = animationAssets.begin(); it != animationAssets.end(); ++it)
		{
			if (it->second.size() == 1)
				LOGF(LogLevel::eWARNING, "No animations found for rigged mesh %s.", it->first.c_str());
		}
	}

	// No assets found. Return.
	if (animationAssets.empty())
		return true;

	// Process the found assets
	int  assetsProcessed = 0;
	bool success = true;
	for (AnimationAssetMap::iterator it = animationAssets.begin(); it != animationAssets.end(); ++it)
	{
		const char* skinnedMesh = it->second[0].c_str();

		// Create skeleton output file name
		char skeletonOutputDir[FS_MAX_PATH] = {};
		fsAppendPathComponent("", it->first.c_str(), skeletonOutputDir);
		char skeletonOutput[FS_MAX_PATH] = {};
		fsAppendPathComponent(skeletonOutputDir, "skeleton.ozz", skeletonOutput);

		// Check if the skeleton is already up-to-date
		bool processSkeleton = true;
		if (!settings->force)
		{
			time_t lastModified = fsGetLastModifiedTime(RD_INPUT, skinnedMesh);
			time_t lastProcessed = fsGetLastModifiedTime(RD_OUTPUT, skeletonOutput);

			if (lastModified < lastProcessed && lastProcessed != ~0u && lastProcessed > settings->minLastModifiedTime)
				processSkeleton = false;
		}

		ozz::animation::Skeleton skeleton;
		if (processSkeleton)
		{
			// Process the skeleton
			if (!CreateRuntimeSkeleton(skinnedMesh, it->first.c_str(), skeletonOutput, &skeleton, settings))
			{
				success = false;
				continue;
			}

			++assetsProcessed;
		}
		else
		{
			// Load skeleton from disk
			FileStream file = {};


			if (!fsOpenStreamFromPath(RD_OUTPUT, skeletonOutput, FM_READ_BINARY, &file))
				return false;
			ozz::io::IArchive archive(&file);
			archive >> skeleton;
			fsCloseStream(&file);
		}

		// Process animations
		for (size_t i = 1; i < it->second.size(); ++i)
		{
			const char* animationFile = it->second[i].c_str();

			char animationName[FS_MAX_PATH] = {};
			fsGetPathFileName(animationFile, animationName);
			// Create animation output file name
			eastl::string outputFileString = it->first + "/animations/" + animationName + ".ozz";
			char animationOutput[FS_MAX_PATH] = {};
			fsAppendPathComponent("", outputFileString.c_str(), animationOutput);

			// Check if the animation is already up-to-date
			bool processAnimation = true;
			if (!settings->force && !processSkeleton)
			{
				time_t lastModified = fsGetLastModifiedTime(RD_INPUT, animationFile);
				time_t lastProcessed = fsGetLastModifiedTime(RD_OUTPUT, animationOutput);

				if (lastModified < lastProcessed && lastProcessed != ~0u && lastModified > settings->minLastModifiedTime)
					processAnimation = false;
			}

			if (processAnimation)
			{
				// Process the animation
				if (!CreateRuntimeAnimation(
					animationFile, &skeleton, it->first.c_str(), animationName, animationOutput, settings))
					continue;

				++assetsProcessed;
			}
		}

		skeleton.Deallocate();
	}

	if (!settings->quiet && assetsProcessed == 0 && success)
		LOGF(LogLevel::eINFO, "All assets already up-to-date.");

	return success;
}

static bool SaveSVT(const char* fileName, FileStream* pSrc, SVT_HEADER* pHeader)
{
	FileStream fh = {};

	if (!fsOpenStreamFromPath(RD_OUTPUT, fileName, FM_WRITE_BINARY, &fh))
		return false;

	//TODO: SVT should support any components somepoint
	const uint32_t numberOfComponents = pHeader->mComponentCount;
	const uint32_t pageSize = pHeader->mPageSize;

	//Header
	fsWriteToStream(&fh, pHeader, sizeof(SVT_HEADER));

	uint32_t mipPageCount = pHeader->mMipLevels - (uint32_t)log2f((float)pageSize);

	// Allocate Pages
	unsigned char** mipLevelPixels = (unsigned char**)tf_calloc(pHeader->mMipLevels, sizeof(unsigned char*));
	unsigned char*** pagePixels = (unsigned char***)tf_calloc(mipPageCount + 1, sizeof(unsigned char**));

	uint32_t* mipSizes = (uint32_t*)tf_calloc(pHeader->mMipLevels, sizeof(uint32_t));

	for (uint32_t i = 0; i < pHeader->mMipLevels; ++i)
	{
		uint32_t mipSize = (pHeader->mWidth >> i) * (pHeader->mHeight >> i) * numberOfComponents;
		mipSizes[i] = mipSize;
		mipLevelPixels[i] = (unsigned char*)tf_calloc(mipSize, sizeof(unsigned char));
		fsReadFromStream(pSrc, mipLevelPixels[i], mipSize);
	}

	// Store Mip data
	for (uint32_t i = 0; i < mipPageCount; ++i)
	{
		uint32_t xOffset = pHeader->mWidth >> i;
		uint32_t yOffset = pHeader->mHeight >> i;

		// width and height in tiles
		uint32_t tileWidth = xOffset / pageSize;
		uint32_t tileHeight = yOffset / pageSize;

		uint32_t xMipOffset = 0;
		uint32_t yMipOffset = 0;
		uint32_t pageIndex = 0;

		uint32_t rowLength = xOffset * numberOfComponents;

		pagePixels[i] = (unsigned char**)tf_calloc(tileWidth * tileHeight, sizeof(unsigned char*));

		for (uint32_t j = 0; j < tileHeight; ++j)
		{
			for (uint32_t k = 0; k < tileWidth; ++k)
			{
				pagePixels[i][pageIndex] = (unsigned char*)tf_calloc(pageSize * pageSize, sizeof(unsigned char) * numberOfComponents);

				for (uint32_t y = 0; y < pageSize; ++y)
				{
					for (uint32_t x = 0; x < pageSize; ++x)
					{
						uint32_t mipPageIndex = (y * pageSize + x) * numberOfComponents;
						uint32_t mipIndex = rowLength * (y + yMipOffset) + (numberOfComponents)*(x + xMipOffset);

						pagePixels[i][pageIndex][mipPageIndex + 0] = mipLevelPixels[i][mipIndex + 0];
						pagePixels[i][pageIndex][mipPageIndex + 1] = mipLevelPixels[i][mipIndex + 1];
						pagePixels[i][pageIndex][mipPageIndex + 2] = mipLevelPixels[i][mipIndex + 2];
						pagePixels[i][pageIndex][mipPageIndex + 3] = mipLevelPixels[i][mipIndex + 3];
					}
				}

				xMipOffset += pageSize;
				pageIndex += 1;
			}

			xMipOffset = 0;
			yMipOffset += pageSize;
		}
	}

	uint32_t mipTailPageSize = 0;

	pagePixels[mipPageCount] = (unsigned char**)tf_calloc(1, sizeof(unsigned char*));

	// Calculate mip tail size
	for (uint32_t i = mipPageCount; i < pHeader->mMipLevels - 1; ++i)
	{
		uint32_t mipSize = mipSizes[i];
		mipTailPageSize += mipSize;
	}

	pagePixels[mipPageCount][0] = (unsigned char*)tf_calloc(mipTailPageSize, sizeof(unsigned char));

	// Store mip tail data
	uint32_t mipTailPageWrites = 0;
	for (uint32_t i = mipPageCount; i < pHeader->mMipLevels - 1; ++i)
	{
		uint32_t mipSize = mipSizes[i];

		for (uint32_t j = 0; j < mipSize; ++j)
		{
			pagePixels[mipPageCount][0][mipTailPageWrites++] = mipLevelPixels[i][j];
		}
	}

	// Write mip data
	for (uint32_t i = 0; i < mipPageCount; ++i)
	{
		// width and height in tiles
		uint32_t mipWidth = (pHeader->mWidth >> i) / pageSize;
		uint32_t mipHeight = (pHeader->mHeight >> i) / pageSize;

		for (uint32_t j = 0; j < mipWidth * mipHeight; ++j)
		{
			fsWriteToStream(&fh, pagePixels[i][j], pageSize * pageSize * numberOfComponents * sizeof(char));
		}
	}

	// Write mip tail data
	fsWriteToStream(&fh, pagePixels[mipPageCount][0], mipTailPageSize * sizeof(char));

	// free memory
	tf_free(mipSizes);

	for (uint32_t i = 0; i < mipPageCount; ++i)
	{
		// width and height in tiles
		uint32_t mipWidth = (pHeader->mWidth >> i) / pageSize;
		uint32_t mipHeight = (pHeader->mHeight >> i) / pageSize;
		uint32_t pageIndex = 0;

		for (uint32_t j = 0; j < mipHeight; ++j)
		{
			for (uint32_t k = 0; k < mipWidth; ++k)
			{
				tf_free(pagePixels[i][pageIndex]);
				pageIndex += 1;
			}
		}

		tf_free(pagePixels[i]);
	}
	tf_free(pagePixels[mipPageCount][0]);
	tf_free(pagePixels[mipPageCount]);
	tf_free(pagePixels);

	for (uint32_t i = 0; i < pHeader->mMipLevels; ++i)
	{
		tf_free(mipLevelPixels[i]);
	}
	tf_free(mipLevelPixels);

	fsCloseStream(&fh);

	return true;
}

bool AssetPipeline::ProcessVirtualTextures(ProcessAssetsSettings* settings)
{
	// Get all image files
	eastl::vector<eastl::string> ddsFilesInDirectory;
	fsGetFilesWithExtension(RD_INPUT, "", ".dds", ddsFilesInDirectory);

	for (size_t i = 0; i < ddsFilesInDirectory.size(); ++i)
	{
		eastl::string outputFile = ddsFilesInDirectory[i];

		if (outputFile.size() > 0)
		{
			TextureDesc textureDesc = {};
			FileStream ddsFile = {};
			fsOpenStreamFromPath(RD_INPUT, ddsFilesInDirectory[i].c_str(), FM_READ_BINARY, &ddsFile);
			bool success = false;
			if (!fsOpenStreamFromPath(RD_INPUT, ddsFilesInDirectory[i].c_str(), FM_READ_BINARY, &ddsFile))
			{
				continue;
			}

			success = loadDDSTextureDesc(&ddsFile, &textureDesc);

			if (!success)
			{
				fsCloseStream(&ddsFile);
				LOGF(LogLevel::eERROR, "Failed to load image %s.", outputFile.c_str());
				continue;
			}

			outputFile.resize(outputFile.size() - 4);
			outputFile.append(".svt");

			SVT_HEADER header = {};
			header.mComponentCount = 4;
			header.mHeight = textureDesc.mHeight;
			header.mMipLevels = textureDesc.mMipLevels;
			header.mPageSize = 128;
			header.mWidth = textureDesc.mWidth;

			success = SaveSVT(outputFile.c_str(), &ddsFile, &header);

			fsCloseStream(&ddsFile);

			if (!success)
			{
				LOGF(LogLevel::eERROR, "Failed to save sparse virtual texture %s.", outputFile.c_str());
				continue;
			}
		}
	}

	return true;
}

bool AssetPipeline::ProcessTFX(ProcessAssetsSettings* settings)
{
	cgltf_result result = cgltf_result_success;

	// Get all tfx files
	eastl::vector<eastl::string> tfxFilesInDirectory;
	fsGetFilesWithExtension(RD_INPUT, "", ".tfx", tfxFilesInDirectory);

#define RETURN_IF_TFX_ERROR(expression) if (!(expression)) { LOGF(eERROR, "Failed to load tfx"); return false; }

	for (size_t i = 0; i < tfxFilesInDirectory.size(); ++i)
	{
		const char* input = tfxFilesInDirectory[i].c_str();
		char outputTemp[FS_MAX_PATH] = {};
		fsGetPathFileName(input, outputTemp);
		char output[FS_MAX_PATH] = {};
		fsAppendPathExtension(outputTemp, "gltf", output);

		char binFilePath[FS_MAX_PATH] = {};
		fsAppendPathExtension(outputTemp, "bin", binFilePath);

		FileStream tfxFile = {};
		fsOpenStreamFromPath(RD_INPUT, input, FM_READ_BINARY, &tfxFile);
		AMD::TressFXAsset tressFXAsset = {};
		RETURN_IF_TFX_ERROR(tressFXAsset.LoadHairData(&tfxFile))
			fsCloseStream(&tfxFile);

		if (settings->mFollowHairCount)
		{
			RETURN_IF_TFX_ERROR(tressFXAsset.GenerateFollowHairs(settings->mFollowHairCount, settings->mTipSeperationFactor, settings->mMaxRadiusAroundGuideHair))
		}

		RETURN_IF_TFX_ERROR(tressFXAsset.ProcessAsset())

			struct TypePair { cgltf_type type; cgltf_component_type comp; };
		const TypePair vertexTypes[] =
		{
			{ cgltf_type_scalar, cgltf_component_type_r_32u },   // Indices
			{ cgltf_type_vec4,   cgltf_component_type_r_32f },   // Position
			{ cgltf_type_vec4,   cgltf_component_type_r_32f },   // Tangents
			{ cgltf_type_vec4,   cgltf_component_type_r_32f },   // Global rotations
			{ cgltf_type_vec4,   cgltf_component_type_r_32f },   // Local rotations
			{ cgltf_type_vec4,   cgltf_component_type_r_32f },   // Ref vectors
			{ cgltf_type_vec4,   cgltf_component_type_r_32f },   // Follow root offsets
			{ cgltf_type_vec2,   cgltf_component_type_r_32f },   // Strand UVs
			{ cgltf_type_scalar, cgltf_component_type_r_32u },   // Strand types
			{ cgltf_type_scalar, cgltf_component_type_r_32f },   // Thickness coeffs
			{ cgltf_type_scalar, cgltf_component_type_r_32f },   // Rest lengths
		};
		const uint32_t vertexStrides[] =
		{
			sizeof(uint32_t), // Indices
			sizeof(float4),   // Position
			sizeof(float4),   // Tangents
			sizeof(float4),   // Global rotations
			sizeof(float4),   // Local rotations
			sizeof(float4),   // Ref vectors
			sizeof(float4),   // Follow root offsets
			sizeof(float2),   // Strand UVs
			sizeof(uint32_t), // Strand types
			sizeof(float),    // Thickness coeffs
			sizeof(float),    // Rest lengths
		};
		const uint32_t vertexCounts[] =
		{
			(uint32_t)tressFXAsset.GetNumHairTriangleIndices(),   // Indices
			(uint32_t)tressFXAsset.m_numTotalVertices,   // Position
			(uint32_t)tressFXAsset.m_numTotalVertices,   // Tangents
			(uint32_t)tressFXAsset.m_numTotalVertices,   // Global rotations
			(uint32_t)tressFXAsset.m_numTotalVertices,   // Local rotations
			(uint32_t)tressFXAsset.m_numTotalVertices,   // Ref vectors
			(uint32_t)tressFXAsset.m_numTotalStrands,    // Follow root offsets
			(uint32_t)tressFXAsset.m_numTotalStrands,    // Strand UVs
			(uint32_t)tressFXAsset.m_numTotalStrands,    // Strand types
			(uint32_t)tressFXAsset.m_numTotalVertices,   // Thickness coeffs
			(uint32_t)tressFXAsset.m_numTotalVertices,   // Rest lengths
		};
		const void* vertexData[] =
		{
			tressFXAsset.m_triangleIndices,    // Indices
			tressFXAsset.m_positions,          // Position
			tressFXAsset.m_tangents,           // Tangents
			tressFXAsset.m_globalRotations,    // Global rotations
			tressFXAsset.m_localRotations,     // Local rotations
			tressFXAsset.m_refVectors,         // Ref vectors
			tressFXAsset.m_followRootOffsets,  // Follow root offsets
			tressFXAsset.m_strandUV,           // Strand UVs
			tressFXAsset.m_strandTypes,        // Strand types
			tressFXAsset.m_thicknessCoeffs,    // Thickness coeffs
			tressFXAsset.m_restLengths,        // Rest lengths
		};
		const char* vertexNames[] =
		{
			"INDEX",             // Indices
			"POSITION",          // Position
			"TANGENT",           // Tangents
			"TEXCOORD_0",        // Global rotations
			"TEXCOORD_1",        // Local rotations
			"TEXCOORD_2",        // Ref vectors
			"TEXCOORD_3",        // Follow root offsets
			"TEXCOORD_4",        // Strand UVs
			"TEXCOORD_5",        // Strand types
			"TEXCOORD_6",        // Thickness coeffs
			"TEXCOORD_7",        // Rest lengths
		};
		const uint32_t count = sizeof(vertexData) / sizeof(vertexData[0]);

		cgltf_buffer buffer = {};
		cgltf_accessor accessors[count] = {};
		cgltf_buffer_view views[count] = {};
		cgltf_attribute attribs[count] = {};
		cgltf_mesh mesh = {};
		cgltf_primitive prim = {};
		cgltf_size offset = 0;
		FileStream binFile = {};
		fsOpenStreamFromPath(RD_OUTPUT, binFilePath, FM_WRITE_BINARY, &binFile);
		size_t fileSize = 0;

		for (uint32_t i = 0; i < count; ++i)
		{
			views[i].type = (i ? cgltf_buffer_view_type_vertices : cgltf_buffer_view_type_indices);
			views[i].buffer = &buffer;
			views[i].offset = offset;
			views[i].size = vertexCounts[i] * vertexStrides[i];
			accessors[i].component_type = vertexTypes[i].comp;
			accessors[i].stride = vertexStrides[i];
			accessors[i].count = vertexCounts[i];
			accessors[i].offset = 0;
			accessors[i].type = vertexTypes[i].type;
			accessors[i].buffer_view = &views[i];

			attribs[i].name = (char*)vertexNames[i];
			attribs[i].data = &accessors[i];

			fileSize += fsWriteToStream(&binFile, vertexData[i], views[i].size);
			offset += views[i].size;
		}
		fsCloseStream(&binFile);

		char uri[FS_MAX_PATH] = {};
		fsGetPathFileName(binFilePath, uri);
		//sprintf(uri, "%s", fn.buffer);
		buffer.uri = uri;
		buffer.size = fileSize;

		prim.indices = accessors;
		prim.attributes_count = count - 1;
		prim.attributes = attribs + 1;
		prim.type = cgltf_primitive_type_triangles;

		mesh.primitives_count = 1;
		mesh.primitives = &prim;

		char extras[128] = {};
		sprintf(extras, "{ \"%s\" : %d, \"%s\" : %d }",
			"mVertexCountPerStrand", tressFXAsset.m_numVerticesPerStrand, "mGuideCountPerStrand", tressFXAsset.m_numGuideStrands);

		char generator[] = "TressFX";
		cgltf_data data = {};
		data.asset.generator = generator;
		data.buffers_count = 1;
		data.buffers = &buffer;
		data.buffer_views_count = count;
		data.buffer_views = views;
		data.accessors_count = count;
		data.accessors = accessors;
		data.meshes_count = 1;
		data.meshes = &mesh;
		data.file_data = extras;
		data.asset.extras.start_offset = 0;
		data.asset.extras.end_offset = strlen(extras);
		result = cgltf_write(output, &data);
	}

	return result == cgltf_result_success;
}

static uint32_t FindJoint(ozz::animation::Skeleton* skeleton, const char* name)
{
	for (int i = 0; i < skeleton->num_joints(); i++)
	{
		if (strcmp(skeleton->joint_names()[i], name) == 0)
			return i;
	}
	return UINT_MAX;
}

static bool isTrsDecomposable(Matrix4 matrix) {
	if (matrix.getCol0().getW() != 0.0 ||
		matrix.getCol1().getW() != 0.0 ||
		matrix.getCol2().getW() != 0.0 ||
		matrix.getCol3().getW() != 1.0) {
		return false;
	}

	if (determinant(matrix) == 0.0) {
		return false;
	}

	return true;
}

bool AssetPipeline::CreateRuntimeSkeleton(
	const char* skeletonAsset, const char* skeletonName, const char* skeletonOutput, ozz::animation::Skeleton* skeleton,
	ProcessAssetsSettings* settings)
{
	cgltf_data* data = NULL;
	void* srcFileData = NULL;
	cgltf_result result = cgltf_parse_and_load(skeletonAsset, &data, &srcFileData);
	if (cgltf_result_success != result)
	{
		return false;
	}

	if (data->skins_count == 0)
	{
		LOGF(LogLevel::eERROR, "Rigged mesh %s has no bones. Skeleton can not be created.", skeletonName);
		return false;
	}

	// Gather node info
	// Used to mark nodes that should be included in the skeleton
	eastl::vector<NodeInfo> nodeData(1);
	size_t startIndex = data->nodes_count - 1;
	nodeData[0] = { data->nodes[startIndex].name, -1, {}, false, &data->nodes[startIndex] };

	const int queueSize = 128;
	int       nodeQueue[queueSize] = {};    // Simple queue because tinystl doesn't have one
	for (int i = 0; i < queueSize; ++i)
		nodeQueue[i] = -1;
	nodeQueue[0] = 0;
	int nodeQueueStart = 0;
	int nodeQueueEnd = 1;
	while (nodeQueue[nodeQueueStart] != -1)
	{
		// Pop
		int     nodeIndex = nodeQueue[nodeQueueStart];
		cgltf_node *node = nodeData[nodeIndex].pNode;
		nodeQueue[nodeQueueStart] = -1;

		for (uint i = 0; i < node->children_count; ++i)
		{
			NodeInfo childNode = {};
			childNode.mName = node->children[i]->name;
			childNode.pParentNodeIndex = nodeIndex;
			int childNodeIndex = (int)nodeData.size();
			childNode.pNode = node->children[i];
			nodeData.push_back(childNode);

			nodeData[nodeIndex].mChildNodeIndices.push_back(childNodeIndex);

			// Push
			nodeQueue[nodeQueueEnd] = childNodeIndex;
			nodeQueueEnd = (nodeQueueEnd + 1) % queueSize;
			if (nodeQueueStart == nodeQueueEnd)
			{
				LOGF(LogLevel::eERROR, "Too many nodes in scene. Skeleton can not be created.", skeletonName);
				return false;
			}
		}

		nodeQueueStart = (nodeQueueStart + 1) % queueSize;
	}

	// Mark all nodes that are required to be in the skeleton
	for (uint i = 0; i < data->skins_count; ++i)
	{
		cgltf_skin* skin = &data->skins[i];
		for (uint j = 0; j < skin->joints_count; ++j)
		{
			cgltf_node* bone = skin->joints[j];
			for (uint k = 0; k < (uint)nodeData.size(); ++k)
			{
				if (nodeData[k].mName == eastl::string(bone->name))
				{
					int nodeIndex = k;
					while (nodeIndex != -1)
					{
						if (nodeData[nodeIndex].mUsedInSkeleton)
							break;    // Remaining part of the tree is already marked
						nodeData[nodeIndex].mUsedInSkeleton = true;
						nodeIndex = nodeData[nodeIndex].pParentNodeIndex;
					}
				}
			}
		}
	}

	// Create raw skeleton
	ozz::animation::offline::RawSkeleton rawSkeleton;

	eastl::vector<BoneInfo> boneData(1);
	boneData[0] = { 0, -1, NULL };

	while (!boneData.empty())
	{
		BoneInfo boneInfo = boneData.back();
		boneData.pop_back();
		NodeInfo* nodeInfo = &nodeData[boneInfo.mNodeIndex];
		cgltf_node*   node = nodeInfo->pNode;

		// Get node transform

		// Create joint from node
		ozz::animation::offline::RawSkeleton::Joint joint;

		if (!node->has_matrix)
		{
			joint.transform.translation = vec3(node->translation[0], node->translation[1], node->translation[2]);
			joint.transform.rotation = Quat(node->rotation[0], node->rotation[1], node->rotation[2], node->rotation[3]);
			joint.transform.scale = vec3(node->scale[0], node->scale[1], node->scale[2]);// *(boneInfo.mParentNodeIndex == -1 ? 0.01f : 1.f);
		}
		else
		{
			// Matrix Decomposition
			Matrix4 mat;
			mat.setCol0(vec4(node->matrix[0], node->matrix[1], node->matrix[2], node->matrix[3]));
			mat.setCol1(vec4(node->matrix[4], node->matrix[5], node->matrix[6], node->matrix[7]));
			mat.setCol2(vec4(node->matrix[8], node->matrix[9], node->matrix[10], node->matrix[11]));
			mat.setCol3(vec4(node->matrix[12], node->matrix[13], node->matrix[14], node->matrix[15]));

			if (isTrsDecomposable(mat))
			{
				// extract translation
				joint.transform.translation = mat.getTranslation();

				// extract the scaling factors from columns of the matrix
				Matrix3 upperMat = mat.getUpper3x3();
				vec3 pScaling = vec3(length(upperMat.getCol0()), length(upperMat.getCol1()), length(upperMat.getCol2()));

				// and the sign of the scaling 
				if (determinant(mat) < 0) pScaling = -pScaling;
				joint.transform.scale = pScaling;//*(boneInfo.mParentNodeIndex == -1 ? 0.01f : 1.f);

				// and remove all scaling from the matrix 
				if (pScaling.getX()) upperMat.setCol0(upperMat.getCol0() / pScaling.getX());
				if (pScaling.getY()) upperMat.setCol1(upperMat.getCol1() / pScaling.getY());
				if (pScaling.getZ()) upperMat.setCol2(upperMat.getCol2() / pScaling.getZ());

				// and generate the rotation quaternion from it
				joint.transform.rotation = Quat(upperMat);
			}
		}

		joint.name = nodeInfo->mName.c_str();

		// Add node to raw skeleton
		ozz::animation::offline::RawSkeleton::Joint* newParentJoint = NULL;
		if (boneInfo.pParentJoint == NULL)
		{
			rawSkeleton.roots.push_back(joint);
			newParentJoint = &rawSkeleton.roots.back();
		}
		else
		{
			boneInfo.pParentJoint->children.push_back(joint);
			newParentJoint = &boneInfo.pParentJoint->children.back();
		}

		// Count the child nodes that are required to be in the skeleton
		int requiredChildCount = 0;
		for (uint i = 0; i < (uint)nodeInfo->mChildNodeIndices.size(); ++i)
		{
			NodeInfo* childNodeInfo = &nodeData[nodeInfo->mChildNodeIndices[i]];
			if (childNodeInfo->mUsedInSkeleton)
				++requiredChildCount;
		}

		// Add child nodes to the list of nodes to process
		newParentJoint->children.reserve(requiredChildCount);    // Reserve to make sure memory isn't reallocated later.
		for (uint i = 0; i < (uint)nodeInfo->mChildNodeIndices.size(); ++i)
		{
			NodeInfo* childNodeInfo = &nodeData[nodeInfo->mChildNodeIndices[i]];
			if (childNodeInfo->mUsedInSkeleton)
			{
				boneInfo.mNodeIndex = nodeInfo->mChildNodeIndices[i];
				boneInfo.mParentNodeIndex = boneInfo.mNodeIndex;
				boneInfo.pParentJoint = newParentJoint;
				boneData.push_back(boneInfo);
			}
		}
	}

	// Validate raw skeleton
	if (!rawSkeleton.Validate())
	{
		LOGF(LogLevel::eERROR, "Skeleton created for %s is invalid. Skeleton can not be created.", skeletonName);
		return false;
	}

	// Build runtime skeleton from raw skeleton
	if (!ozz::animation::offline::SkeletonBuilder::Build(rawSkeleton, skeleton))
	{
		LOGF(LogLevel::eERROR, "Skeleton can not be created for %s.", skeletonName);
		return false;
	}

	// Write skeleton to disk
	FileStream file = {};
	if (!fsOpenStreamFromPath(RD_OUTPUT, skeletonOutput, FM_WRITE_BINARY, &file))
		return false;

	ozz::io::OArchive archive(&file);
	archive << *skeleton;
	fsCloseStream(&file);

	void* fileData = data->file_data;

	// Generate joint remaps in the gltf
	{
		eastl::vector<char> buffer;
		uint32_t size = 0;

		for (uint32_t i = 0; i < data->meshes_count; ++i)
		{
			cgltf_mesh* mesh = &data->meshes[i];
			cgltf_node* node = NULL;
			for (uint32_t n = 0; n < data->nodes_count; ++n)
			{
				if (data->nodes[n].mesh == mesh)
				{
					node = &data->nodes[n];
					break;
				}
			}

			if (node && node->mesh && node->skin)
			{
				cgltf_skin* skin = node->skin;

				char jointRemaps[1024] = {};
				uint32_t offset = 0;
				offset += sprintf(jointRemaps + offset, "[ ");

				for (uint32_t j = 0; j < skin->joints_count; ++j)
				{
					const cgltf_node* jointNode = skin->joints[j];
					uint32_t jointIndex = FindJoint(skeleton, jointNode->name);
					if (j == 0)
						offset += sprintf(jointRemaps + offset, "%u", jointIndex);
					else
						offset += sprintf(jointRemaps + offset, ", %u", jointIndex);
				}

				offset += sprintf(jointRemaps + offset, " ]");
				skin->extras.start_offset = size;
				size += (uint32_t)strlen(jointRemaps) + 1;
				skin->extras.end_offset = size - 1;

				for (uint32_t i = 0; i < strlen(jointRemaps) + 1; ++i)
					buffer.push_back(jointRemaps[i]);
			}
		}

		for (uint32_t i = 0; i < data->buffers_count; ++i)
		{
			if (strstr(data->buffers[i].uri, "://") == NULL)
			{
				char parentPath[FS_MAX_PATH] = {};
				fsGetParentPath(skeletonAsset, parentPath);
				char bufferOut[FS_MAX_PATH] = {};
				fsAppendPathComponent(parentPath, data->buffers[i].uri, bufferOut);
				FileStream fs = {};
				fsOpenStreamFromPath(RD_OUTPUT, bufferOut, FM_WRITE_BINARY, &fs);
				fsWriteToStream(&fs, data->buffers[i].data, data->buffers[i].size);
				fsCloseStream(&fs);
			}
		}

		data->file_data = buffer.data();
		if (cgltf_result_success != cgltf_write(skeletonAsset, data))
		{
			return false;
		}
	}

	data->file_data = srcFileData;
	cgltf_free(data);

	return true;
}

bool AssetPipeline::CreateRuntimeAnimation(
	const char* animationAsset, ozz::animation::Skeleton* skeleton, const char* skeletonName, const char* animationName,
	const char* animationOutput, ProcessAssetsSettings* settings)
{
	// Import the glTF with the animation
	cgltf_data* data = NULL;
	void* srcFileData = NULL;
	cgltf_result result = cgltf_parse_and_load(animationAsset, &data, &srcFileData);
	if (result != cgltf_result_success)
	{
		return false;
	}

	// Check if the asset contains any animations
	if (data->animations_count == 0)
	{
		if (!settings->quiet)
			LOGF(LogLevel::eWARNING, "Animation asset %s of skeleton %s contains no animations.", animationName, skeletonName);
		return false;
	}

	// Create raw animation
	ozz::animation::offline::RawAnimation rawAnimation;
	rawAnimation.name = animationName;
	rawAnimation.duration = 0.f;

	bool rootFound = false;
	rawAnimation.tracks.resize(skeleton->num_joints());
	for (uint i = 0; i < (uint)skeleton->num_joints(); ++i)
	{
		const char* jointName = skeleton->joint_names()[i];

		ozz::animation::offline::RawAnimation::JointTrack* track = &rawAnimation.tracks[i];

		for (cgltf_size animationIndex = 0; animationIndex < data->animations_count; animationIndex += 1)
		{
			cgltf_animation* animationData = &data->animations[animationIndex];

			for (cgltf_size channelIndex = 0; channelIndex < animationData->channels_count; channelIndex += 1)
			{
				cgltf_animation_channel* channel = &animationData->channels[channelIndex];

				if (strcmp(channel->target_node->name, jointName) != 0) continue;

				if (channel->target_path == cgltf_animation_path_type_translation)
				{
					track->translations.resize(channel->sampler->output->count);
					for (cgltf_size j = 0; j < channel->sampler->output->count; j += 1)
					{
						float time = 0.0;
						vec3 translation = vec3(0.0f);
						cgltf_accessor_read_float(channel->sampler->input, j, &time, 1);
						cgltf_accessor_read_float(channel->sampler->output, j, (float*)&translation, 3);

						track->translations[j] = { time, translation };
					}
				}

				if (channel->target_path == cgltf_animation_path_type_rotation)
				{
					track->rotations.resize(channel->sampler->output->count);
					for (cgltf_size j = 0; j < channel->sampler->output->count; j += 1)
					{
						float time = 0.0;
						Quat rotation = Quat(0.0f);
						cgltf_accessor_read_float(channel->sampler->input, j, &time, 1);
						cgltf_accessor_read_float(channel->sampler->output, j, (float*)&rotation, 4);

						track->rotations[j] = { time, rotation };
					}
				}

				if (channel->target_path == cgltf_animation_path_type_scale)
				{
					track->scales.resize(channel->sampler->output->count);
					for (cgltf_size j = 0; j < channel->sampler->output->count; j += 1)
					{
						float time = 0.0;
						vec3 scale = vec3(0.0f);
						cgltf_accessor_read_float(channel->sampler->input, j, &time, 1);
						cgltf_accessor_read_float(channel->sampler->output, j, (float*)&scale, 3);

						track->scales[j] = { time, scale };
					}
				}
			}
		}

		if (!track->translations.empty())
			rawAnimation.duration = max(rawAnimation.duration, track->translations.back().time);

		if (!track->rotations.empty())
			rawAnimation.duration = max(rawAnimation.duration, track->rotations.back().time);

		if (!track->scales.empty())
			rawAnimation.duration = max(rawAnimation.duration, track->scales.back().time);
	}

	tf_free(srcFileData);
	cgltf_free(data);

	// Validate raw animation
	if (!rawAnimation.Validate())
	{
		LOGF(LogLevel::eERROR, "Animation %s created for %s is invalid. Animation can not be created.", animationName, skeletonName);
		return false;
	}

	// Build runtime animation from raw animation
	ozz::animation::Animation animation;
	if (!ozz::animation::offline::AnimationBuilder::Build(rawAnimation, &animation))
	{
		LOGF(LogLevel::eERROR, "Animation %s can not be created for %s.", animationName, skeletonName);
		return false;
	}

	// Write animation to disk
	FileStream file = {};

	if (!fsOpenStreamFromPath(RD_OUTPUT, animationOutput, FM_WRITE_BINARY, &file))
		return false;

	ozz::io::OArchive archive(&file);
	archive << animation;
	fsCloseStream(&file);
	//Deallocate animation
	animation.Deallocate();

	return true;
}
