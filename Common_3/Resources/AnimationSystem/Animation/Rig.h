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

#include "../../../Utilities/Math/MathTypes.h"

#include "../ThirdParty/OpenSource/ozz-animation/include/ozz/animation/runtime/skeleton.h"
#include "../ThirdParty/OpenSource/ozz-animation/include/ozz/animation/runtime/skeleton_utils.h"

#include "../ThirdParty/OpenSource/ozz-animation/include/ozz/base/span.h"
#include "../ThirdParty/OpenSource/ozz-animation/include/ozz/base/io/archive.h"
#include "../ThirdParty/OpenSource/ozz-animation/include/ozz/base/memory/allocator.h"

#include "../../../Utilities/Interfaces/ILog.h"

// Stores skeleton properties and posable by animations
class FORGE_API Rig
{
	public:
	// Sets up the rig by loading the skeleton from an ozz skeleton file
	void Initialize(const ResourceDirectory resourceDir, const char* fileName, const char* filePassword);

	// Must be called to clean up the object if it was initialized
	void Exit();

	// Updates the skeleton's joint and bone world matricies based on mJointModelMats
	void Pose(const Matrix4& rootTransform);
	
	// Finds the index of the joint with name jointName, if it cannot find it returns -1
	int32_t FindJoint(const char* jointName);

	// Finds the indexes of joint chain with names joinNames
	void FindJointChain(const char* jointNames[], size_t numNames, int32_t jointChain[]);

	// Runtime skeleton.
	ozz::animation::Skeleton mSkeleton;

	// The number of soa elements matching the number of joints of the
	// skeleton. This value is useful to allocate SoA runtime data structures.
	uint32_t mNumSoaJoints = 0;

	// The number of joints of the skeleton
	uint32_t mNumJoints = 0;

	// Location of the root joint
	uint32_t mRootIndex = 0;

private:
	// Load a runtime skeleton from a skeleton.ozz file
	bool LoadSkeleton(const ResourceDirectory resourceDir, const char* fileName, const char* filePassword);
};
