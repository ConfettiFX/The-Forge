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

#include "AnimatedObject.h"

void AnimatedObject::Initialize(Rig* rig, Animation* animation)
{
	mRig = rig;
	mAnimation = animation;

	ozz::memory::Allocator* allocator = ozz::memory::default_allocator();

	// Allocates sampler runtime buffer.
	mLocalTrans = allocator->AllocateRange<SoaTransform>(rig->GetNumSoaJoints());
}

void AnimatedObject::Destroy()
{
	ozz::memory::Allocator* allocator = ozz::memory::default_allocator();
	allocator->Deallocate(mLocalTrans);
}

bool AnimatedObject::Update(float dt)
{
	// sample the current animation to get mLocalTrans
	if (!mAnimation->Sample(dt, mLocalTrans))
		return false;

	// Local to model job

	// Setup local-to-model conversion job.
	ozz::animation::LocalToModelJob ltmJob;
	ltmJob.skeleton = mRig->GetSkeleton();
	ltmJob.input = mLocalTrans;
	ltmJob.output = mRig->GetJointModelMats();    // Save results in mRig's model mat buffer

	// Runs ltm job.
	if (!ltmJob.Run())
		return false;

	return true;
}

void AnimatedObject::PoseRigInBind()
{
	// Setup local-to-model conversion job.
	ozz::animation::LocalToModelJob ltmJob;
	ltmJob.skeleton = mRig->GetSkeleton();
	ltmJob.input = mRig->GetSkeleton()->bind_pose();    // Use the skeleton's bind pose
	ltmJob.output = mRig->GetJointModelMats();          // Save results in mRig's model mat buffer

	// Runs ltm job.
	if (ltmJob.Run())
		PoseRig();
}