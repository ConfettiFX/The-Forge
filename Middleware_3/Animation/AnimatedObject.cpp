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

#include "AnimatedObject.h"
#include "../../Common_3/ThirdParty/OpenSource/ozz-animation/include/ozz/animation/runtime/ik_aim_job.h"
#include "../../Common_3/ThirdParty/OpenSource/ozz-animation/include/ozz/animation/runtime/ik_two_bone_job.h"

namespace {
void MultiplySoATransformQuaternion(int _index, const Quat& _quat, ozz::Range<SoaTransform>& _transforms)
{
	SoaTransform& soa_transform_ref = _transforms[_index / 4];
	Vector4       aos_quats[4];

	transpose4x4(&soa_transform_ref.rotation.x, aos_quats);

	Vector4& aos_quat_ref = aos_quats[_index & 3];
	aos_quat_ref = Vector4((Quat(aos_quat_ref) * _quat).get128());

	transpose4x4(aos_quats, &soa_transform_ref.rotation.x);
}
}    // namespace

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

bool AnimatedObject::AimIK(AimIKDesc* params, Point3 target)
{
	ozz::Range<Matrix4> models = mRig->GetJointModelMats();

	ozz::animation::IKAimJob ikJob;
	ikJob.pole_vector = params->mPoleVector;
	ikJob.twist_angle = params->mTwistAngle;
	ikJob.target = target;
	ikJob.reached = &params->mReached;

	Quat correction;
	ikJob.joint_correction = &correction;

	int previous_joint = ozz::animation::Skeleton::kNoParentIndex;
	for (int i = 0, joint = params->mJointChain[0]; i < params->mJointChainLength;
		 ++i, previous_joint = joint, joint = params->mJointChain[i])
	{
		ikJob.joint = &models[joint];
		ikJob.up = params->mJointUpVectors[i];

		const bool last = i == params->mJointChainLength - 1;
		ikJob.weight = last ? 1.f : params->mJointWeight;

		if (i == 0)
		{
			ikJob.offset = params->mOffset;
			ikJob.forward = params->mForward;
		}
		else
		{
			const Vector3 corrected_forward_ms((models[previous_joint] * rotate(correction, ikJob.forward)).getXYZ());
			const Point3  corrected_offset_ms((models[previous_joint] * Point3(rotate(correction, ikJob.offset))).getXYZ());

			Matrix4 inv_joint = inverse(models[joint]);
			ikJob.forward = (inv_joint * corrected_forward_ms).getXYZ();
			ikJob.offset = (inv_joint * corrected_offset_ms).getXYZ();
		}

		if (!ikJob.Run())
		{
			return false;
		}

		MultiplySoATransformQuaternion(joint, correction, mLocalTrans);
	}

	ozz::animation::LocalToModelJob ltmJob;
	ltmJob.skeleton = mRig->GetSkeleton();
	ltmJob.input = mLocalTrans;
	ltmJob.output = mRig->GetJointModelMats();

	if (!ltmJob.Run())
		return false;

	return true;
}

bool AnimatedObject::TwoBonesIK(TwoBonesIKDesc* params, Point3 target)
{
	ozz::animation::IKTwoBoneJob ik_job;

	ik_job.target = target;
	ik_job.pole_vector = params->mPoleVector;
	ik_job.mid_axis = params->mMidAxis;

	ik_job.weight = params->mWeight;
	ik_job.soften = params->mSoften;
	ik_job.twist_angle = params->mTwistAngle;

	ozz::Range<Matrix4> models = mRig->GetJointModelMats();

	ik_job.start_joint = &models[params->mJointChain[0]];
	ik_job.mid_joint = &models[params->mJointChain[1]];
	ik_job.end_joint = &models[params->mJointChain[2]];

	// Outputs
	Quat start_correction;
	ik_job.start_joint_correction = &start_correction;
	Quat mid_correction;
	ik_job.mid_joint_correction = &mid_correction;
	ik_job.reached = &params->mReached;

	if (!ik_job.Run())
	{
		return false;
	}

	MultiplySoATransformQuaternion(params->mJointChain[0], start_correction, mLocalTrans);
	MultiplySoATransformQuaternion(params->mJointChain[1], mid_correction, mLocalTrans);

	ozz::animation::LocalToModelJob ltmJob;
	ltmJob.skeleton = mRig->GetSkeleton();
	ltmJob.input = mLocalTrans;
	ltmJob.output = mRig->GetJointModelMats();

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