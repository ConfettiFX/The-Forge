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

#pragma once

#include "../../../OS/Interfaces/IFileSystem.h"

#include "../../../ThirdParty/OpenSource/ozz-animation/include/ozz/animation/runtime/skeleton.h"
#include "../../../ThirdParty/OpenSource/ozz-animation/include/ozz/animation/runtime/animation.h"
#include "../../../ThirdParty/OpenSource/EASTL/string.h"

#include "gltfpack.h"

struct TFXAsset
{
	// Hair data from *.tfx
	eastl::vector<float4> mPositions;
	eastl::vector<float2> mStrandUV;
	eastl::vector<float4> mRefVectors;
	eastl::vector<float4> mGlobalRotations;
	eastl::vector<float4> mLocalRotations;
	eastl::vector<float4> mTangents;
	eastl::vector<float4> mFollowRootOffsets;
	eastl::vector<int>    mStrandTypes;
	eastl::vector<float>  mThicknessCoeffs;
	eastl::vector<float>  mRestLengths;
	eastl::vector<int>    mTriangleIndices;
	int                   mNumVerticesPerStrand;
	int                   mNumGuideStrands;
};

struct TFXVertex
{
	float3 mPosition;
	float3 mNormal;
	uint   mBoneIndices[4];
	float  mBoneWeights[4];
};

struct TFXMesh
{
	eastl::vector<eastl::string> mBones;
	eastl::vector<TFXVertex>     mVertices;
	eastl::vector<uint>          mIndices;
};

typedef enum AssetProcessFlags
{
	alFLAGS_NONE = 0x0,
	alGEN_NORMALS = 0x1,
	alGEN_MATERIAL_ID = 0x2,
	alMAKE_LEFT_HANDED = 0x4,
	alOPTIMIZE = 0x8,
	alSTRIPIFY = 0x10,
	alIS_QUANTIZED = 0x20,
}AssetProcessFlags;

class AssetLoader
{
	public:
	static bool LoadSkeleton(const Path* skeletonFile, ozz::animation::Skeleton* skeleton);
	static bool LoadAnimation(const Path* animationFile, ozz::animation::Animation* animation);
	static bool LoadModel(const Path* modelFile, Model* model, unsigned int flags = alFLAGS_NONE);
};
