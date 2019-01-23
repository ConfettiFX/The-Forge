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

#include "../../Common_3/OS/Math/MathTypes.h"

#include "../../Common_3/ThirdParty/OpenSource/ozz-animation/include/ozz/animation/runtime/blending_job.h"
#include "../../Common_3/ThirdParty/OpenSource/ozz-animation/include/ozz/animation/runtime/local_to_model_job.h"

#include "Rig.h"
#include "Animation.h"

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
