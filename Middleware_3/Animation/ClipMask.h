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

#include "../../Common_3/ThirdParty/OpenSource/ozz-animation/include/ozz/base/memory/allocator.h"

#include "Rig.h"

// Allows for masking the influence of a clip on a Rig
class ClipMask
{
	public:
	// Set up a mask associated with a rig so that it can modify any clips for that rig
	void Initialize(Rig* rig);

	// Must be called to clean up if the clip mask was initialized
	void Destroy();

	// Will set the weight of all joints to 1.0f
	void EnableAllJoints();

	// Will set the weight of all joints to 0.0f
	void DisableAllJoints();

	// Will set the weight of the joint at jointIndex and all of its children
	// to setValue. All other joints will be set to zero
	void SetAllChildrenOf(int jointIndex, float setValue);

	// Get the joint weights
	inline ozz::Range<Vector4> GetJointWeights() { return mJointWeights; };

	private:
	// Pointer to the rig that this clip mask corresponds to
	Rig* mRig;

	// Per-joint weights used to define the partial animation mask. Allows to
	// select which joints are considered during blending, and their individual
	// weight_setting.
	ozz::Range<Vector4> mJointWeights;
};