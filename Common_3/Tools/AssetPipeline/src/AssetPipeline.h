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

#pragma once

#include "../../../OS/Interfaces/IFileSystem.h"

#include "../../../ThirdParty/OpenSource/ozz-animation/include/ozz/animation/runtime/skeleton.h"
#include "../../../ThirdParty/OpenSource/ozz-animation/include/ozz/animation/runtime/animation.h"

struct ProcessAssetsSettings
{
	bool quiet;                  // Only output warnings.
	bool force;                  // Force all assets to be processed.
	uint minLastModifiedTime;    // Force all assets older than this to be processed.
	uint quantizePositionBits;   // N value for N-Bit position quantization.
	uint quantizeNormalBits;     // N value for N-Bit normal and tangent quantization.
	uint quantizeTexBits;        // N value for N-Bit texture coordinate quantization.

	// TressFX settings
	uint32_t    mFollowHairCount;
	float       mMaxRadiusAroundGuideHair;
	float       mTipSeperationFactor;
};

class AssetPipeline
{
public:
	static bool ProcessAnimations(const Path* animationDirectory, const Path* outputDirectory, ProcessAssetsSettings* settings);
	static bool CreateRuntimeSkeleton(
		const Path* skeletonAsset, const char* skeletonName, const Path* skeletonOutput, ozz::animation::Skeleton* skeleton,
		ProcessAssetsSettings* settings);
	static bool CreateRuntimeAnimation(
		const Path* animationAsset, ozz::animation::Skeleton* skeleton, const char* skeletonName, const char* animationName,
		const Path* animationOutput, ProcessAssetsSettings* settings);

	static bool ProcessTextures(const Path* textureDirectory, const Path* outputDirectory, ProcessAssetsSettings* settings);
	static bool ProcessVirtualTextures(const Path* textureDirectory, const Path* outputDirectory, ProcessAssetsSettings* settings);
	static bool ProcessTFX(const Path* tfxDirectory, const Path* outputDirectory, ProcessAssetsSettings* settings);
};
