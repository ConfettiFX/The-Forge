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

#include "ClipMask.h"

void ClipMask::Initialize(Rig* rig)
{
	mRig = rig;

	ozz::memory::Allocator* allocator = ozz::memory::default_allocator();

	// Allocates per-joint weights used to mask the animation. Note that
	// this is a Soa structure.
	mJointWeights = allocator->AllocateRange<Vector4>(rig->GetNumSoaJoints());

	EnableAllJoints();
}

void ClipMask::Destroy()
{
	ozz::memory::Allocator* allocator = ozz::memory::default_allocator();
	allocator->Deallocate(mJointWeights);
}

void ClipMask::EnableAllJoints()
{
	// Sets all weights to 1.0f
	for (unsigned int i = 0; i < mRig->GetNumSoaJoints(); i++)
	{
		mJointWeights[i] = Vector4::one();
	}
}

void ClipMask::DisableAllJoints()
{
	// Sets all weights to 0.0f
	for (unsigned int i = 0; i < mRig->GetNumSoaJoints(); i++)
	{
		mJointWeights[i] = Vector4::zero();
	}
}

void ClipMask::SetAllChildrenOf(int jointIndex, float setValue)
{
	// Extracts the list of children of the joint at jointIndex.
	ozz::animation::JointsIterator it;
	ozz::animation::IterateJointsDF(*mRig->GetSkeleton(), jointIndex, &it);

	// Sets the weight_setting of all the joints children to setValue. Note
	// that weights are stored in SoA format.
	for (int i = 0; i < it.num_joints; i++)
	{
		const int jointId = it.joints[i];

		mJointWeights[jointId / 4].setElem(jointId % 4, setValue);
	}
}