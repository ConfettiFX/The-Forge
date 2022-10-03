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

#pragma once

#include "../../../Utilities/Interfaces/IFileSystem.h"

#include "../../../Utilities/ThirdParty/OpenSource/bstrlib/bstrlib.h"

#include "../../../Resources/AnimationSystem/ThirdParty/OpenSource/ozz-animation/include/ozz/animation/runtime/skeleton.h"
#include "../../../Resources/AnimationSystem/ThirdParty/OpenSource/ozz-animation/include/ozz/animation/runtime/animation.h"

#define STRCMP(a, b) (stricmp(a, b) == 0)

#define MAX_FLAGS 100
#define MAX_FILTERS 100

// Error codes returned by AssetPipelineRun
typedef enum AssetPipelineErrorCode
{
	ASSET_PIPELINE_SUCCESS = 0,
	ASSET_PIPELINE_GENERAL_FAILURE = 1,
	// TODO: add more error codes as needed
} AssetPipelineErrorCode;

enum AssetPipelineProcessMode {
	PROCESS_MODE_FILE,
	PROCESS_MODE_DIRECTORY,
	
	PROCESS_MODE_NONE
};

struct ProcessAssetsSettings
{
	bool quiet;                  // Only output warnings.
	bool force;                  // Force all assets to be processed.
	uint minLastModifiedTime;    // Force all assets older than this to be processed.
};

enum AssetPipelineProcess {
	PROCESS_ANIMATIONS,
	PROCESS_VIRTUAL_TEXTURES,
	PROCESS_TFX,
	PROCESS_GLTF,
	PROCESS_TEXTURES,
	PROCESS_WRITE_ZIP,
	PROCESS_WRITE_ZIP_ALL,
};

struct AssetPipelineProcessCommand {
	const char* mCommandString;
	AssetPipelineProcess mProcessType;
};

struct AssetPipelineParams
{
	// If processing individual file
	const char* mInFilePath;
	
	const char* mInExt;
	
	const char* mInDir;
	const char* mOutDir;
	
	// From argc and argv
	char* mFlags[MAX_FLAGS];
	int32_t mFlagsCount;
	
	AssetPipelineProcess mProcessType;
	AssetPipelineProcessMode mPathMode; // FILE or DIRECTORYxw
	ProcessAssetsSettings mSettings;

	ResourceDirectory mRDInput;
	ResourceDirectory mRDOutput;
	ResourceDirectory mRDZipOutput;
	ResourceDirectory mRDZipInput;
};

struct SkeletonAndAnimations
{
	struct AnimationFile
	{
		// Paths relative to AssetPipeline in/out directories
		bstring mInputAnim;
		bstring mOutputAnimPath;
	};

	bstring mSkeletonName;

	// Paths relative to AssetPipeline in/out directories
	bstring mSkeletonInFile;
	bstring mSkeletonOutFile;

	AnimationFile* mAnimations; // stbds array
};

struct ProcessTexturesParams {
	const char* mInExt;
	const char* mOutExt;
};

struct ProcessTressFXParams {
	uint32_t    mFollowHairCount;
	float       mMaxRadiusAroundGuideHair;
	float       mTipSeperationFactor;
};

struct WriteZipParams {
	char* mFilters[MAX_FILTERS];
	int mFiltersCount;
	const char* mZipFileName;
};

struct RuntimeAnimationSettings
{
	// Enablind this might introduce noise to the animation
	bool mOptimizeTracks = false;

	// ozz defaults, 1mm tolerance and 10cm distance
	float mOptimizationTolerance = 1e-3f;
	float mOptimizationDistance = 1e-1f;

	// TODO: Add settings per joints, we might want root joint to have less optimization because it affects all the joints of the skeleton and everything might look shaky
	//       See joints_setting_override variable in ozz::animation::offline::AnimationOptimizer for more information on this
};

struct ProcessAnimationsParams
{
	SkeletonAndAnimations* pSkeletonAndAnims = nullptr;
	RuntimeAnimationSettings mAnimationSettings; // Global settings applied to all animations
};

bool CreateRuntimeSkeleton(
	ResourceDirectory resourceDirInput, const char* skeletonInfile,
	ResourceDirectory resourceDirOutput, const char* skeletonOutputFile, 
	ozz::animation::Skeleton* pOutSkeleton,
	const char* skeletonName,
	ProcessAssetsSettings* settings);

bool CreateRuntimeAnimations(
	ResourceDirectory resourceDirInput, const char* animationInput,
	ResourceDirectory resourceDirOutput, const char* animationOutput,
	ozz::animation::Skeleton* skeleton,
	const char* skeletonName,
	RuntimeAnimationSettings* animationSettings,
	ProcessAssetsSettings* settings);

void DiscoverAnimations(ResourceDirectory resourceDirInput, SkeletonAndAnimations** pOutArray);
bool ProcessAnimations(AssetPipelineParams* assetParams, ProcessAnimationsParams* pProcessAnimationsParams);

bool ProcessVirtualTextures(AssetPipelineParams* assetParams);
bool ProcessTFX(AssetPipelineParams* assetParams, ProcessTressFXParams* tfxParams);
bool ProcessGLTF(AssetPipelineParams* assetParams);
bool ProcessTextures(AssetPipelineParams* assetParams, ProcessTexturesParams* texturesParams);
bool WriteZip(AssetPipelineParams* assetParams, WriteZipParams* zipParams);
bool ZipAllAssets(AssetPipelineParams* assetParams, WriteZipParams* zipParams);

// Error code 0 means success, see AssetPipelineErrorCode for other codes
int AssetPipelineRun(AssetPipelineParams* assetParams);
