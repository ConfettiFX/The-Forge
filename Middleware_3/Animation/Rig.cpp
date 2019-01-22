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

#include "Rig.h"

void Rig::Initialize(const char* skeletonFile)
{
	// Reading skeleton.
	if (!LoadSkeleton(skeletonFile))
		return;    //need error catching

	mNumSoaJoints = mSkeleton.num_soa_joints();
	mNumJoints = mSkeleton.num_joints();

	// Find the root index
	for (unsigned int i = 0; i < mNumJoints; i++)
	{
		if (mSkeleton.joint_properties()[i].parent == ozz::animation::Skeleton::kNoParentIndex)
		{
			mRootIndex = i;
			break;
		}
	}

	mJointWorldMats = tinystl::vector<Matrix4>(mNumJoints, Matrix4::identity());
	mBoneWorldMats = tinystl::vector<Matrix4>(mNumJoints, Matrix4::identity());
	mJointScales = tinystl::vector<Vector3>(mNumJoints, Vector3(1.0f, 1.0f, 1.0f));

	// Allocates joint model matrix buffer
	ozz::memory::Allocator* allocator = ozz::memory::default_allocator();
	mJointModelMats = allocator->AllocateRange<Matrix4>(mNumJoints);
}

void Rig::Destroy()
{
	mSkeleton.Deallocate();

	ozz::memory::Allocator* allocator = ozz::memory::default_allocator();
	allocator->Deallocate(mJointModelMats);
}

void Rig::Pose(const Matrix4& rootTransform)
{
	// Set the world matrix of each joint
	for (unsigned int jointIndex = 0; jointIndex < mNumJoints; jointIndex++)
	{
		mJointWorldMats[jointIndex] = rootTransform * mJointModelMats[jointIndex];
	}

	// If we wish to update the world matricies of the bones and the scales of the joints
	// based on the distance between each joint
	if (mUpdateBones)
	{
		// Traverses through the skeleton's joint hierarchy, placing bones between
		// joints and altering the size of joints and bones to reflect distances
		// between joints

		// Store smallest bone lenth to be reused for root joint scale
		float minBoneLen = 0.f;
		bool  minBoneLenSet = false;

		// For each joint
		for (unsigned int childIndex = 0; childIndex < mNumJoints; childIndex++)
		{
			// Do not make a bone if it is the root
			// Handle the root joint specially after the loop
			if (childIndex == mRootIndex)
			{
				mBoneWorldMats[childIndex] = mat4::scale(vec3(0.0f, 0.0f, 0.0f));
				continue;
			}

			// Get the index of the parent of childIndex
			const int parentIndex = mSkeleton.joint_properties()[childIndex].parent;

			// Selects joint matrices.
			const mat4 parentMat = mJointModelMats[parentIndex];
			const mat4 childMat = mJointModelMats[childIndex];

			vec3  boneDir = childMat.getCol3().getXYZ() - parentMat.getCol3().getXYZ();
			float boneLen = length(boneDir);

			// Save smallest boneLen for the root joints scale size
			if ((!minBoneLenSet) || (boneLen < minBoneLen))
			{
				minBoneLen = boneLen;
			}

			// Use the parent and child world matricies to create a bone world
			// matrix which will place it between the two joints
			// Using Gramm Schmidt process'
			float dotProd = dot(parentMat.getCol2().getXYZ(), boneDir);
			vec3  binormal = abs(dotProd) < 0.01f ? parentMat.getCol2().getXYZ() : parentMat.getCol1().getXYZ();

			vec4 col0 = vec4(boneDir, 0.0f);
			vec4 col1 = vec4(boneLen * normalize(cross(binormal, boneDir)), 0.0f);
			vec4 col2 = vec4(boneLen * normalize(cross(boneDir, col1.getXYZ())), 0.0f);
			vec4 col3 = vec4(parentMat.getCol3().getXYZ(), 1.0f);
			mBoneWorldMats[childIndex] = rootTransform * mat4(col0, col1, col2, col3);

			// Sets the scale of the joint equivilant to the boneLen between it and its parent joint
			// Separete from world so outside objects can use a joint's world mat w/o its scale
			mJointScales[childIndex] = vec3(boneLen / 2.0f);
		}

		// Set the root joints scale based on the saved min value
		mJointScales[mRootIndex] = vec3(minBoneLen / 2.0f);
	}
}

bool Rig::LoadSkeleton(const char* fileName)
{
	ozz::io::File file(fileName, "rb");
	if (!file.opened())
	{
		ErrorMsg("Cannot open skeleton file");
		return false;
	}

	ozz::io::IArchive archive(&file);
	if (!archive.TestTag<ozz::animation::Skeleton>())
	{
		ErrorMsg("Skeleton Archive doesn't contain the expected object type");
		return false;
	}

	archive >> mSkeleton;

	return true;
}

int Rig::FindJoint(const char* jointName)
{
	for (unsigned int i = 0; i < mNumJoints; i++)
	{
		if (strstr(mSkeleton.joint_names()[i], jointName))
			return i;
	}
	return -1;
}
