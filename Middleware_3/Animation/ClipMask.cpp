/*
 * Copyright (c) 2018-2021 The Forge Interactive Inc.
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

	mJointWeights.resize(rig->GetNumSoaJoints(), Vector4::one());
}

void ClipMask::Destroy()
{
	mJointWeights.set_capacity(0);
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

// Helper functor used to set weights while traversing joints hierarchy.
struct WeightSetupIterator 
{
	WeightSetupIterator(eastl::vector<Vector4>* _weights,
		float _weight_setting)
		: weights(_weights), weight_setting(_weight_setting) {}
	void operator()(int _joint, int) 
	{
		Vector4& soa_weight = weights->at(_joint / 4);
		soa_weight[_joint % 4] = weight_setting;
	}
	eastl::vector<Vector4>* weights;
	float weight_setting;
};

void ClipMask::SetAllChildrenOf(int jointIndex, float setValue)
{
	// Extracts the list of children of the joint at jointIndex.
	WeightSetupIterator it(&mJointWeights, setValue);
	ozz::animation::IterateJointsDF(*mRig->GetSkeleton(), it, jointIndex);
}