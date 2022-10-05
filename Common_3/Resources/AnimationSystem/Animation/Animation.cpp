/*
 * Copyright (c) 2017-2022 The Forge Interactive Inc.
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
	mBlendType = animationDesc.mBlendType;
	mNumClips = min(animationDesc.mNumLayers, MAX_NUM_CLIPS);

	mNumAdditiveClips = 0;

	ozz::memory::Allocator* allocator = ozz::memory::default_allocator();

	for (uint32_t i = 0; i < mNumClips; i++)
	{
		// Save clip structures
		mClips[i] = animationDesc.mLayerProperties[i].mClip;
		mClipControllers[i] = animationDesc.mLayerProperties[i].mClipController;
		mClipMasks[i] = animationDesc.mLayerProperties[i].mClipMask;

		// Save additive properties of additive clips
		mClipControllers[i]->mAdditive = animationDesc.mLayerProperties[i].mAdditive;
		if (mClipControllers[i]->mAdditive)
			mNumAdditiveClips++;

		// Find the index of the longest clip
		if (mDuration < mClipControllers[i]->mDuration)
		{
			mDuration = mClipControllers[i]->mDuration;
			mLongestClipIndex = i;
		}

		// Prepare input and output of clip sampling

		// Allocates sampler runtime buffers.
		mClipLocalTrans[i] = allocator->AllocateRange<SoaTransform>(mRig->mNumSoaJoints);

		// Allocates a cache that matches animation requirements.
		mClipSamplingCaches[i] = ozz::New<ozz::animation::SamplingJob::Context>(mRig->mNumJoints);
	}

	// Allocate the blend layers that will be set each sampling based on each clip's properties
	mLayers = allocator->AllocateRange<ozz::animation::BlendingJob::Layer>(mNumClips - mNumAdditiveClips);
	mAdditiveLayers = allocator->AllocateRange<ozz::animation::BlendingJob::Layer>(mNumAdditiveClips);
}

void Animation::Exit()
{
	ozz::memory::Allocator* allocator = ozz::memory::default_allocator();

	for (uint32_t i = 0; i < mNumClips; i++)
	{
		ozz::Delete(mClipSamplingCaches[i]);
		allocator->Deallocate(mClipLocalTrans[i]);
	}
	allocator->Deallocate(mLayers);
	allocator->Deallocate(mAdditiveLayers);
}

bool Animation::Sample(float dt, ozz::span<SoaTransform>& localTrans)
{
	//update blend and sample parameters
	if (mAutoSetBlendParams)
	{
		UpdateBlendParameters();
	}

	//sample each of the clips that make up this animation
	for (uint32_t i = 0; i < mNumClips; i++)
	{
		// Updates clips time.
		mClipControllers[i]->Update(dt);

		// Early out if this layers weight makes it irrelevant during blending.
		if (mClipControllers[i]->mWeight != 0.f)
		{
			//if (!mClips[i]->Sample(mClipControllers[i]->mTimeRatio))
			if (!mClips[i]->Sample(mClipSamplingCaches[i], mClipLocalTrans[i], mClipControllers[i]->mTimeRatio))
				return false;
		}
	}

	// Update the animations current time ratio
	mTimeRatio = mClipControllers[mLongestClipIndex]->mTimeRatio;

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
		for (uint32_t i = 0; i < mNumClips; i++)
		{
			mClipControllers[i]->mWeight = 1.f / mNumClips;
		}
	}

	// The animations will fade into one another in the order they were added
	// Based on mBlendRatio
	else if (mBlendType == BlendType::CROSS_DISSOLVE)
	{
		// Computes weight parameters for all samplers.
		const float numIntervals = (float)mNumClips - 1;
		const float interval = 1.f / numIntervals;
		for (uint32_t i = 0; i < mNumClips; ++i)
		{
			const float med = i * interval;    // unique order of animation between [0,1]
			const float x = mBlendRatio - med;
			const float y = ((x < 0.f ? x : -x) + interval) * numIntervals;

			mClipControllers[i]->mWeight = max(0.f, y);
		}
	}

	// The animations will fade into one another in the order they were added, syncronizing their speeds as they fade into eachother
	// Based on mBlendRatio
	else if (mBlendType == BlendType::CROSS_DISSOLVE_SYNC)
	{
		// Computes weight parameters for all samplers.
		const float numIntervals = (float)mNumClips - 1;
		const float interval = 1.f / numIntervals;
		for (uint32_t i = 0; i < mNumClips; ++i)
		{
			const float med = i * interval;    // unique order of animation between [0,1]
			const float x = mBlendRatio - med;
			const float y = ((x < 0.f ? x : -x) + interval) * numIntervals;

			mClipControllers[i]->mWeight = max(0.f, y);
		}

		// Synchronizes animations.
		// First computes loop cycle duration. Selects the 2 Clips that define
		// interval that contains mBlendRatio.
		// Uses a maximum value smaller that 1.f (-epsilon) to ensure that
		// (relevantClip + 1) is always valid.
		const uint32_t relevantClip = static_cast<uint32_t>((mBlendRatio - 1e-3f) * (mNumClips - 1));
		ASSERT(relevantClip + 1 < mNumClips);
		ClipController* ClipControllerL = mClipControllers[relevantClip];
		ClipController* ClipControllerR = mClipControllers[relevantClip + 1];

		// Interpolates animation durations using their respective weights, to
		// find the loop cycle duration that matches blend_ratio_.
		const float loopDuration =

			ClipControllerL->mDuration * ClipControllerL->mWeight + ClipControllerR->mDuration * ClipControllerR->mWeight;

		// Finally finds the speed coefficient for all Clips.
		const float invLoopDuration = 1.f / loopDuration;
		for (uint32_t i = 0; i < mNumClips; ++i)
		{
			ClipController* ClipController = mClipControllers[i];
			const float     speed = ClipController->mDuration * invLoopDuration;
			ClipController->mPlaybackSpeed = speed;
		}
	}
}

bool Animation::Blend(ozz::span<SoaTransform>& localTrans)
{
	uint32_t additiveIndex = 0;
	for (uint32_t i = 0; i < mNumClips; i++)
	{
		if (mClipControllers[i]->mAdditive)
		{
			mAdditiveLayers[additiveIndex].transform = mClipLocalTrans[i];
			mAdditiveLayers[additiveIndex].weight = mClipControllers[i]->mWeight;

			if (mClipMasks[i])
				mAdditiveLayers[additiveIndex].joint_weights = mClipMasks[i]->GetJointWeights();
			else
				mAdditiveLayers[additiveIndex].joint_weights = ozz::span<const Vector4>();

			additiveIndex++;
		}
		else
		{
			mLayers[i].transform = mClipLocalTrans[i];
			mLayers[i].weight = mClipControllers[i]->mWeight;

			if (mClipMasks[i])
				mLayers[i].joint_weights = mClipMasks[i]->GetJointWeights();
			else
				mLayers[i].joint_weights = ozz::span<const Vector4>();
		}
	}

	// Setups blending job.
	ozz::animation::BlendingJob blendJob;
	blendJob.threshold = mThreshold;
	blendJob.layers = mLayers;
	if (mNumAdditiveClips > 0)
		blendJob.additive_layers = mAdditiveLayers;
	blendJob.rest_pose = mRig->mSkeleton.joint_rest_poses();
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

	for (uint32_t i = 0; i < mNumClips; i++)
	{
		mClipControllers[i]->SetTimeRatio(time);
	}
}