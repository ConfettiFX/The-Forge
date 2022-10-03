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

#include "AssetPipeline.h"

// Math
#include "../../../Utilities/ThirdParty/OpenSource/ModifiedSonyMath/vectormath.hpp"

// OZZ
#include "../../../Resources/AnimationSystem/ThirdParty/OpenSource/ozz-animation/include/ozz/base/io/archive.h"
#include "../../../Resources/AnimationSystem/ThirdParty/OpenSource/ozz-animation/include/ozz/animation/offline/raw_skeleton.h"
#include "../../../Resources/AnimationSystem/ThirdParty/OpenSource/ozz-animation/include/ozz/animation/offline/raw_animation.h"
#include "../../../Resources/AnimationSystem/ThirdParty/OpenSource/ozz-animation/include/ozz/animation/offline/skeleton_builder.h"
#include "../../../Resources/AnimationSystem/ThirdParty/OpenSource/ozz-animation/include/ozz/animation/offline/animation_builder.h"
#include "../../../Resources/AnimationSystem/ThirdParty/OpenSource/ozz-animation/include/ozz/animation/offline/animation_optimizer.h"
#include "../../../Resources/AnimationSystem/ThirdParty/OpenSource/ozz-animation/include/ozz/animation/offline/track_optimizer.h"

#include "../../../Resources/ResourceLoader/ThirdParty/OpenSource/tinyimageformat/tinyimageformat_base.h"

// TressFX
#include "../../../Resources/AnimationSystem/ThirdParty/OpenSource/TressFX/TressFXAsset.h"

#define CGLTF_WRITE_IMPLEMENTATION
#define CGLTF_IMPLEMENTATION
#include "../../../Resources/ResourceLoader/ThirdParty/OpenSource/cgltf/cgltf_write.h"

#define TINYDDS_IMPLEMENTATION
#define TINYKTX_IMPLEMENTATION
#include "../../../Resources/ResourceLoader/TextureContainers.h"

// ASTC
#include "../../../Resources/ResourceLoader/ThirdParty/OpenSource/astc-encoder/Source/astcenc.h"
#include "../../../Resources/ResourceLoader/ThirdParty/OpenSource/astc-encoder/Source/astcenccli_internal.h"

// minizip
#include "../../../Utilities/ThirdParty/OpenSource/minizip/mz.h"
#include "../../../Utilities/ThirdParty/OpenSource/minizip/mz_crypt.h"
#include "../../../Utilities/ThirdParty/OpenSource/minizip/mz_os.h"
#include "../../../Utilities/ThirdParty/OpenSource/minizip/mz_zip.h"
#include "../../../Utilities/ThirdParty/OpenSource/minizip/mz_strm.h"

#include "../../../Resources/ResourceLoader/Interfaces/IResourceLoader.h"
#include "../../../OS/Interfaces/IOperatingSystem.h"
#include "../../../Utilities/Interfaces/IFileSystem.h"
#include "../../../Utilities/Interfaces/ILog.h"

#include "../../../Utilities/Interfaces/IToolFileSystem.h"

#include "../../../Utilities/Interfaces/IMemory.h"    //NOTE: this should be the last include in a .cpp

struct SkeletonNodeInfo
{
	bstring				mName = bempty();
	cgltf_node*			pNode = nullptr;
	int32_t*			mChildNodeIndices = nullptr;
	int32_t             pParentNodeIndex = 0;
	bool                mUsedInSkeleton = false;
};

struct SkeletonBoneInfo
{
	int32_t                                      mNodeIndex;
	int32_t                                      mParentNodeIndex;
	ozz::animation::offline::RawSkeleton::Joint* pParentJoint;
};

static void BeginAssetPipelineSection(const char* section)
{
	LOGF(eINFO, "========== %s ==========", section);
}

static void EndAssetPipelineSection(const char* section)
{
	LOGF(eINFO, "========== !%s ==========", section);
}

void ReleaseSkeletonAndAnimationParams(SkeletonAndAnimations* pArray, uint32_t count)
{
	for (uint32_t i = 0; i < count; ++i)
	{
		bdestroy(&pArray[i].mSkeletonName);
		bdestroy(&pArray[i].mSkeletonInFile);
		bdestroy(&pArray[i].mSkeletonOutFile);

		for (uint32_t a = 0, end = (uint32_t)arrlen(pArray[i].mAnimations); a < end; ++a)
		{
			bdestroy(&pArray[i].mAnimations[a].mInputAnim);
			bdestroy(&pArray[i].mAnimations[a].mOutputAnimPath);
		}

		arrfree(pArray[i].mAnimations);
	}
}

void ReleaseSkeletonAndAnimationParams(SkeletonAndAnimations* pStbdsArray)
{
	ReleaseSkeletonAndAnimationParams(pStbdsArray, (uint32_t)arrlen(pStbdsArray));
	arrfree(pStbdsArray);
}

cgltf_result cgltf_parse_and_load(ResourceDirectory resourceDir, const char* skeletonAsset, cgltf_data** ppData, void** ppFileData)
{
	// Import the glTF with the animation
	FileStream file = {};
	if (!fsOpenStreamFromPath(resourceDir, skeletonAsset, FM_READ_BINARY, NULL, &file))
	{
		LOGF(eERROR, "Failed to open gltf file %s", skeletonAsset);
		ASSERT(false);
		return cgltf_result_file_not_found;
	}

	ssize_t fileSize = fsGetStreamFileSize(&file);
	void* fileData = tf_malloc(fileSize);

	fsReadFromStream(&file, fileData, fileSize);

	cgltf_data* data = NULL;
	cgltf_options options = {};
	options.memory_alloc = [](void* user, cgltf_size size) { return tf_malloc(size); };
	options.memory_free = [](void* user, void* ptr) { tf_free(ptr); };
	cgltf_result result = cgltf_parse(&options, fileData, fileSize, &data);
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
			if (fsOpenStreamFromPath(resourceDir, path, FM_READ_BINARY, NULL, &fs))
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

cgltf_result cgltf_write(ResourceDirectory resourceDir, const char* skeletonAsset, cgltf_data* data)
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
	if (!fsOpenStreamFromPath(resourceDir, skeletonAsset, FM_WRITE, NULL, &file))
	{
		tf_free(writeBuffer);
		return cgltf_result_io_error;
	}

	fsWriteToStream(&file, writeBuffer, actual - 1);
	fsCloseStream(&file);
	tf_free(writeBuffer);

	return cgltf_result_success;
}

void DiscoverAnimations(ResourceDirectory resourceDirInput, SkeletonAndAnimations** pOutArray)
{
	BeginAssetPipelineSection("DiscoverAnimations");
	LOGF(eINFO, "Discovering animations assets: %s", fsGetResourceDirectory(resourceDirInput));

	// Check for assets containing animations in animationDirectory
	SkeletonAndAnimations* pSkeletonAndAnims = nullptr;

	char** subDirectories = NULL;
	int subDirectoryCount = 0;
	fsGetSubDirectories(resourceDirInput, "", &subDirectories, &subDirectoryCount);
	for (int i = 0; i < subDirectoryCount; ++i)
	{
		const char* subDir = subDirectories[i];
		LOGF(eINFO, "Exploring subdir: %s", subDir);

		// Get all glTF files
		char** filesInDirectory = NULL;
		int filesFoundInDirectory = 0;
		fsGetFilesWithExtension(resourceDirInput, subDir, ".gltf", &filesInDirectory, &filesFoundInDirectory);

		for (int j = 0; j < filesFoundInDirectory; ++j)
		{
			const char* file = filesInDirectory[j];

			char skeletonName[FS_MAX_PATH] = {};
			fsGetPathFileName(file, skeletonName);
			if (!stricmp(skeletonName, "riggedmesh"))
			{
				// Add new animation asset named after the folder it is in
				char assetName[FS_MAX_PATH] = {};
				fsGetPathFileName(subDir, assetName);
				LOGF(eINFO, "Found Rigged Mesh: %s/%s.gtlf", assetName, "riggedmesh");

				// Create skeleton output file name
				char skeletonOutputDir[FS_MAX_PATH] = {};
				fsAppendPathComponent("", assetName, skeletonOutputDir);
				char skeletonOutput[FS_MAX_PATH] = {};
				fsAppendPathComponent(skeletonOutputDir, "skeleton.ozz", skeletonOutput);

				SkeletonAndAnimations skeletonAndAnims = {};
				skeletonAndAnims.mSkeletonName = bdynfromcstr(assetName);
				skeletonAndAnims.mSkeletonInFile = bdynfromcstr(file);
				skeletonAndAnims.mSkeletonOutFile = bdynfromcstr(skeletonOutput);

				// Find sub directory called animations
				char** assetSubDirectories = NULL;
				int assetSubDirectoryCount = 0;
				fsGetSubDirectories(resourceDirInput, subDir, &assetSubDirectories, &assetSubDirectoryCount);
				if (assetSubDirectories)
				{
					for (int k = 0; k < assetSubDirectoryCount; ++k)
					{
						const char* assetSubDir = assetSubDirectories[k];

						char subDir[FS_MAX_PATH] = {};
						fsGetPathFileName(assetSubDir, subDir);
						if (!stricmp(subDir, "animations"))
						{
							// Add all files in animations to the animation asset
							char** animationFiles = NULL;
							int animationFileCount = 0;
							fsGetFilesWithExtension(resourceDirInput, assetSubDir, ".gltf", &animationFiles, &animationFileCount);
							if (animationFiles)
							{
								for (int curAnim = 0; curAnim < animationFileCount; ++curAnim)
								{
									// Create animation output file name
									bstring animOutputFileString = bempty();
									bcatcstr(&animOutputFileString, assetName);
									bcatcstr(&animOutputFileString, "/animations/");

									SkeletonAndAnimations::AnimationFile anim = {};
									anim.mInputAnim = bdynfromcstr(animationFiles[curAnim]);
									anim.mOutputAnimPath = animOutputFileString;
									arrpush(skeletonAndAnims.mAnimations, anim);

									LOGF(eINFO, "Found Animation file '%s' animations will be outputted to '%s'", (const char*)anim.mInputAnim.data, (const char*)anim.mOutputAnimPath.data);
								}
								tf_free(animationFiles);
							}
							break;
						}
					}

					tf_free(assetSubDirectories);
				}

				arrpush(pSkeletonAndAnims, skeletonAndAnims);
				break;
			}
		}

		if (filesInDirectory)
		{
			tf_free(filesInDirectory);
		}
	}

	if (subDirectories)
	{
		tf_free(subDirectories);
	}

	const uint32_t discoveredSkeletons = (uint32_t)arrlen(pSkeletonAndAnims);
	uint32_t discoveredAnimFiles = 0;
	for (uint32_t i = 0; i < discoveredSkeletons; ++i)
	{
		discoveredAnimFiles += (uint32_t)arrlen(pSkeletonAndAnims[i].mAnimations);
	}

	LOGF(eINFO, "Discovered %d skeletons and %d animation files", discoveredSkeletons, discoveredAnimFiles);
	EndAssetPipelineSection("DiscoverAnimations");

	*pOutArray = pSkeletonAndAnims;
}

bool ProcessAnimations(AssetPipelineParams* assetParams, ProcessAnimationsParams* pProcessAnimationsParams)
{
	BeginAssetPipelineSection("ProcessAnimations");
	LOGF(eINFO, "Processing animations and writing to directory: %s", fsGetResourceDirectory(assetParams->mRDOutput));

	const SkeletonAndAnimations* pSkeletonAndAnims = pProcessAnimationsParams->pSkeletonAndAnims;
	for (uint32_t i = 0; i < (uint32_t)arrlen(pSkeletonAndAnims); ++i)
	{
		if (arrlen(pSkeletonAndAnims[i].mAnimations) == 0)
			LOGF(LogLevel::eWARNING, "Skeleton '%s' has no associated animations.", (char*)pSkeletonAndAnims[i].mSkeletonInFile.data);
	}

	// No assets found. Return.
	if (arrlen(pSkeletonAndAnims) == 0)
	{
		LOGF(eINFO, "No animation files found to process");
		EndAssetPipelineSection("ProcessAnimations");
		return true;
	}

	LOGF(eINFO, "Processing %d skeleton assets and their animations...", (uint32_t)arrlen(pSkeletonAndAnims));

	// Process the found assets
	uint32_t assetsProcessed = 0;
	uint32_t assetsChecked = 0;
	bool success = true;
	for (uint32_t i = 0, end = (uint32_t)arrlen(pSkeletonAndAnims); i < end; ++i)
	{
		const SkeletonAndAnimations* skeletonAndAnims = &pSkeletonAndAnims[i];
		const char* skeletonName = (char*)skeletonAndAnims->mSkeletonName.data;
		const char* skeletonInputFile = (char*)skeletonAndAnims->mSkeletonInFile.data;

		// Create skeleton output file name
		char skeletonOutputDir[FS_MAX_PATH] = {};
		fsAppendPathComponent("", skeletonName, skeletonOutputDir);
		char skeletonOutput[FS_MAX_PATH] = {};
		fsAppendPathComponent(skeletonOutputDir, "skeleton.ozz", skeletonOutput);

		assetsChecked++;

		// Check if the skeleton is already up-to-date
		bool processSkeleton = true;
		if (!assetParams->mSettings.force)
		{
			time_t lastModified = fsGetLastModifiedTime(assetParams->mRDInput, skeletonInputFile);
			time_t lastProcessed = fsGetLastModifiedTime(assetParams->mRDOutput, skeletonOutput);

			if (lastModified < lastProcessed && lastProcessed != ~0u && lastProcessed > assetParams->mSettings.minLastModifiedTime)
				processSkeleton = false;
		}

		ozz::animation::Skeleton skeleton;
		if (processSkeleton)
		{
			LOGF(eINFO, "Regenerating Skeleton: %s -> %s", skeletonInputFile, skeletonOutput);

			// Process the skeleton
			if (CreateRuntimeSkeleton(assetParams->mRDInput, skeletonInputFile, assetParams->mRDOutput, skeletonOutput, &skeleton, skeletonName, &assetParams->mSettings))
			{
				LOGF(eERROR, "Couldn't create Skeleton for mesh '%s'. Skipping asset and all it's animations", skeletonInputFile, skeletonOutput);
				success = false;
				continue;
			}

			++assetsProcessed;
		}
		else
		{
			LOGF(eINFO, "Skeleton for mesh '%s' up to date, loading from disk: %s", skeletonInputFile, skeletonOutput);

			// Load skeleton from disk
			FileStream file = {};
			if (!fsOpenStreamFromPath(assetParams->mRDOutput, skeletonOutput, FM_READ_BINARY, NULL, &file))
			{
				LOGF(eERROR, "Couldn't load Skeleton for mesh '%s' in %s. Skipping asset and all it's animations", skeletonInputFile, skeletonOutput);
				success = false;
				continue; // TODO: We could add a parameter so that the user can select whether to stop processing more assets or not when encountering an error
			}
			ozz::io::IArchive archive(&file);
			archive >> skeleton;
			fsCloseStream(&file);
		}

		// Process animations
		for (uint32_t a = 0, animEnd = (uint32_t)arrlen(skeletonAndAnims->mAnimations); a < animEnd; ++a)
		{
			const SkeletonAndAnimations::AnimationFile* anim = &skeletonAndAnims->mAnimations[a];
			const char* animInputFile = (char*)anim->mInputAnim.data;
			const char* animOutputPath = (char*)anim->mOutputAnimPath.data;

			assetsChecked++;

			// Check if the animation is already up-to-date
			bool processAnimation = true;
			if (!assetParams->mSettings.force && !processSkeleton)
			{
				time_t lastModified = fsGetLastModifiedTime(assetParams->mRDInput, animInputFile);
				time_t lastProcessed = fsGetLastModifiedTime(assetParams->mRDOutput, animOutputPath);

				if (lastModified < lastProcessed && lastProcessed != ~0u && lastModified > assetParams->mSettings.minLastModifiedTime)
					processAnimation = false;
			}

			if (processAnimation)
			{
				LOGF(eINFO, "Processing animations for mesh '%s': %s -> %s", skeletonInputFile, animInputFile, animOutputPath);

				// Process the animation
				if (CreateRuntimeAnimations(
					assetParams->mRDInput, animInputFile,
					assetParams->mRDOutput, animOutputPath,
					&skeleton, skeletonName, 
					&pProcessAnimationsParams->mAnimationSettings, &assetParams->mSettings))
				{
					LOGF(eERROR, "Failed to process animation for mesh '%s': %s -> %s", skeletonInputFile, animInputFile, animOutputPath);
					success = false;
					continue;
				}

				++assetsProcessed;
			}
		}

		skeleton.Deallocate();
	}

	LOGF(LogLevel::eINFO, "ProcessAnimations: checked %d assets, regenerated %d assets.", assetsChecked, assetsProcessed);
	EndAssetPipelineSection("ProcessAnimations");

	return !success;
}

static bool SaveSVT(ResourceDirectory resourceDir, const char* fileName, FileStream* pSrc, SVT_HEADER* pHeader)
{
	FileStream fh = {};

	if (!fsOpenStreamFromPath(resourceDir, fileName, FM_WRITE_BINARY, NULL, &fh))
		return true;

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

	return false;
}

bool ProcessVirtualTextures(AssetPipelineParams* assetParams)
{
	// Get all image files
	char ** ddsFiles = NULL;
    int ddsFileCount = 0;
	
	if (assetParams->mPathMode == PROCESS_MODE_DIRECTORY)
	{
		fsGetFilesWithExtension(assetParams->mRDInput, "", "dds", &ddsFiles, &ddsFileCount);
	}
	else
	{
		ddsFiles = (char**)tf_malloc(sizeof(char**));
		ddsFiles[0] = (char*)assetParams->mInFilePath;
		ddsFileCount = 1;
	}

	for (int i = 0; i < ddsFileCount; ++i)
	{
		bstring outputFile = bdynfromcstr(ddsFiles[i]);

		if (blength(&outputFile) > 0)
		{
			TextureDesc textureDesc = {};
			FileStream ddsFile = {};
			
			bool success = false;
			if (!fsOpenStreamFromPath(assetParams->mRDInput, ddsFiles[i], FM_READ_BINARY, nullptr, &ddsFile))
			{
				continue;
			}

			success = loadDDSTextureDesc(&ddsFile, &textureDesc);

			if (!success)
			{
				fsCloseStream(&ddsFile);
				LOGF(LogLevel::eERROR, "Failed to load image %s.", (char*)outputFile.data);
				continue;
			}

			bcatcstr(&outputFile, ".svt");

			SVT_HEADER header = {};
			header.mComponentCount = 4;
			header.mHeight = textureDesc.mHeight;
			header.mMipLevels = textureDesc.mMipLevels;
			header.mPageSize = 128;
			header.mWidth = textureDesc.mWidth;

			success = SaveSVT(assetParams->mRDOutput, (char*)outputFile.data, &ddsFile, &header);

			fsCloseStream(&ddsFile);

			if (!success)
			{
				LOGF(LogLevel::eERROR, "Failed to save sparse virtual texture %s.", (char*)outputFile.data);
				continue;
			}
		}
	}

    if (ddsFiles)
	{
		tf_free(ddsFiles);
	}
	
	bool error = false;
	return error;
}

bool ProcessTFX(AssetPipelineParams* assetParams, ProcessTressFXParams* tfxParams)
{
	cgltf_result result = cgltf_result_success;
	
	// Get all tfx files
	char ** tfxFiles = NULL;
    int tfxFileCount = 0;
	if (assetParams->mPathMode == PROCESS_MODE_DIRECTORY)
	{
		fsGetFilesWithExtension(assetParams->mRDInput, "", "tfx", &tfxFiles, &tfxFileCount);
	}
	else
	{
		tfxFiles = (char**)tf_malloc(sizeof(char**));
		tfxFiles[0] = (char*)assetParams->mInFilePath;
		tfxFileCount = 1;
	}

#define RETURN_IF_TFX_ERROR(expression) if (!(expression)) { LOGF(eERROR, "Failed to load tfx"); return false; }

	for (int i = 0; i < tfxFileCount; ++i)
	{
		const char* input = tfxFiles[i];
		char outputTemp[FS_MAX_PATH] = {};
		fsGetPathFileName(input, outputTemp);
		char output[FS_MAX_PATH] = {};
		fsAppendPathExtension(outputTemp, "gltf", output);

		char binFilePath[FS_MAX_PATH] = {};
		fsAppendPathExtension(outputTemp, "bin", binFilePath);

		FileStream tfxFile = {};
		fsOpenStreamFromPath(assetParams->mRDInput, input, FM_READ_BINARY, NULL, &tfxFile);
		AMD::TressFXAsset tressFXAsset = {};
		RETURN_IF_TFX_ERROR(tressFXAsset.LoadHairData(&tfxFile))
			fsCloseStream(&tfxFile);

		if (tfxParams->mFollowHairCount)
		{
			RETURN_IF_TFX_ERROR(tressFXAsset.GenerateFollowHairs(tfxParams->mFollowHairCount, tfxParams->mTipSeperationFactor, tfxParams->mMaxRadiusAroundGuideHair))
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
		fsOpenStreamFromPath(assetParams->mRDOutput, binFilePath, FM_WRITE_BINARY, NULL, &binFile);
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
		result = cgltf_write(assetParams->mRDOutput, output, &data);
	}

    if (tfxFiles)
	{
		tf_free(tfxFiles);
	}
	
	return result != cgltf_result_success;
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

static bool isTrsDecomposable(const Matrix4& matrix) {
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

bool CreateRuntimeSkeleton(
	ResourceDirectory resourceDirInput, const char* skeletonInputfile,
	ResourceDirectory resourceDirOutput, const char* skeletonOutputFile,
	ozz::animation::Skeleton* pOutSkeleton,
	const char* skeletonAsset,
	ProcessAssetsSettings* settings)
{
	cgltf_data* data = NULL;
	void* srcFileData = NULL;
	cgltf_result result = cgltf_parse_and_load(resourceDirInput, skeletonInputfile, &data, &srcFileData);
	if (cgltf_result_success != result)
	{
		LOGF(LogLevel::eERROR, "gltf parsing failed: %s", skeletonInputfile);
		return true;
	}

	if (data->skins_count == 0)
	{
		LOGF(LogLevel::eERROR, "Rigged mesh %s has no bones. Skeleton can not be created.", skeletonInputfile);
		return true;
	}

	// Gather node info
	// Used to mark nodes that should be included in the skeleton
	SkeletonNodeInfo* nodeData = nullptr;
	size_t startIndex = data->nodes_count - 1;
	const SkeletonNodeInfo startNode{ bdynfromcstr(data->nodes[startIndex].name), &data->nodes[startIndex] };
	arrpush(nodeData, startNode);

	const uint32_t queueSize = (uint32_t)(data->nodes_count + 1);
	int32_t* nodeQueue = nullptr;    // Simple queue because tinystl doesn't have one
	arrsetlen(nodeQueue, queueSize);
	for (uint32_t i = 0; i < queueSize; ++i)
		nodeQueue[i] = -1;
	nodeQueue[0] = 0;
	uint32_t nodeQueueStart = 0;
	uint32_t nodeQueueEnd = 1;
	while (nodeQueue[nodeQueueStart] != -1)
	{
		// Pop
		int32_t     nodeIndex = nodeQueue[nodeQueueStart];
		cgltf_node *node = nodeData[nodeIndex].pNode;
		nodeQueue[nodeQueueStart] = -1;

		for (uint i = 0; i < node->children_count; ++i)
		{
			SkeletonNodeInfo childNode = {};
			childNode.mName = bdynfromcstr(node->children[i]->name);
			childNode.pParentNodeIndex = nodeIndex;
			const int32_t childNodeIndex = (int32_t)arrlen(nodeData);
			childNode.pNode = node->children[i];
			arrpush(nodeData, childNode);

			arrpush(nodeData[nodeIndex].mChildNodeIndices, childNodeIndex);

			const uint32_t queueIdx = nodeQueueEnd;
			ASSERT(queueIdx != nodeQueueStart && "Seems like we didn't allocate enough space for the queue");
			if (queueIdx == nodeQueueStart)
			{
				LOGF(LogLevel::eERROR, "Too many nodes in scene. Skeleton can not be created.", skeletonInputfile);
				return true;
			}

			nodeQueueEnd = (nodeQueueEnd + 1) % queueSize;

			// Push
			nodeQueue[queueIdx] = childNodeIndex;
		}

		nodeQueueStart++;
		ASSERT(nodeQueueStart < (uint32_t)arrlen(nodeQueue) && "Seems like we didn't allocate enough space for the queue");
	}

	arrfree(nodeQueue);
	nodeQueue = nullptr;

	// Mark all nodes that are required to be in the skeleton
	for (cgltf_size i = 0; i < data->skins_count; ++i)
	{
		cgltf_skin* skin = &data->skins[i];
		for (cgltf_size j = 0; j < skin->joints_count; ++j)
		{
			cgltf_node* bone = skin->joints[j];
			for (cgltf_size k = 0; k < (cgltf_size)arrlen(nodeData); ++k)
			{
				if (strcmp((const char*)nodeData[k].mName.data, bone->name) == 0)
				{
					int32_t nodeIndex = (int32_t)k;
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
	ozz::animation::offline::RawSkeleton rawSkeleton = {};

	SkeletonBoneInfo* boneData = nullptr;
	const SkeletonBoneInfo firstBone{ 0, -1, NULL };
	arrpush(boneData, firstBone);

	while (arrlen(boneData) != 0)
	{
		SkeletonBoneInfo boneInfo = arrlast(boneData);
		arrpop(boneData);
		SkeletonNodeInfo* nodeInfo = &nodeData[boneInfo.mNodeIndex];
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

		bassign(&joint.name, &nodeInfo->mName);

		// Add node to raw skeleton
		ozz::animation::offline::RawSkeleton::Joint* newParentJoint = NULL;
		if (boneInfo.pParentJoint == NULL)
		{
			arrpush(rawSkeleton.roots, joint);
			newParentJoint = &arrlast(rawSkeleton.roots);
		}
		else
		{
			arrpush(boneInfo.pParentJoint->children, joint);
			newParentJoint = &arrlast(boneInfo.pParentJoint->children);
		}

		// Count the child nodes that are required to be in the skeleton
		int requiredChildCount = 0;
		for (uint i = 0; i < (uint)arrlen(nodeInfo->mChildNodeIndices); ++i)
		{
			SkeletonNodeInfo* childNodeInfo = &nodeData[nodeInfo->mChildNodeIndices[i]];
			if (childNodeInfo->mUsedInSkeleton)
				++requiredChildCount;
		}

		// Add child nodes to the list of nodes to process
		arrsetcap(newParentJoint->children, requiredChildCount);    // Reserve to make sure memory isn't reallocated later.
		for (uint i = 0; i < (uint)arrlen(nodeInfo->mChildNodeIndices); ++i)
		{
			SkeletonNodeInfo* childNodeInfo = &nodeData[nodeInfo->mChildNodeIndices[i]];
			if (childNodeInfo->mUsedInSkeleton)
			{
				boneInfo.mNodeIndex = nodeInfo->mChildNodeIndices[i];
				boneInfo.mParentNodeIndex = boneInfo.mNodeIndex;
				boneInfo.pParentJoint = newParentJoint;
				arrpush(boneData, boneInfo);
			}
		}
	}

	for (uint32_t i = 0, end = (uint32_t)arrlen(nodeData); i < end; ++i)
	{
		bdestroy(&nodeData[i].mName);
		arrfree(nodeData[i].mChildNodeIndices);
	}

	arrfree(boneData);
	boneData = nullptr;
	arrfree(nodeData);
	nodeData = nullptr;

	// Validate raw skeleton
	if (!rawSkeleton.Validate())
	{
		LOGF(LogLevel::eERROR, "Skeleton created for %s is invalid. Skeleton can not be created.", skeletonInputfile);
		return true;
	}

	// Build runtime skeleton from raw skeleton
	if (!ozz::animation::offline::SkeletonBuilder::Build(rawSkeleton, pOutSkeleton))
	{
		LOGF(LogLevel::eERROR, "Failed to build Skeleton: %s", skeletonInputfile);
		return true;
	}

	// Write skeleton to disk
	FileStream file = {};
	if (!fsOpenStreamFromPath(resourceDirOutput, skeletonOutputFile, FM_WRITE_BINARY, NULL, &file))
		return true;

	ozz::io::OArchive archive(&file);
	archive << *pOutSkeleton;
	fsCloseStream(&file);

	// Generate joint remaps in the gltf
	{
		char* buffer = nullptr;
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
					uint32_t jointIndex = FindJoint(pOutSkeleton, jointNode->name);
					if (j == 0)
						offset += sprintf(jointRemaps + offset, "%u", jointIndex);
					else
						offset += sprintf(jointRemaps + offset, ", %u", jointIndex);
				}

				offset += sprintf(jointRemaps + offset, " ]");
				skin->extras.start_offset = size;
				size += (uint32_t)strlen(jointRemaps) + 1;
				skin->extras.end_offset = size - 1;

				const size_t iterCount = strlen(jointRemaps) + 1;
				for (uint32_t i = 0; i < iterCount; ++i)
					arrpush(buffer, jointRemaps[i]);
			}
		}

		for (uint32_t i = 0; i < data->buffers_count; ++i)
		{
			if (strncmp(data->buffers[i].uri, "://", 3) == 0)
			{
				char parentPath[FS_MAX_PATH] = {};
				fsGetParentPath(skeletonInputfile, parentPath);
				char bufferOut[FS_MAX_PATH] = {};
				fsAppendPathComponent(parentPath, data->buffers[i].uri, bufferOut);

				LOGF(eINFO, "Writing binary gltf data: %s -> %s", skeletonInputfile, bufferOut);

				FileStream fs = {};
				fsOpenStreamFromPath(resourceDirOutput, bufferOut, FM_WRITE_BINARY, NULL, &fs);
				fsWriteToStream(&fs, data->buffers[i].data, data->buffers[i].size);
				fsCloseStream(&fs);
			}
		}

		LOGF(eINFO, "Writing text gltf data: %s -> %s", skeletonInputfile, skeletonInputfile);

		data->file_data = buffer;
		if (cgltf_result_success != cgltf_write(resourceDirOutput, skeletonInputfile, data))
		{
			LOGF(eERROR, "Couldn't write the gltf file with the remapped joint data for it's skeleton.");
			arrfree(buffer);
			return true;
		}

		arrfree(buffer);
	}

	data->file_data = srcFileData;
	cgltf_free(data);

	const bool error = false;
	return error;
}

static bool CreateRuntimeAnimation(RuntimeAnimationSettings* animationSettings, cgltf_animation* animationData, ResourceDirectory resourceDirOutput, const char* animOutFile, ozz::animation::Skeleton* skeleton, const char* skeletonName, const char* animationSourceFile)
{
	ASSERT(animationData && skeleton);

	// Create raw animation
	ozz::animation::offline::RawAnimation rawAnimation = {};
	rawAnimation.name = bdynfromcstr(animationData->name);
	rawAnimation.duration = 0.f;

	arrsetlen(rawAnimation.tracks, skeleton->num_joints());
	memset(rawAnimation.tracks, 0, sizeof(*rawAnimation.tracks) * skeleton->num_joints());

	for (uint i = 0; i < (uint)skeleton->num_joints(); ++i)
	{
		const char* jointName = skeleton->joint_names()[i];

		ozz::animation::offline::RawAnimation::JointTrack* track = &rawAnimation.tracks[i];

		for (cgltf_size channelIndex = 0; channelIndex < animationData->channels_count; channelIndex += 1)
		{
			cgltf_animation_channel* channel = &animationData->channels[channelIndex];

			if (strcmp(channel->target_node->name, jointName) != 0) continue;

			if (channel->target_path == cgltf_animation_path_type_translation)
			{
				arrsetlen(track->translations, channel->sampler->output->count);
				memset((char*)track->translations, 0, sizeof(*track->translations) * channel->sampler->output->count);
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
				arrsetlen(track->rotations, channel->sampler->output->count);
				memset((char*)track->rotations, 0, sizeof(*track->rotations) * channel->sampler->output->count);
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
				arrsetlen(track->scales, channel->sampler->output->count);
				memset((char*)track->scales, 0, sizeof(*track->scales) * channel->sampler->output->count);
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

		if (arrlen(track->translations) != 0)
			rawAnimation.duration = max(rawAnimation.duration, arrlast(track->translations).time);

		if (arrlen(track->rotations) != 0)
			rawAnimation.duration = max(rawAnimation.duration, arrlast(track->rotations).time);

		if (arrlen(track->scales) != 0)
			rawAnimation.duration = max(rawAnimation.duration, arrlast(track->scales).time);
	}

	// Validate raw animation
	if (!rawAnimation.Validate())
	{
		LOGF(LogLevel::eERROR, "Animation %s:%s created for %s is invalid. Animation can not be created.", animationSourceFile, animationData->name, skeletonName);
		return true;
	}

	ozz::animation::offline::RawAnimation* rawAnimToBuild = &rawAnimation;

	ozz::animation::offline::RawAnimation optimizedRawAnimation = {};
	if (animationSettings->mOptimizeTracks)
	{
		LOGF(eINFO, "Optimizing tracks...");
		ozz::animation::offline::AnimationOptimizer optimizer = {};
		optimizer.setting.tolerance = animationSettings->mOptimizationTolerance;
		optimizer.setting.distance = animationSettings->mOptimizationDistance;
		if (!optimizer(rawAnimation, *skeleton, &optimizedRawAnimation))
		{
			LOGF(LogLevel::eERROR, "Animation %s:%s could not be optimized for skeleton %s.", animationSourceFile, animationData->name, skeletonName);
			return true;
		}

		rawAnimToBuild = &optimizedRawAnimation;
	}

	// Build runtime animation from raw animation
	ozz::animation::Animation animation = {};
	if (!ozz::animation::offline::AnimationBuilder::Build(*rawAnimToBuild, &animation))
	{
		LOGF(LogLevel::eERROR, "Animation %s:%s can not be created for %s.", animationSourceFile, animationData->name, skeletonName);
		return true;
	}

	// Write animation to disk
	FileStream file = {};
	if (!fsOpenStreamFromPath(resourceDirOutput, animOutFile, FM_WRITE_BINARY, NULL, &file))
	{
		LOGF(LogLevel::eERROR, "Animation %s:%s can not be saved in %s/%s", animationSourceFile, animationData->name, fsGetResourceDirectory(resourceDirOutput), animOutFile);
		animation.Deallocate();
		return true;
	}

	ozz::io::OArchive archive(&file);
	archive << animation;
	fsCloseStream(&file);
	//Deallocate animation
	animation.Deallocate();

	return false;
}

bool CreateRuntimeAnimations(
	ResourceDirectory resourceDirInput, const char* animationInputFile, 
	ResourceDirectory resourceDirOutput, const char* animationOutputPath, 
	ozz::animation::Skeleton* skeleton, 
	const char* skeletonName, 
	RuntimeAnimationSettings* animationSettings,
	ProcessAssetsSettings* settings)
{
	// Import the glTF with the animation
	cgltf_data* data = NULL;
	void* srcFileData = NULL;
	cgltf_result result = cgltf_parse_and_load(resourceDirInput, animationInputFile, &data, &srcFileData);
	if (result != cgltf_result_success)
	{
		return true;
	}

	// Check if the asset contains any animations
	if (data->animations_count == 0)
	{
		LOGF(LogLevel::eWARNING, "Animation asset %s of skeleton %s contains no animations.", animationInputFile, skeletonName);

		tf_free(srcFileData);
		cgltf_free(data);
		return true;
	}

	LOGF(eINFO, "Processing %d animations from file '%s'", (uint32_t)data->animations_count, animationInputFile);

	bool error = false;
	for (cgltf_size animationIndex = 0; animationIndex < data->animations_count; animationIndex += 1)
	{
		cgltf_animation* animationData = &data->animations[animationIndex];

		LOGF(eINFO, "Processing animation %d: %s", (uint32_t)animationIndex, animationData->name);

		char buffer[FS_MAX_PATH] = {};
		if (data->animations_count == 1)
		{
			// If the file only contains one animation we use the name of the input file as the output file
			char inAnimFilename[FS_MAX_PATH / 2] = {};
			fsGetPathFileName(animationInputFile, inAnimFilename);
			snprintf(buffer, sizeof(buffer), "%s/%s.ozz", animationOutputPath, inAnimFilename);
		}
		else
		{
			// When the input file contains more than one animation we use the name of the animation so that we don't have collisions
			snprintf(buffer, sizeof(buffer), "%s/%s.ozz", animationOutputPath, animationData->name);
		}

		const char* animOutFilePath = buffer;
		if (CreateRuntimeAnimation(animationSettings, animationData, resourceDirOutput, animOutFilePath, skeleton, skeletonName, animationInputFile))
			error = true;
	}

	tf_free(srcFileData);
	cgltf_free(data);

	return error;
}

static inline constexpr ShaderSemantic util_cgltf_attrib_type_to_semantic(cgltf_attribute_type type, uint32_t index)
{
	switch (type)
	{
		case cgltf_attribute_type_position: return SEMANTIC_POSITION;
		case cgltf_attribute_type_normal: return SEMANTIC_NORMAL;
		case cgltf_attribute_type_tangent: return SEMANTIC_TANGENT;
		case cgltf_attribute_type_color: return SEMANTIC_COLOR;
		case cgltf_attribute_type_joints: return SEMANTIC_JOINTS;
		case cgltf_attribute_type_weights: return SEMANTIC_WEIGHTS;
		case cgltf_attribute_type_texcoord: return (ShaderSemantic)(SEMANTIC_TEXCOORD0 + index);
		default: return SEMANTIC_TEXCOORD0;
	}
}

static uint32_t gltfAttributeStride(const cgltf_attribute* const* attributes, ShaderSemantic semantic)
{
	const cgltf_size stride = attributes[semantic] ? attributes[semantic]->data->stride : 0;
	ASSERT(stride < UINT32_MAX);
	if (stride > UINT32_MAX)
	{
		LOGF(eERROR, "ShaderSemantic stride of this gltf_attribute is too big to store in uint32, value will be truncated");
		return UINT32_MAX;
	}
	return (uint32_t)stride;
}

bool ProcessGLTF(AssetPipelineParams* assetParams)
{
	bool error = false;
	
	// Get all gltf files
	char ** gltfFiles = NULL;
	int gltfFileCount = 0;
	
	if (assetParams->mPathMode == PROCESS_MODE_DIRECTORY)
	{
		fsGetFilesWithExtension(assetParams->mRDInput, "", "gltf", &gltfFiles, &gltfFileCount);
	}
	else
	{
		gltfFiles = (char**)tf_malloc(sizeof(char**));
		gltfFiles[0] = (char*)assetParams->mInFilePath;
		gltfFileCount = 1;
	}
	
	for (int i = 0; i < gltfFileCount; ++i)
	{
		const char* fileName = gltfFiles[i];
		
		char newFileName[FS_MAX_PATH] = { 0 };
		
		fsReplacePathExtension(fileName, "bin", newFileName);
		
		if (!assetParams->mSettings.force && fsFileExist(assetParams->mRDOutput, newFileName))
		{
			continue;
		}
		
		LOGF(eINFO, "Converting %s to TF custom binary file", fileName);

		FileStream file = {};
		if (!fsOpenStreamFromPath(assetParams->mRDInput, fileName, FM_READ_BINARY, nullptr, &file))
		{
			LOGF(eERROR, "Failed to open gltf file %s", fileName);
			error = true;
			continue;
		}

		ssize_t fileSize = fsGetStreamFileSize(&file);
		void* fileData = tf_malloc(fileSize);

		fsReadFromStream(&file, fileData, fileSize);

		cgltf_options options = {};
		cgltf_data* data = NULL;
		options.memory_alloc = [](void* user, cgltf_size size) { return tf_malloc(size); };
		options.memory_free = [](void* user, void* ptr) { tf_free(ptr); };
		cgltf_result result = cgltf_parse(&options, fileData, fileSize, &data);
		fsCloseStream(&file);

		if (cgltf_result_success != result)
		{
			LOGF(eERROR, "Failed to parse gltf file %s with error %u", fileName, (uint32_t)result);
			tf_free(fileData);
			error = true;
			continue;
		}

	#if defined(FORGE_DEBUG)
		result = cgltf_validate(data);
		if (cgltf_result_success != result)
		{
			LOGF(eWARNING, "GLTF validation finished with error %u for file %s", (uint32_t)result, fileName);
		}
	#endif

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
				fsGetParentPath(fileName, parent);
				char path[FS_MAX_PATH] = { 0 };
				fsAppendPathComponent(parent, uri, path);
				FileStream fs = {};
				if (fsOpenStreamFromPath(assetParams->mRDInput, path, FM_READ_BINARY, nullptr, &fs))
				{
					ASSERT(fsGetStreamFileSize(&fs) >= (ssize_t)data->buffers[i].size);
					data->buffers[i].data = tf_malloc(data->buffers[i].size);
					fsReadFromStream(&fs, data->buffers[i].data, data->buffers[i].size);
				}
				fsCloseStream(&fs);
			}
		}

		result = cgltf_load_buffers(&options, data, fileName);
		if (cgltf_result_success != result)
		{
			LOGF(eERROR, "Failed to load buffers from gltf file %s with error %u", fileName, (uint32_t)result);
			tf_free(fileData);
			continue;
		}

		cgltf_attribute* vertexAttribs[MAX_VERTEX_ATTRIBS] = {};

		uint32_t indexCount = 0;
		uint32_t vertexCount = 0;
		uint32_t drawCount = 0;
		uint32_t jointCount = 0;

		// Find number of traditional draw calls required to draw this piece of geometry
		// Find total index count, total vertex count
		for (uint32_t i = 0; i < data->meshes_count; ++i)
		{
			for (uint32_t p = 0; p < data->meshes[i].primitives_count; ++p)
			{
				const cgltf_primitive* prim = &data->meshes[i].primitives[p];
				indexCount += (uint32_t)(prim->indices->count);
				vertexCount += (uint32_t)(prim->attributes[0].data->count);
				++drawCount;

				for (uint32_t j = 0; j < prim->attributes_count; ++j)
				{
					if(prim->attributes[j].data->count != prim->attributes[0].data->count)
					{
						LOGF(eERROR, "Mismatched vertex attribute count for %s, attribute index %d", fileName, j);
						ASSERT(false);
					}
					const uint32_t semanticIdx = (uint32_t)util_cgltf_attrib_type_to_semantic(prim->attributes[j].type, prim->attributes[j].index);
					ASSERT(semanticIdx < MAX_VERTEX_ATTRIBS);
					vertexAttribs[semanticIdx] = &prim->attributes[j];
				}
			}
		}
		for (uint32_t i = 0; i < data->skins_count; ++i)
			jointCount += (uint32_t)data->skins[i].joints_count;

		// Determine index stride
		// This depends on vertex count rather than the stride specified in gltf
		// since gltf assumes we have index buffer per primitive which is non optimal
		const uint32_t indexStride = vertexCount > UINT16_MAX ? sizeof(uint32_t) : sizeof(uint16_t);

		uint32_t totalSize = 0;
		totalSize += round_up(sizeof(Geometry), 16);
		totalSize += round_up(drawCount * sizeof(IndirectDrawIndexArguments), 16);
		totalSize += round_up(jointCount * sizeof(mat4), 16);
		totalSize += round_up(jointCount * sizeof(uint32_t), 16);


		Geometry* geom = (Geometry*)tf_calloc(1, totalSize);
		ASSERT(geom);

		geom->pDrawArgs = (IndirectDrawIndexArguments*)(geom + 1);    //-V1027
		geom->pInverseBindPoses = (mat4*)((uint8_t*)geom->pDrawArgs + round_up(drawCount * sizeof(*geom->pDrawArgs), 16));
		geom->pJointRemaps = (uint32_t*)((uint8_t*)geom->pInverseBindPoses + round_up(jointCount * sizeof(*geom->pInverseBindPoses), 16));

		uint32_t shadowSize = 0;
		
		shadowSize += sizeof(Geometry::ShadowData);
		shadowSize += gltfAttributeStride(vertexAttribs, SEMANTIC_POSITION) * vertexCount;
		shadowSize += gltfAttributeStride(vertexAttribs, SEMANTIC_NORMAL) * vertexCount;
		shadowSize += gltfAttributeStride(vertexAttribs, SEMANTIC_TEXCOORD0) * vertexCount;
		shadowSize += gltfAttributeStride(vertexAttribs, SEMANTIC_JOINTS) * vertexCount;
		shadowSize += gltfAttributeStride(vertexAttribs, SEMANTIC_WEIGHTS) * vertexCount;
		shadowSize += indexCount * indexStride;

		geom->pShadow = (Geometry::ShadowData*)tf_calloc(1, shadowSize);
		geom->pShadow->pIndices = geom->pShadow + 1;
		geom->pShadow->pAttributes[SEMANTIC_POSITION] = (uint8_t*)geom->pShadow->pIndices + (indexCount * indexStride);
		geom->pShadow->pAttributes[SEMANTIC_NORMAL] = (uint8_t*)geom->pShadow->pAttributes[SEMANTIC_POSITION] + gltfAttributeStride(vertexAttribs, SEMANTIC_POSITION) * vertexCount;
		geom->pShadow->pAttributes[SEMANTIC_TEXCOORD0] = (uint8_t*)geom->pShadow->pAttributes[SEMANTIC_NORMAL] + gltfAttributeStride(vertexAttribs, SEMANTIC_NORMAL) * vertexCount;
		geom->pShadow->pAttributes[SEMANTIC_JOINTS] = (uint8_t*)geom->pShadow->pAttributes[SEMANTIC_TEXCOORD0] + gltfAttributeStride(vertexAttribs, SEMANTIC_TEXCOORD0) * vertexCount;
		geom->pShadow->pAttributes[SEMANTIC_WEIGHTS] = (uint8_t*)geom->pShadow->pAttributes[SEMANTIC_JOINTS] + gltfAttributeStride(vertexAttribs, SEMANTIC_JOINTS) * vertexCount;
		ASSERT(((const char*)geom->pShadow) + shadowSize == ((char*)geom->pShadow->pAttributes[SEMANTIC_WEIGHTS] + gltfAttributeStride(vertexAttribs, SEMANTIC_WEIGHTS) * vertexCount));
		// #TODO: Add more if needed
		

		geom->mDrawArgCount = drawCount;
		geom->mIndexCount = indexCount;
		geom->mVertexCount = vertexCount;
		geom->mIndexType = (sizeof(uint16_t) == indexStride) ? INDEX_TYPE_UINT16 : INDEX_TYPE_UINT32;
		geom->mJointCount = jointCount;

		COMPILE_ASSERT(TF_ARRAY_COUNT(vertexAttribs) <= TF_ARRAY_COUNT(geom->mVertexStrides));
		for (uint32_t i = 0; i < TF_ARRAY_COUNT(vertexAttribs); ++i)
		{
			geom->mVertexStrides[i] = gltfAttributeStride(vertexAttribs, (ShaderSemantic)i);
		}
		
		indexCount = 0;
		vertexCount = 0;
		drawCount = 0;

		// Load the remap joint indices generated in the offline process
		uint32_t remapCount = 0;
		for (uint32_t i = 0; i < data->skins_count; ++i)
		{
			const cgltf_skin* skin = &data->skins[i];
			uint32_t          extrasSize = (uint32_t)(skin->extras.end_offset - skin->extras.start_offset);
			if (extrasSize)
			{
				const char* jointRemaps = (const char*)data->json + skin->extras.start_offset;
				jsmn_parser parser = {};
				jsmntok_t*  tokens = (jsmntok_t*)tf_malloc((skin->joints_count + 1) * sizeof(jsmntok_t));
				jsmn_parse(&parser, (const char*)jointRemaps, extrasSize, tokens, skin->joints_count + 1);
				ASSERT(tokens[0].size == (int)skin->joints_count + 1);
				cgltf_accessor_unpack_floats(
					skin->inverse_bind_matrices, (cgltf_float*)geom->pInverseBindPoses,
					skin->joints_count * sizeof(float[16]) / sizeof(float));
				for (uint32_t r = 0; r < skin->joints_count; ++r)
					geom->pJointRemaps[remapCount + r] = atoi(jointRemaps + tokens[1 + r].start);
				tf_free(tokens);
			}

			remapCount += (uint32_t)skin->joints_count;
		}

		// Load the tressfx specific data generated in the offline process
		if (stricmp(data->asset.generator, "tressfx") == 0)
		{
			// { "mVertexCountPerStrand" : "16", "mGuideCountPerStrand" : "3456" }
			uint32_t    extrasSize = (uint32_t)(data->asset.extras.end_offset - data->asset.extras.start_offset);
			const char* json = data->json + data->asset.extras.start_offset;
			jsmn_parser parser = {};
			jsmntok_t   tokens[5] = {};
			jsmn_parse(&parser, (const char*)json, extrasSize, tokens, 5);
			geom->mHair.mVertexCountPerStrand = atoi(json + tokens[2].start);
			geom->mHair.mGuideCountPerStrand = atoi(json + tokens[4].start);
		}

		for (uint32_t i = 0; i < data->meshes_count; ++i)
		{
			for (uint32_t p = 0; p < data->meshes[i].primitives_count; ++p)
			{
				const cgltf_primitive* prim = &data->meshes[i].primitives[p];
				/************************************************************************/
				// Fill index buffer for this primitive
				/************************************************************************/
				if (sizeof(uint16_t) == indexStride)
				{
					uint16_t* dst = (uint16_t*)geom->pShadow->pIndices;
					for (uint32_t idx = 0; idx < prim->indices->count; ++idx)
						dst[indexCount + idx] = vertexCount + (uint16_t)cgltf_accessor_read_index(prim->indices, idx);
				}
				else
				{
					uint32_t* dst = (uint32_t*)geom->pShadow->pIndices;
					for (uint32_t idx = 0; idx < prim->indices->count; ++idx)
						dst[indexCount + idx] = vertexCount + (uint32_t)cgltf_accessor_read_index(prim->indices, idx);
				}

				for (uint32_t a = 0; a < prim->attributes_count; ++a)
				{
					cgltf_attribute* attr = &prim->attributes[a];
					if (cgltf_attribute_type_position == attr->type)
					{
						const uint8_t* src =
							(uint8_t*)attr->data->buffer_view->buffer->data + attr->data->offset + attr->data->buffer_view->offset;
						uint8_t* dst = (uint8_t*)geom->pShadow->pAttributes[SEMANTIC_POSITION] + vertexCount * attr->data->stride;
						memcpy(dst, src, attr->data->count * attr->data->stride);
					}
					else if (cgltf_attribute_type_normal == attr->type)
					{
						const uint8_t* src =
							(uint8_t*)attr->data->buffer_view->buffer->data + attr->data->offset + attr->data->buffer_view->offset;
						uint8_t* dst = (uint8_t*)geom->pShadow->pAttributes[SEMANTIC_NORMAL] + vertexCount * attr->data->stride;
						memcpy(dst, src, attr->data->count * attr->data->stride);
					}
					else if (cgltf_attribute_type_texcoord == attr->type)
					{
						const uint8_t* src =
							(uint8_t*)attr->data->buffer_view->buffer->data + attr->data->offset + attr->data->buffer_view->offset;
						uint8_t* dst = (uint8_t*)geom->pShadow->pAttributes[SEMANTIC_TEXCOORD0] + vertexCount * attr->data->stride;
						memcpy(dst, src, attr->data->count * attr->data->stride);
					}
					else if (cgltf_attribute_type_joints == attr->type)
					{
						const uint8_t* src =
							(uint8_t*)attr->data->buffer_view->buffer->data + attr->data->offset + attr->data->buffer_view->offset;
						uint8_t* dst = (uint8_t*)geom->pShadow->pAttributes[SEMANTIC_JOINTS] + vertexCount * attr->data->stride;
						memcpy(dst, src, attr->data->count * attr->data->stride);
					}
					else if (cgltf_attribute_type_weights == attr->type)
					{
						const uint8_t* src =
							(uint8_t*)attr->data->buffer_view->buffer->data + attr->data->offset + attr->data->buffer_view->offset;
						uint8_t* dst = (uint8_t*)geom->pShadow->pAttributes[SEMANTIC_WEIGHTS] + vertexCount * attr->data->stride;
						memcpy(dst, src, attr->data->count * attr->data->stride);
					}
				}

				/************************************************************************/
				// Fill draw arguments for this primitive
				/************************************************************************/
				geom->pDrawArgs[drawCount].mIndexCount = (uint32_t)prim->indices->count;
				geom->pDrawArgs[drawCount].mInstanceCount = 1;
				geom->pDrawArgs[drawCount].mStartIndex = indexCount;
				geom->pDrawArgs[drawCount].mStartInstance = 0;
				// Since we already offset indices when creating the index buffer, vertex offset will be zero
				// With this approach, we can draw everything in one draw call or use the traditional draw per subset without the
				// need for changing shader code
				geom->pDrawArgs[drawCount].mVertexOffset = 0;

				indexCount += (uint32_t)(prim->indices->count);
				vertexCount += (uint32_t)(prim->attributes[0].data->count);
				++drawCount;
			}
		}
		
		FileStream fStream = {};
		if (!fsOpenStreamFromPath(assetParams->mRDOutput, newFileName, FM_WRITE_BINARY_ALLOW_READ, nullptr, &fStream))
		{
			LOGF(eERROR, "Couldn't open file '%s' for write.", newFileName);
			error = true;
		}
		else
		{
			fsWriteToStream(&fStream, GEOMETRY_FILE_MAGIC_STR, sizeof(GEOMETRY_FILE_MAGIC_STR));

			fsWriteToStream(&fStream, &totalSize, sizeof(uint32_t));

			fsWriteToStream(&fStream, geom, totalSize);

			fsWriteToStream(&fStream, &shadowSize, sizeof(uint32_t));

			fsWriteToStream(&fStream, geom->pShadow, shadowSize);

			if (!fsCloseStream(&fStream))
			{
				LOGF(eERROR, "Failed to close write stream for file '%s'.", newFileName);
				error = true;
			}
		}

		tf_free(geom->pShadow);
		tf_free(geom);
		
		data->file_data = fileData;
		cgltf_free(data);
	}
		
	if (gltfFiles)
		tf_free(gltfFiles);
	
	return error;
}

static bool HandleAstcEncoderError(astcenc_error errCode)
{
	if (errCode != ASTCENC_SUCCESS)
	{
		LOGF(eERROR, "astc-encoder reported: %s", astcenc_get_error_string(errCode));
		return true;
	}
	return false;
}

struct CompressTextureParams
{
	astcenc_profile mProfile = ASTCENC_PRF_HDR;
	float mQuality = ASTCENC_PRE_FAST;
	uint32_t mFlags = 0; // See ASTCENC_ALL_FLAGS for information about available flags

	uint32_t mBlockX = 4;
	uint32_t mBlockY = 4;
	uint32_t mBlockZ = 1;
};

bool CompressSingleTexture(CompressTextureParams* params, astcenc_image* image, astc_compressed_image* outImage)
{
	ASSERT(image);
	ASSERT(outImage);
	ASSERT(params);

	astcenc_config astcConfig = {};
	astcenc_error astcErr = astcenc_config_init(params->mProfile, params->mBlockX, params->mBlockY, params->mBlockZ, params->mQuality, params->mFlags, &astcConfig);
	if (HandleAstcEncoderError(astcErr))
		return true;

	astcenc_context* astcContext;
	astcErr = astcenc_context_alloc(&astcConfig, 1, &astcContext);
	if (HandleAstcEncoderError(astcErr))
		return true;

	astc_compressed_image cimage = {};

	cimage.block_x = params->mBlockX;
	cimage.block_y = params->mBlockY;
	cimage.block_z = params->mBlockZ;
	cimage.dim_x = image->dim_x;
	cimage.dim_y = image->dim_y;
	cimage.dim_z = image->dim_z;

	const uint32_t xblocks = (image->dim_x + params->mBlockX - 1) / params->mBlockX;
	const uint32_t yblocks = (image->dim_y + params->mBlockX - 1) / params->mBlockX;
	const uint32_t zblocks = (image->dim_z + params->mBlockX - 1) / params->mBlockX;
	const uint32_t bytesPerBlock = 16;

	const uint32_t bufferSize = xblocks * yblocks * zblocks * bytesPerBlock;
	cimage.data = (uint8_t*)tf_malloc(bufferSize);
	cimage.data_len = bufferSize;

	const astcenc_swizzle astcDefaultSwizzle = { ASTCENC_SWZ_R, ASTCENC_SWZ_G, ASTCENC_SWZ_B, ASTCENC_SWZ_A };

	// TODO: Create some workers and reuse them for all texture compressions, otherwise it'll take too long
	astcErr = astcenc_compress_image(astcContext, image, &astcDefaultSwizzle, cimage.data, cimage.data_len, 0);

	if (HandleAstcEncoderError(astcErr))
	{
		free_cimage(cimage);
		astcenc_context_free(astcContext);
		return true;
	}

	// Reset so that we can compress the next texture
	astcErr = astcenc_compress_reset(astcContext);
	if (HandleAstcEncoderError(astcErr))
	{
		free_cimage(cimage);
		astcenc_context_free(astcContext);
		return true;
	}

	astcenc_context_free(astcContext);

	*outImage = cimage;
	return false;
}

bool ProcessTextures(AssetPipelineParams* assetParams, ProcessTexturesParams* texturesParams)
{
	bool error = false;

	// Get all image files
	char** imgFiles = NULL;
	int imgFileCount = 0;

	if (assetParams->mPathMode == PROCESS_MODE_DIRECTORY)
	{
		fsGetFilesWithExtension(assetParams->mRDInput, "", texturesParams->mInExt, &imgFiles, &imgFileCount);
	}
	else
	{
		imgFiles = (char**)tf_malloc(sizeof(char**));
		imgFiles[0] = (char*)assetParams->mInFilePath;
		imgFileCount = 1;
	}
	
	for (int i = 0; i < imgFileCount; ++i)
	{
		const char* fileName = imgFiles[i];
		
		if (!assetParams->mSettings.force && fsFileExist(assetParams->mRDOutput, fileName))
		{
			continue;
		}
		
		LOGF(eINFO, "Converting texture %s from .%s to .%s", fileName, texturesParams->mInExt, texturesParams->mOutExt);
		
		char inFilePath[FS_MAX_PATH];
		
		fsAppendPathComponent(fsGetResourceDirectory(assetParams->mRDInput), fileName, inFilePath);
		
		bool isHdr = false;
		unsigned int componentCount = 0;
		astcenc_image* image = load_ncimage(inFilePath, false, isHdr, componentCount);

		bool isRgb = false;
		astc_compressed_image cimage = {};
		bool loadedCompressedKtx = false;
		bool loadedCompressedImage = false;
		
		if (image)
		{
			CompressTextureParams compressionParams = {};
			if (CompressSingleTexture(&compressionParams, image, &cimage))
			{
				LOGF(eERROR, "Couldn't compress texture: %s", inFilePath);
				error = true;
			}
		}
		else
		{
			if (STRCMP(texturesParams->mInExt, "ktx"))
			{
				LOGF(eINFO, "Trying to load compressed ktx: %s");
				loadedCompressedKtx = !load_ktx_compressed_image(inFilePath, isRgb, cimage);
				
				if (!loadedCompressedKtx)
				{
					LOGF(eERROR, "Couldn't load ktx compressed image: %s", inFilePath);
					error = true;
				}
			}
			else
			{
				loadedCompressedImage = !load_cimage(inFilePath, cimage);
				
				if (!loadedCompressedImage)
				{
					LOGF(eERROR, "Couldn't load compressed image: %s", inFilePath);
					error = true;
				}
			}
			
			if (!loadedCompressedKtx && !loadedCompressedImage)
			{
				LOGF(eERROR, "Failed to load texture: %s", inFilePath);
				error = true;
				continue;
			}
		}
		
		char newFileName[FS_MAX_PATH] = { 0 };
		
		fsReplacePathExtension(fileName, texturesParams->mOutExt, newFileName);
		
		char outFilePath[FS_MAX_PATH];
		
		fsAppendPathComponent(fsGetResourceDirectory(assetParams->mRDOutput), newFileName, outFilePath);
		
		if (STRCMP(texturesParams->mOutExt, "ktx"))
		{
			// This function returns true on failure
			if (store_ktx_compressed_image(cimage, outFilePath, isRgb))
			{
				LOGF(eERROR, "Couldn't store ktx compressed image: %s", outFilePath);
				error = true;
			}
		}
		else if (STRCMP(texturesParams->mOutExt, "dds"))
		{
			// TODO: We should save cimage in compressed mode here, but astc-enconder cannot save dds textures compressed,
			//       store_cimage does nothing.

			// This function returns true on success
			if (!store_ncimage(image, outFilePath, isRgb))
			{
				LOGF(eERROR, "Couldn't store ktx compressed image: %s", outFilePath);
				error = true;
			}
		}
		else 
		{
			if (store_cimage(cimage, outFilePath))
			{
				LOGF(eERROR, "Couldn't store compressed image: %s", outFilePath);
				error = true;
			}
		}

		// We are done with the uncompressed image
		free_image(image);
		free_cimage(cimage);
	}
	
	if (imgFiles)
		tf_free(imgFiles);
	
	return error;
}

bool WriteZip(AssetPipelineParams* assetParams, WriteZipParams* zipParams)
{
	char** filteredFiles = NULL;
	int filteredFileCount = 0;
	
	IFileSystem zipWriteFileSystem = { 0 };
	
	const char* outPath = fsGetResourceDirectory(assetParams->mRDOutput);
	
	const char* zipFileName = zipParams->mZipFileName;
	
	//fsAppendPathExtension(zipFileName, ".zip", zipFileName);
	
	// Delete zip file before opening
	char fPath[FS_MAX_PATH];
	fsAppendPathComponent(outPath, zipFileName, fPath);
	remove(fPath);
	
	if (!initZipFileSystem(assetParams->mRDOutput, zipFileName, FM_WRITE, NULL, &zipWriteFileSystem))
	{
		LOGF(eERROR, "Failed to open zip file for write.");
		
		bool error = true;
		return error;
	}
	
	fsSetPathForResourceDir(&zipWriteFileSystem, RM_CONTENT, assetParams->mRDZipInput, "");
	
	FileStream file = {};
	FileStream zipFile = {};
	bool error = false;
	
	for (int i = 0; i < zipParams->mFiltersCount; ++i)
	{
		fsGetFilesWithExtension(assetParams->mRDInput, "", zipParams->mFilters[i], &filteredFiles, &filteredFileCount);
		for (int j = 0; j < filteredFileCount; ++j)
		{
			const char* fileName = filteredFiles[j];
			
			if (!fsOpenStreamFromPath(assetParams->mRDInput, fileName, FM_READ_BINARY, nullptr, &file))
			{
				LOGF(eERROR, "Coudln't open file '%s' for read.", fileName);
				error = true;
			}
			
			ssize_t fileSize = fsGetStreamFileSize(&file);
			void* fileData = (void*)tf_malloc(fileSize);
			fsReadFromStream(&file, fileData, fileSize);
			
			if (!fsOpenStreamFromPath(assetParams->mRDZipInput, fileName, FM_WRITE_BINARY, nullptr, &zipFile))
			{
				LOGF(eERROR, "Coudln't open file '%s' for write.", zipFileName);
				error = true;
			}
			
			// Write to zip
			size_t writtenBytes = fsWriteToStream(&zipFile, fileData, fileSize);

			if ((ssize_t)writtenBytes != fileSize) {
				LOGF(eERROR, "Couldn't write %ul bytes into file '%s'. Wrote %ul bytes instead.", (unsigned long)fileSize, zipFileName, (unsigned long)writtenBytes);
				error = true;
			}
			
			if (!fsCloseStream(&zipFile))
			{
				LOGF(eERROR, "Failed to close write stream for file '%s'.", zipFileName);
				error = true;
			}
			
			if (!fsCloseStream(&file))
			{
				LOGF(eERROR, "Failed to close write stream for file '%s'.", fileName);
				error = true;
			}
			
			tf_free(fileData);
		}
		
		if (filteredFiles)
			tf_free(filteredFiles);
	}
	
	exitZipFileSystem(&zipWriteFileSystem);
	
	return error;
}

void RecurseWriteZipSubDir(const AssetPipelineParams* assetParams, IFileSystem* zipWriteFileSystem, const char* zipFileName, const char* subDir, bool& error)
{
	char ** subDirectories = NULL;
	int subDirectoryCount = 0;
	fsGetSubDirectories(assetParams->mRDInput, subDir, &subDirectories, &subDirectoryCount);
	
	FileStream file = {};
	FileStream zipFile = {};
	
	if (subDirectories)
	{
		for (int i = 0; i < subDirectoryCount; ++i)
		{
			char* subDir = subDirectories[i];
			
			char** subSubDirectories = NULL;
			int subSubDirectoryCount = 0;
			
			char path[FS_MAX_PATH] = { 0 };
			fsAppendPathComponent(path, subDir, path);
			
			fsGetSubDirectories(assetParams->mRDInput, path, &subSubDirectories, &subSubDirectoryCount);
			
			RecurseWriteZipSubDir(assetParams, zipWriteFileSystem, zipFileName, path, error);
			
			if(subSubDirectories)
				tf_free(subSubDirectories);
			
		}
	}
	
	char** assetFiles = NULL;
	int assetFileCount = 0;
	char zipFilePath[FS_MAX_PATH] = { 0 };
	fsAppendPathComponent(subDir, zipFileName, zipFilePath);
	fsGetFilesWithExtension(assetParams->mRDInput, subDir, "*", &assetFiles, &assetFileCount);
	if (assetFiles)
	{
		for (int i = 0; i < assetFileCount; ++i)
		{
			const char* fileName = assetFiles[i];
			LOGF(eINFO, "Zipping %s into %s", fileName, zipFileName);
			
			if (STRCMP(fileName, zipFilePath))
			{
				continue;
			}
			
			if(!fsFileExist(assetParams->mRDInput, fileName))
			{
				continue;
			}
			
			if (!fsOpenStreamFromPath(assetParams->mRDInput, fileName, FM_READ_BINARY, nullptr, &file))
			{
				LOGF(eERROR, "Coudln't open file '%s' for read.", fileName);
				error = true;
			}
			
			ssize_t fileSize = fsGetStreamFileSize(&file);
			void* fileData = (void*)tf_malloc(fileSize);
			fsReadFromStream(&file, fileData, fileSize);

			if (!fsOpenStreamFromPath(assetParams->mRDZipInput, fileName, FM_WRITE_BINARY, nullptr, &zipFile))
			{
				LOGF(eERROR, "Coudln't open file '%s' for write.", zipFileName);
				error = true;
			}
			
			// Write to zip
			size_t writtenBytes = fsWriteToStream(&zipFile, fileData, fileSize);

			if ((ssize_t)writtenBytes != fileSize) {
				LOGF(eERROR, "Couldn't write %ul bytes into file '%s'. Wrote %ul bytes instead.", (unsigned long)fileSize, zipFileName, (unsigned long)writtenBytes);
				error = true;
			}
			
			if (!fsCloseStream(&zipFile))
			{
				LOGF(eERROR, "Failed to close write stream for file '%s'.", zipFileName);
				error = true;
			}
			
			if (!fsCloseStream(&file))
			{
				LOGF(eERROR, "Failed to close write stream for file '%s'.", fileName);
				error = true;
			}
			
			tf_free(fileData);
		}
	}

	if(assetFiles)
		tf_free(assetFiles);
	
	if (subDirectories)
		tf_free(subDirectories);
}

bool ZipAllAssets(AssetPipelineParams* assetParams, WriteZipParams* zipParams)
{
	IFileSystem zipWriteFileSystem = { 0 };
	
	const char* outPath = fsGetResourceDirectory(assetParams->mRDOutput);
	
	const char* zipFileName = zipParams->mZipFileName;
	
	if (!assetParams->mSettings.force && fsFileExist(assetParams->mRDOutput, zipFileName))
	{
		return false;
	}
	
    // Delete zip file before opening
    char fPath[FS_MAX_PATH];
    fsAppendPathComponent(outPath, zipFileName, fPath);
    remove(fPath);

	if (!initZipFileSystem(assetParams->mRDOutput, zipFileName, FM_WRITE, NULL, &zipWriteFileSystem))
	{
		LOGF(eERROR, "Failed to open zip file for write.");

		bool error = true;
		return error;
	}
	
	fsSetPathForResourceDir(&zipWriteFileSystem, RM_CONTENT, assetParams->mRDZipInput, "");

	bool error = false;

	if (zipParams->mFiltersCount > 0)
	{
		for(int i = 0; i < zipParams->mFiltersCount && !error; ++i)
		{
			RecurseWriteZipSubDir(assetParams, &zipWriteFileSystem, zipFileName, zipParams->mFilters[i], error);
		}
	}
	else if (assetParams->mInDir && assetParams->mInDir[0] != '\0')
	{
		RecurseWriteZipSubDir(assetParams, &zipWriteFileSystem, zipFileName, "", error);
	}
	else
	{
		RecurseWriteZipSubDir(assetParams, &zipWriteFileSystem, zipFileName, "Animation", error);
		RecurseWriteZipSubDir(assetParams, &zipWriteFileSystem, zipFileName, "Audio", error);
		RecurseWriteZipSubDir(assetParams, &zipWriteFileSystem, zipFileName, "Meshes", error);
		RecurseWriteZipSubDir(assetParams, &zipWriteFileSystem, zipFileName, "Textures", error);
		RecurseWriteZipSubDir(assetParams, &zipWriteFileSystem, zipFileName, "Shaders", error);
		RecurseWriteZipSubDir(assetParams, &zipWriteFileSystem, zipFileName, "CompiledShaders", error);
		RecurseWriteZipSubDir(assetParams, &zipWriteFileSystem, zipFileName, "Fonts", error);
		RecurseWriteZipSubDir(assetParams, &zipWriteFileSystem, zipFileName, "GPUCfg", error);
		RecurseWriteZipSubDir(assetParams, &zipWriteFileSystem, zipFileName, "Scripts", error);
		RecurseWriteZipSubDir(assetParams, &zipWriteFileSystem, zipFileName, "Other", error);
	}
	
	exitZipFileSystem(&zipWriteFileSystem);
	
	return error;
}

int AssetPipelineRun(AssetPipelineParams* assetParams)
{	
	if (assetParams->mProcessType == PROCESS_ANIMATIONS)
	{
		SkeletonAndAnimations* skeletonAndAnims = nullptr;
		if (assetParams->mInFilePath && assetParams->mInFilePath[0] != '\0')
		{
			LOGF(eINFO, "ProcessAnimations with input filepath '%s'", assetParams->mInFilePath);

			char filename[FS_MAX_PATH] = {};
			fsGetPathFileName(assetParams->mInFilePath, filename);

			SkeletonAndAnimations skeleton = {};
			skeleton.mSkeletonName = bdynfromcstr(filename);
			skeleton.mSkeletonInFile = bdynfromcstr(assetParams->mInFilePath);
			skeleton.mSkeletonOutFile = bdynfromcstr("skeleton.ozz");

			SkeletonAndAnimations::AnimationFile anim = {};
			anim.mInputAnim = bdynfromcstr(assetParams->mInFilePath);
			anim.mOutputAnimPath = bdynfromcstr("");
			arrpush(skeleton.mAnimations, anim);
			arrpush(skeletonAndAnims, skeleton);
		}
		else
		{
			DiscoverAnimations(assetParams->mRDInput, &skeletonAndAnims);
		}

		ProcessAnimationsParams processAnimationParams = {};
		processAnimationParams.pSkeletonAndAnims = skeletonAndAnims;

		for (int32_t i = 0; i < assetParams->mFlagsCount; ++i)
		{
			if(strcmp(assetParams->mFlags[i], "--optimizetracks") == 0)
				processAnimationParams.mAnimationSettings.mOptimizeTracks = true;
		}
		const bool result = ProcessAnimations(assetParams, &processAnimationParams);
		ReleaseSkeletonAndAnimationParams(skeletonAndAnims);
		return result;
	}
	
	if (assetParams->mProcessType == PROCESS_VIRTUAL_TEXTURES)
	{
		return ProcessVirtualTextures(assetParams);
	}
	
	if (assetParams->mProcessType == PROCESS_TFX)
	{
		ProcessTressFXParams tfxParams;
		
		return ProcessTFX(assetParams, &tfxParams);
	}
	
	if (assetParams->mProcessType == PROCESS_GLTF)
	{
		return ProcessGLTF(assetParams);
	}
	
	if (assetParams->mProcessType == PROCESS_TEXTURES)
	{
		ProcessTexturesParams texturesParams;
		
		texturesParams.mInExt = assetParams->mInExt;
		texturesParams.mOutExt = "";
		
		bool error = false;
		for(int i = 0; i < assetParams->mFlagsCount; i++)
		{
			char* flag = assetParams->mFlags[i];
			
			if (STRCMP(flag, "--in-all"))
			{
				if (texturesParams.mInExt && texturesParams.mInExt[0] != '\0')
				{
					LOGF(eERROR, "Already set input format to %s.", texturesParams.mInExt);
					error = true;
				}
				
				texturesParams.mInExt = "*";
			}
			else if (STRCMP(flag, "--in-jpg"))
			{
				if (texturesParams.mInExt && texturesParams.mInExt[0] != '\0')
				{
					LOGF(eERROR, "Already set input format to %s.", texturesParams.mInExt);
					error = true;
				}
				
				texturesParams.mInExt = "jpg";
			}
			else if (STRCMP(flag, "--in-png"))
			{
				if (texturesParams.mInExt && texturesParams.mInExt[0] != '\0')
				{
					LOGF(eERROR, "Already set input format to %s.", texturesParams.mInExt);
					error = true;
				}
				
				texturesParams.mInExt = "png";
			}
			else if (STRCMP(flag, "--in-ktx"))
			{
				if (texturesParams.mInExt && texturesParams.mInExt[0] != '\0')
				{
					LOGF(eERROR, "Already set input format to %s.", texturesParams.mInExt);
					error = true;
				}
				
				texturesParams.mInExt = "ktx";
			}
			else if (STRCMP(flag, "--in-dds"))
			{
				if (texturesParams.mInExt && texturesParams.mInExt[0] != '\0')
				{
					LOGF(eERROR, "Already set input format to %s.", texturesParams.mOutExt);
					error = true;
				}
				
				texturesParams.mInExt = "dds";
			}
			else if (STRCMP(flag, "--out-png"))
			{
				if (texturesParams.mInExt && texturesParams.mInExt[0] != '\0')
				{
					LOGF(eERROR, "Already set output format to %s.", texturesParams.mOutExt);
					error = true;
				}
				
				texturesParams.mOutExt = "png";
			}
			else if (STRCMP(flag, "--out-ktx"))
			{
				if (texturesParams.mOutExt && texturesParams.mOutExt[0] != '\0')
				{
					LOGF(eERROR, "Already set output format to %s.", texturesParams.mOutExt);
					error = true;
				}
				
				texturesParams.mOutExt = "ktx";
			}
			else if (STRCMP(flag, "--out-dds"))
			{
				if (texturesParams.mOutExt && texturesParams.mOutExt[0] != '\0')
				{
					LOGF(eERROR, "Already set output format to %s.", texturesParams.mOutExt);
					error = true;
				}
				
				texturesParams.mOutExt = "dds";
			}
			else
			{
				LOGF(eERROR, "Unrecognized flag %s.", flag);
				
				error = true;
			}
		}
		
		if (!texturesParams.mInExt)
		{
			LOGF(eERROR, "Texture input format isn't set.");
			error = true;
		}
		
		if (!texturesParams.mOutExt)
		{
			LOGF(eERROR, "Texture output format isn't set.");
			error = true;
		}
		
		if (error)
			return 1;
		
		return ProcessTextures(assetParams, &texturesParams);
	}
	
	if (assetParams->mProcessType == PROCESS_WRITE_ZIP || assetParams->mProcessType == PROCESS_WRITE_ZIP_ALL)
	{
		WriteZipParams zipParams;
		
		zipParams.mZipFileName = assetParams->mProcessType == PROCESS_WRITE_ZIP ? "Output.zip" : "Assets.zip";
		zipParams.mFiltersCount = 0;
		
		for(int i = 0; i < assetParams->mFlagsCount; i++)
		{
			char* flag = assetParams->mFlags[i];
			
			if (STRCMP(flag, "--filter"))
			{
				i++;
				zipParams.mFiltersCount = 0;
				for(; i < assetParams->mFlagsCount; i++)
				{
					char*& filter = assetParams->mFlags[i];
					
					zipParams.mFilters[zipParams.mFiltersCount++] = filter;

					// next argument is a flag, not an extension
					if (i + 1 == assetParams->mFlagsCount || assetParams->mFlags[i + 1][0] == '-')
						break;
				}
			}
			else if (STRCMP(flag, "--name"))
			{
				i++;
				zipParams.mZipFileName = assetParams->mFlags[i];
			}
			else
			{
				LOGF(eERROR, "Unrecognized flag %s.", flag);
			}
		}
				
		if(assetParams->mProcessType == PROCESS_WRITE_ZIP)
		{
			return WriteZip(assetParams, &zipParams);
		}
		else
		{
			return ZipAllAssets(assetParams, &zipParams);
		}
	}
	
	// if reached, the used processed type wasn't included
	ASSERT(false);
	return 0;
}
