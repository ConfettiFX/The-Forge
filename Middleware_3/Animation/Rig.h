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

#include "../../Common_3/OS/Math/MathTypes.h"

#include "../../Common_3/ThirdParty/OpenSource/EASTL/vector.h"

#include "../../Common_3/ThirdParty/OpenSource/ozz-animation/include/ozz/animation/runtime/skeleton.h"
#include "../../Common_3/ThirdParty/OpenSource/ozz-animation/include/ozz/animation/runtime/skeleton_utils.h"

#include "../../Common_3/ThirdParty/OpenSource/ozz-animation/include/ozz/base/io/archive.h"
#include "../../Common_3/ThirdParty/OpenSource/ozz-animation/include/ozz/base/memory/allocator.h"

#include "../../Common_3/OS/Interfaces/ILog.h"

namespace eastl
{
	template <>
	struct has_equality<Vector3> : eastl::false_type {};
}

// Stores skeleton properties and posable by animations
class Rig
{
	public:
	// Sets up the rig by loading the skeleton from an ozz skeleton file
	void Initialize(const ResourceDirectory resourceDir, const char* fileName);

	// Must be called to clean up the object if it was initialized
	void Destroy();

	// Updates the skeleton's joint and bone world matricies based on mJointModelMats
	void Pose(const Matrix4& rootTransform);

	// Set the color of the joints
	inline void SetJointColor(const Vector4& color) { mJointColor = color; };

	// Set the color of the bones
	inline void SetBoneColor(const Vector4& color) { mBoneColor = color; };

	// Toggle whether or not to update the bones world matricies
	inline void SetUpdateBones(bool setValue) { mUpdateBones = setValue; };

	// For hard setting the world matrix of a specific joint
	inline void HardSetJointWorldMat(const Matrix4& worldMat, unsigned int index) { mJointWorldMats[index] = worldMat; };

	// Gets a pointer to the skeleton of this rig
	inline ozz::animation::Skeleton* GetSkeleton() { return &mSkeleton; };

	// Gets the world matrix of the joint at index (returns identity if index is invalid)
	inline Matrix4 GetJointWorldMat(unsigned int index)
	{
		if ((0 <= index) && (index < mNumJoints))
		{
			return mJointWorldMats[index];
		}
		else
		{
			return Matrix4::identity();
		}
	};

	// Gets the world matrix without any scale data of the joint at index (returns identity if index is invalid)
	inline Matrix4 GetJointWorldMatNoScale(unsigned int index)
	{
		if ((0 <= index) && (index < mNumJoints))
		{
			mat4 withScale = mJointWorldMats[index];

			// Normalize the first three collumns
			vec4 col0 = vec4(normalize(withScale.getCol0().getXYZ()), withScale.getCol0().getW());
			vec4 col1 = vec4(normalize(withScale.getCol1().getXYZ()), withScale.getCol1().getW());
			vec4 col2 = vec4(normalize(withScale.getCol2().getXYZ()), withScale.getCol2().getW());

			mat4 withoutScale = mat4(col0, col1, col2, withScale.getCol3());
			return withoutScale;
		}
		else
		{
			return Matrix4::identity();
		}
	};

	// Gets the world matrix of the bone with child joint at index (returns identity if index is invalid)
	inline Matrix4 GetBoneWorldMat(unsigned int index)
	{
		if ((0 <= index) && (index < mNumJoints))
		{
			return mBoneWorldMats[index];
		}
		else
		{
			return Matrix4::identity();
		}
	};

	// Gets the joint's model matricies so they can be set by animations
	inline ozz::Range<Matrix4> GetJointModelMats() { return mJointModelMats; };

	// Gets the scale of joint at index
	inline Vector3 GetJointScale(unsigned int index) { return mJointScales[index]; };

	// Gets the SOA num of joints
	inline unsigned int GetNumSoaJoints() { return mNumSoaJoints; };

	// Gets the number of joints
	inline unsigned int GetNumJoints() { return mNumJoints; };

	// Gets the color of the joints
	inline Vector4 GetJointColor() { return mJointColor; };

	// Gets the color of the bones
	inline Vector4 GetBoneColor() { return mBoneColor; };

	// Finds the index of the joint with name jointName, if it cannot find it returns -1
	int FindJoint(const char* jointName);

	// Finds the indexes of joint chain with names joinNames
	void FindJointChain(const char* jointNames[], size_t numNames, int jointChain[]);

	private:
	// Load a runtime skeleton from a skeleton.ozz file
	bool LoadSkeleton(const ResourceDirectory resourceDir, const char* fileName);

	// Runtime skeleton.
	ozz::animation::Skeleton mSkeleton;

	// The number of soa elements matching the number of joints of the
	// skeleton. This value is useful to allocate SoA runtime data structures.
	unsigned int mNumSoaJoints;

	// The number of joints of the skeleton
	unsigned int mNumJoints;

	// Location of the root joint
	unsigned int mRootIndex;

	// Color of the joints
	Vector4 mJointColor = vec4(.9f, .9f, .9f, 1.f);    // white

	// Color of the bones
	Vector4 mBoneColor = vec4(0.9f, 0.6f, 0.1f, 1.0f);    // orange

	// Toggle on whether or not to update the bones world matricies
	bool mUpdateBones = true;

	// Buffer of world model space matrices for joints.
	eastl::vector<Matrix4> mJointWorldMats;

	// Buffer of world model space matrices for bones.
	eastl::vector<Matrix4> mBoneWorldMats;

	// Buffer of joint model space matrices set by animations
	ozz::Range<Matrix4> mJointModelMats;

	// Scales to apply to each joint - will be proportional to length of its child's bone
	eastl::vector<Vector3> mJointScales;
};
