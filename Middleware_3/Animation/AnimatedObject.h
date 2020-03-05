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

#include "../../Common_3/ThirdParty/OpenSource/ozz-animation/include/ozz/animation/runtime/blending_job.h"
#include "../../Common_3/ThirdParty/OpenSource/ozz-animation/include/ozz/animation/runtime/local_to_model_job.h"

#include "Rig.h"
#include "Animation.h"

struct AimIKDesc
{
	// Joint forward axis, in joint local-space, to be aimed at target position.
	Vector3        mForward;
	// Offset position from the joint in local-space, that will aim at target.
	Vector3        mOffset;
	// Pole vector, in model-space. The pole vector defines the direction
	// the up should point to.
	Vector3        mPoleVector;
	// Twist_angle rotates joint around the target vector.
	float          mTwistAngle;
	// Weight given to the IK correction clamped in range [0,1]. Applied to each joint in chain.
	float          mJointWeight;
	// Chain Length
	int            mJointChainLength;
	// Array of joint indexes.
	const int*     mJointChain;
	// Array of joint up axises, in joint local-space, used to keep the joint oriented in the
	// same direction as the pole vector.
	const Vector3* mJointUpVectors;
	// Optional boolean output value, set to true if target can be reached with IK
	// computations.
	bool mReached;
};

struct TwoBonesIKDesc
{
	// Pole vector, in model-space. The pole vector defines the direction the
	// middle joint should point to, allowing to control IK chain orientation.
	Vector3 mPoleVector;
	// Normalized middle joint rotation axis, in middle joint local-space.
	Vector3 mMidAxis;
	// Weight given to the IK correction clamped in range [0,1].
	float mWeight;
	// Soften ratio allows the chain to gradually fall behind the target
	// position. This prevents the joint chain from snapping into the final
	// position, softening the final degrees before the joint chain becomes flat.
	// This ratio represents the distance to the end, from which softening is
	// starting.
	float mSoften;
	// Twist_angle rotates IK chain around the vector define by start-to-target
	// vector.
	float mTwistAngle;
	// Array of joint indexes.
	int mJointChain[3];
	// Optional boolean output value, set to true if target can be reached with IK
	// computations.
	bool mReached;
};

// Responsible for coordinating the posing of a Rig by an Animation
class AnimatedObject
{
	public:
	// Set up an Animated object with the Rig it will be posing and the default animation to play when idle
	void Initialize(Rig* rig, Animation* animation);

	// Must be called to clean up the system if it has been initialized
	void Destroy();

	// To be called every frame of the main application, handles sampling and updating the current animation
	bool Update(float dt);

	bool AimIK(AimIKDesc* params, Point3 target);

	// Apply two bone inverse kinematic
	bool TwoBonesIK(TwoBonesIKDesc* params, Point3 target);

	// Update mRigs world matricies
	inline void PoseRig() { mRig->Pose(mRootTransform); };

	// Have mRig update to display in its bind pose
	void PoseRigInBind();

	// Set the animation
	inline void SetAnimation(Animation* animation) { mAnimation = animation; };

	// Set the root transform of the object
	inline void SetRootTransform(const Matrix4& rootTransform) { mRootTransform = rootTransform; };

	// Get the rig of this animated object
	inline Rig* GetRig() { return mRig; };

	private:
	// The Rig the AnimatedObject will be posing
	Rig* mRig;

	// Pointer to the animation we are sampling
	Animation* mAnimation;

	// Buffer of local transforms as sampled from the animation.
	ozz::Range<SoaTransform> mLocalTrans;

	// Transform to apply to entire rig
	Matrix4 mRootTransform = Matrix4::identity();
};
