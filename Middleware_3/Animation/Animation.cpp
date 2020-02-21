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

#include "Animation.h"

void Animation::Initialize(AnimationDesc animationDesc)
{
	mRig = animationDesc.mRig;
	mNumClips = min(animationDesc.mNumLayers, MAX_NUM_CLIPS);
	mBlendType = animationDesc.mBlendType;

	ozz::memory::Allocator* allocator = ozz::memory::default_allocator();

	for (unsigned int i = 0; i < mNumClips; i++)
	{
		// Save clip structures
		mClips[i] = animationDesc.mLayerProperties[i].mClip;
		mClipControllers[i] = animationDesc.mLayerProperties[i].mClipController;
		mClipMasks[i] = animationDesc.mLayerProperties[i].mClipMask;

		// Save additive properties of additive clips
		mClipControllers[i]->SetAdditive(animationDesc.mLayerProperties[i].mAdditive);
		if (mClipControllers[i]->IsAdditive())
			mNumAdditiveClips++;

		// Find the index of the longest clip
		if (mDuration < mClipControllers[i]->GetDuration())
		{
			mDuration = mClipControllers[i]->GetDuration();
			mLongestClipIndex = i;
		}

		// Prepare input and output of clip sampling

		// Allocates sampler runtime buffers.
		mClipLocalTrans[i] = allocator->AllocateRange<SoaTransform>(mRig->GetNumSoaJoints());

		// Allocates a cache that matches animation requirements.
		mClipSamplingCaches[i] = allocator->New<ozz::animation::SamplingCache>(mRig->GetNumJoints());
	}

	// Allocate the blend layers that will be set each sampling based on each clip's properties
	mLayers = allocator->AllocateRange<ozz::animation::BlendingJob::Layer>(mNumClips - mNumAdditiveClips);
	mAdditiveLayers = allocator->AllocateRange<ozz::animation::BlendingJob::Layer>(mNumAdditiveClips);
}

void Animation::Destroy()
{
	ozz::memory::Allocator* allocator = ozz::memory::default_allocator();

	for (unsigned int i = 0; i < mNumClips; i++)
	{
		allocator->Delete(mClipSamplingCaches[i]);
		allocator->Deallocate(mClipLocalTrans[i]);
	}
	allocator->Deallocate(mLayers);
	allocator->Deallocate(mAdditiveLayers);
}

bool Animation::Sample(float dt, ozz::Range<SoaTransform>& localTrans)
{
	//update blend and sample parameters
	if (mAutoSetBlendParams)
	{
		UpdateBlendParameters();
	}

	//sample each of the clips that make up this animation
	for (unsigned int i = 0; i < mNumClips; i++)
	{
		// Updates clips time.
		mClipControllers[i]->Update(dt);

		// Early out if this layers weight makes it irrelevant during blending.
		if (mClipControllers[i]->GetWeight() != 0.f)
		{
			//if (!mClips[i]->Sample(mClipControllers[i]->GetTimeRatio()))
			if (!mClips[i]->Sample(mClipSamplingCaches[i], mClipLocalTrans[i], mClipControllers[i]->GetTimeRatio()))
				return false;
		}
	}

	// Update the animations current time ratio
	mTimeRatio = mClipControllers[mLongestClipIndex]->GetTimeRatio();

	//blend these samples together
	return Blend(localTrans);
}

void Animation::UpdateBlendParameters()
{
	// Set to Ozz's default min value to undo any external changes
	mThreshold = ozz::animation::BlendingJob().threshold;

	// Each animation will have equal influence
	if (mBlendType == BlendType::EQUAL)
	{
		for (unsigned int i = 0; i < mNumClips; i++)
		{
			mClipControllers[i]->SetWeight(1.f / mNumClips);
		}
	}

	// The animations will fade into one another in the order they were added
	// Based on mBlendRatio
	else if (mBlendType == BlendType::CROSS_DISSOLVE)
	{
		// Computes weight parameters for all samplers.
		const float numIntervals = (float)mNumClips - 1;
		const float interval = 1.f / numIntervals;
		for (unsigned int i = 0; i < mNumClips; ++i)
		{
			const float med = i * interval;    // unique order of animation between [0,1]
			const float x = mBlendRatio - med;
			const float y = ((x < 0.f ? x : -x) + interval) * numIntervals;

			mClipControllers[i]->SetWeight(max(0.f, y));
		}
	}

	// The animations will fade into one another in the order they were added, syncronizing their speeds as they fade into eachother
	// Based on mBlendRatio
	else if (mBlendType == BlendType::CROSS_DISSOLVE_SYNC)
	{
		// Computes weight parameters for all samplers.
		const float numIntervals = (float)mNumClips - 1;
		const float interval = 1.f / numIntervals;
		for (unsigned int i = 0; i < mNumClips; ++i)
		{
			const float med = i * interval;    // unique order of animation between [0,1]
			const float x = mBlendRatio - med;
			const float y = ((x < 0.f ? x : -x) + interval) * numIntervals;

			mClipControllers[i]->SetWeight(max(0.f, y));
		}

		// Synchronizes animations.
		// First computes loop cycle duration. Selects the 2 Clips that define
		// interval that contains mBlendRatio.
		// Uses a maximum value smaller that 1.f (-epsilon) to ensure that
		// (relevantClip + 1) is always valid.
		const unsigned int relevantClip = static_cast<unsigned int>((mBlendRatio - 1e-3f) * (mNumClips - 1));
		assert(relevantClip + 1 < mNumClips);
		ClipController* ClipControllerL = mClipControllers[relevantClip];
		ClipController* ClipControllerR = mClipControllers[relevantClip + 1];

		// Interpolates animation durations using their respective weights, to
		// find the loop cycle duration that matches blend_ratio_.
		const float loopDuration =

			ClipControllerL->GetDuration() * ClipControllerL->GetWeight() + ClipControllerR->GetDuration() * ClipControllerR->GetWeight();

		// Finally finds the speed coefficient for all Clips.
		const float invLoopDuration = 1.f / loopDuration;
		for (unsigned int i = 0; i < mNumClips; ++i)
		{
			ClipController* ClipController = mClipControllers[i];
			const float     speed = ClipController->GetDuration() * invLoopDuration;
			ClipController->SetPlaybackSpeed(speed);
		}
	}
}

bool Animation::Blend(ozz::Range<SoaTransform>& localTrans)
{
	unsigned int additiveIndex = 0;
	for (unsigned int i = 0; i < mNumClips; i++)
	{
		if (mClipControllers[i]->IsAdditive())
		{
			mAdditiveLayers[additiveIndex].transform = mClipLocalTrans[i];
			mAdditiveLayers[additiveIndex].weight = mClipControllers[i]->GetWeight();

			if (mClipMasks[i])
				mAdditiveLayers[additiveIndex].joint_weights = mClipMasks[i]->GetJointWeights();
			else
				mAdditiveLayers[additiveIndex].joint_weights = ozz::Range<const Vector4>();

			additiveIndex++;
		}
		else
		{
			mLayers[i].transform = mClipLocalTrans[i];
			mLayers[i].weight = mClipControllers[i]->GetWeight();

			if (mClipMasks[i])
				mLayers[i].joint_weights = mClipMasks[i]->GetJointWeights();
			else
				mLayers[i].joint_weights = ozz::Range<const Vector4>();
		}
	}

	// Setups blending job.
	ozz::animation::BlendingJob blendJob;
	blendJob.threshold = mThreshold;
	blendJob.layers = mLayers;
	if (mNumAdditiveClips > 0)
		blendJob.additive_layers = mAdditiveLayers;
	blendJob.bind_pose = mRig->GetSkeleton()->bind_pose();
	blendJob.output = localTrans;

	// Blends.
	if (!blendJob.Run())
	{
		return false;
	}

	return true;
}

void Animation::SetTimeRatio(float timeRatio)
{
	float time = timeRatio * mDuration;

	for (unsigned int i = 0; i < mNumClips; i++)
	{
		mClipControllers[i]->SetTimeRatio(time);
	}
}