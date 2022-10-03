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

#include "SkeletonBatcher.h"

#include "../../../Utilities/ThirdParty/OpenSource/Nothings/stb_ds.h"

void SkeletonBatcher::Initialize(const SkeletonRenderDesc& skeletonRenderDesc)
{
#ifdef ENABLE_FORGE_ANIMATION_DEBUG
	// Set member render variables based on the description
	mRenderer = skeletonRenderDesc.mRenderer;
	mSkeletonPipeline = skeletonRenderDesc.mSkeletonPipeline;
	mJointVertexBuffer = skeletonRenderDesc.mJointVertexBuffer;
	mNumJointPoints = skeletonRenderDesc.mNumJointPoints;
	mJointVertexStride = skeletonRenderDesc.mJointVertexStride;
	mBoneVertexStride = skeletonRenderDesc.mBoneVertexStride;

	ASSERT(skeletonRenderDesc.mMaxAnimatedObjects > 0 && "Need to specify the maximum number of animated objects");
	mMaxAnimatedObjects = skeletonRenderDesc.mMaxAnimatedObjects;
	arrsetlen(mAnimatedObjects, skeletonRenderDesc.mMaxAnimatedObjects);
	arrsetlen(mCumulativeAnimatedObjectInstanceCount, skeletonRenderDesc.mMaxAnimatedObjects + 1);
	memset(mAnimatedObjects, 0, sizeof(*mAnimatedObjects) * skeletonRenderDesc.mMaxAnimatedObjects);
	memset(mCumulativeAnimatedObjectInstanceCount, 0, sizeof(*mCumulativeAnimatedObjectInstanceCount) * (skeletonRenderDesc.mMaxAnimatedObjects + 1));

	// Determine if we will ever expect to use this renderer to draw bones
	mDrawBones = skeletonRenderDesc.mDrawBones;
	if (mDrawBones)
	{
		mBoneVertexBuffer = skeletonRenderDesc.mBoneVertexBuffer;
		mNumBonePoints = skeletonRenderDesc.mNumBonePoints;
	}

	mNumAnimatedObjects = 0;
	mNumActiveAnimatedObjects = 0;
	mInstanceCount = 0;

	// Initialize all the buffer that will be used for each batch per each frame index
	BufferLoadDesc ubDesc = {};
	ubDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	ubDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
	ubDesc.mDesc.mSize = sizeof(UniformSkeletonBlock);
	ubDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT | skeletonRenderDesc.mCreationFlag;
	ubDesc.pData = NULL;

	for (uint32_t i = 0; i < ImageCount; ++i)
	{
		mBatchCounts[i] = 0;

		for (uint32_t j = 0; j < MAX_SKELETON_BATCHES; ++j)
		{
            mBatchSize[i][j] = 0;

			ubDesc.ppBuffer = &mProjViewUniformBufferJoints[i][j];
			addResource(&ubDesc, NULL);

			if (mDrawBones)
			{
				ubDesc.ppBuffer = &mProjViewUniformBufferBones[i][j];
				addResource(&ubDesc, NULL);
			}
		}
	}
#endif
}

void SkeletonBatcher::Exit()
{
#ifdef ENABLE_FORGE_ANIMATION_DEBUG
	for (uint32_t i = 0; i < ImageCount; ++i)
	{
		for (uint32_t j = 0; j < MAX_SKELETON_BATCHES; ++j)
		{
			removeResource(mProjViewUniformBufferJoints[i][j]);
			if (mDrawBones)
			{
				removeResource(mProjViewUniformBufferBones[i][j]);
			}
		}
	}

	arrfree(mAnimatedObjects);
	arrfree(mCumulativeAnimatedObjectInstanceCount);
#endif
}

void SkeletonBatcher::LoadPipeline(Pipeline* pipeline)
{
#ifdef ENABLE_FORGE_ANIMATION_DEBUG
	mSkeletonPipeline = pipeline;
#endif
}

void SkeletonBatcher::AddDescriptorSets(RootSignature* pRootSignature)
{
#ifdef ENABLE_FORGE_ANIMATION_DEBUG
	// 2 because updates buffer twice per instanced draw call: one for joints and one for bones
	DescriptorSetDesc setDesc = { pRootSignature, DESCRIPTOR_UPDATE_FREQ_PER_DRAW, MAX_SKELETON_BATCHES * 2 * ImageCount };
	addDescriptorSet(mRenderer, &setDesc, &pDescriptorSet);
#endif
}

void SkeletonBatcher::RemoveDescriptorSets()
{
#ifdef ENABLE_FORGE_ANIMATION_DEBUG
	removeDescriptorSet(mRenderer, pDescriptorSet);
#endif
}

void SkeletonBatcher::PrepareDescriptorSets()
{
#ifdef ENABLE_FORGE_ANIMATION_DEBUG
	DescriptorData params[1] = {};
	params[0].pName = "uniformBlock";

	for (uint32_t i = 0; i < ImageCount; ++i)
	{
		for (uint32_t j = 0; j < MAX_SKELETON_BATCHES; ++j)
		{
			params[0].ppBuffers = &mProjViewUniformBufferJoints[i][j];
			updateDescriptorSet(mRenderer, (i * (MAX_SKELETON_BATCHES * 2)) + (j * 2 + 0), pDescriptorSet, 1, params);

			if (mDrawBones)
			{
				params[0].ppBuffers = &mProjViewUniformBufferBones[i][j];
				updateDescriptorSet(mRenderer, (i * (MAX_SKELETON_BATCHES * 2)) + (j * 2 + 1), pDescriptorSet, 1, params);
			}
		}
	}
#endif
}

void SkeletonBatcher::SetSharedUniforms(const CameraMatrix& projViewMat, const Vector3& lightPos, const Vector3& lightColor)
{
#ifdef ENABLE_FORGE_ANIMATION_DEBUG
	for (uint32_t i = 0; i < MAX_SKELETON_BATCHES; ++i)
	{
		mUniformDataJoints[i].mProjectView = projViewMat;
		mUniformDataJoints[i].mLightPosition = Vector4(lightPos);
		mUniformDataJoints[i].mLightColor = Vector4(lightColor);

		if (mDrawBones)
		{
			mUniformDataBones[i].mProjectView = projViewMat;
			mUniformDataBones[i].mLightPosition = Vector4(lightPos);
			mUniformDataBones[i].mLightColor = Vector4(lightColor);
		}
	}
#endif
}

void SkeletonBatcher::SetActiveRigs(uint32_t activeRigs)
{
#ifdef ENABLE_FORGE_ANIMATION_DEBUG
	mNumActiveAnimatedObjects = min(activeRigs, mNumAnimatedObjects);
#endif
}

void SkeletonBatcher::SetPerInstanceUniforms(const uint32_t frameIndex, int32_t numRigs, uint32_t rigsOffset)
{
#ifdef ENABLE_FORGE_ANIMATION_DEBUG
	// If the numRigs parameter was not initialized, used the data from all the active rigs
	if (numRigs == -1)
	{
		numRigs = mNumActiveAnimatedObjects;
	}

	const uint32_t lastBatchIndex = mCumulativeAnimatedObjectInstanceCount[mNumActiveAnimatedObjects] / MAX_SKELETON_BLOCK_INSTANCES;
	const uint32_t lastBatchSize = mCumulativeAnimatedObjectInstanceCount[mNumActiveAnimatedObjects] % MAX_SKELETON_BLOCK_INSTANCES;

	// Will keep track of the number of instances that have their data added
	const uint32_t totalInstanceCount = mCumulativeAnimatedObjectInstanceCount[rigsOffset + numRigs] - mCumulativeAnimatedObjectInstanceCount[rigsOffset];
	uint32_t instanceCount = tfrg_atomic32_add_relaxed(&mInstanceCount, totalInstanceCount);

	// Last resets mInstanceCount
	if (instanceCount + totalInstanceCount == mCumulativeAnimatedObjectInstanceCount[mNumActiveAnimatedObjects])
		mInstanceCount = 0;

	uint32_t batchInstanceCount = 0;
	uint32_t batchIndex = instanceCount / MAX_SKELETON_BLOCK_INSTANCES;

	// Reset batch counts
	tfrg_atomic32_store_relaxed(&mBatchCounts[frameIndex], 0);
	for (uint32_t i = 0; i < MAX_SKELETON_BATCHES; ++i)
	{
		tfrg_atomic32_store_relaxed(&mBatchSize[frameIndex][i], 0);
	}

	// For every rig
	for (uint32_t objIndex = rigsOffset; objIndex < numRigs + rigsOffset; ++objIndex)
	{
		const AnimatedObject* animObj = mAnimatedObjects[objIndex];

		// Get the number of joints in the rig
		uint32_t numJoints = animObj->mRig->mNumJoints;

		// For every joint in the rig
		for (uint32_t jointIndex = 0; jointIndex < numJoints; jointIndex++)
		{
			uint32_t instanceIndex = instanceCount % MAX_SKELETON_BLOCK_INSTANCES;
			UniformSkeletonBlock& uniformDataJoints = mUniformDataJoints[batchIndex];
			UniformSkeletonBlock& uniformDataBones = mUniformDataBones[batchIndex];

			if (mDrawBones)
			{
				// add bones data to the uniform
				uniformDataBones.mToWorldMat[instanceIndex] = animObj->mBoneWorldMats[jointIndex];
				uniformDataBones.mColor[instanceIndex] = animObj->mBoneColor;

				// add joint data to the uniform while scaling the joints by their determined chlid bone length
				uniformDataJoints.mToWorldMat[instanceIndex] =
					animObj->GetJointWorldMatNoScale(jointIndex) * mat4::scale(animObj->mJointScales[jointIndex]);
			}
			else
			{
				// add joint data to the uniform without scaling
				uniformDataJoints.mToWorldMat[instanceIndex] = animObj->GetJointWorldMatNoScale(jointIndex);
			}
			uniformDataJoints.mColor[instanceIndex] = animObj->mJointColor;

			// increment the count of uniform data that has been filled for this batch
			++instanceCount;
			++batchInstanceCount;

			// If we have reached our maximun amount of instances, or the end of our data
			if ((instanceIndex == MAX_SKELETON_BLOCK_INSTANCES - 1) || ((objIndex - rigsOffset == (uint32_t)(numRigs - 1)) && (jointIndex == numJoints - 1)))
			{

				// Finalize the data for this batch by adding the batch instance to the batch total size
				uint32_t currBatchSize = tfrg_atomic32_add_relaxed(&mBatchSize[frameIndex][batchIndex], batchInstanceCount) + batchInstanceCount;

				// Only update if batch is full, or this is the last batch as it could be less than MAX_SKELETON_BLOCK_INSTANCES
				if (currBatchSize == MAX_SKELETON_BLOCK_INSTANCES ||
					(lastBatchIndex == batchIndex && currBatchSize == lastBatchSize))
				{
					tfrg_atomic32_add_relaxed(&mBatchCounts[frameIndex], 1);
					BufferUpdateDesc viewProjCbvJoints = { mProjViewUniformBufferJoints[frameIndex][batchIndex] };
					beginUpdateResource(&viewProjCbvJoints);
					memcpy(viewProjCbvJoints.pMappedData, &uniformDataJoints, sizeof(uniformDataJoints));
					endUpdateResource(&viewProjCbvJoints, NULL);

					if (mDrawBones)
					{
						BufferUpdateDesc viewProjCbvBones = { mProjViewUniformBufferBones[frameIndex][batchIndex] };
						beginUpdateResource(&viewProjCbvBones);
						memcpy(viewProjCbvBones.pMappedData, &uniformDataBones, sizeof(uniformDataBones));
						endUpdateResource(&viewProjCbvBones, NULL);
					}
				}

				// Increase batchIndex for next batch
				++batchIndex;
				// Reset the count so it can be used for the next batch
				batchInstanceCount = 0;
			}
		}
	}
#endif
}

void SkeletonBatcher::AddAnimatedObject(AnimatedObject* animatedObject)
{
#ifdef ENABLE_FORGE_ANIMATION_DEBUG
	ASSERT(animatedObject && animatedObject->mRig);
	for (uint32_t i = 0; i < mNumAnimatedObjects; ++i)
	{
		ASSERT(animatedObject != mAnimatedObjects[i] && "Trying to add duplicated animated object");
	}

	// Adds the rig so its data can be used and increments the rig count
	ASSERT(mNumAnimatedObjects < (uint32_t)arrlen(mAnimatedObjects) && "Exceed maximum amount of rigs");
	mAnimatedObjects[mNumAnimatedObjects] = animatedObject;
	uint32_t joints = animatedObject->mRig->mNumJoints + mCumulativeAnimatedObjectInstanceCount[mNumAnimatedObjects];
	ASSERT(joints < MAX_SKELETON_BLOCK_INSTANCES* MAX_SKELETON_BATCHES && "Exceed maximum amount of instances");
	++mNumAnimatedObjects;
	++mNumActiveAnimatedObjects;
	mCumulativeAnimatedObjectInstanceCount[mNumAnimatedObjects] = joints;
#endif
}

void SkeletonBatcher::RemoveAnimatedObject(AnimatedObject* animatedObject)
{
#ifdef ENABLE_FORGE_ANIMATION_DEBUG
	ASSERT(animatedObject);

	uint32_t start = mNumAnimatedObjects;
	for (uint32_t i = 0; i < mNumAnimatedObjects; ++i)
	{
		if (animatedObject == mAnimatedObjects[i])
		{
			start = i;
			break;
		}
	}

	if (start < mNumAnimatedObjects)
	{
		ASSERT(mAnimatedObjects[start] == animatedObject);
		for (uint32_t i = start; i < mNumAnimatedObjects - 1; ++i)
		{
			ASSERT(mAnimatedObjects[i + 1] != animatedObject && "Animated object was added twice");
			mAnimatedObjects[i] = mAnimatedObjects[i + 1];

			ASSERT(mAnimatedObjects[i]->mRig);

			const uint32_t cumulativeJoints = mAnimatedObjects[i]->mRig->mNumJoints + mCumulativeAnimatedObjectInstanceCount[i];
			mCumulativeAnimatedObjectInstanceCount[i + 1] = cumulativeJoints;
			ASSERT(cumulativeJoints < MAX_SKELETON_BLOCK_INSTANCES* MAX_SKELETON_BATCHES && "Exceed maximum amount of instances");
		}

		mNumAnimatedObjects--;
		if (mNumActiveAnimatedObjects > start)
			mNumActiveAnimatedObjects--;

		ASSERT(mNumActiveAnimatedObjects <= mNumAnimatedObjects);
	}

#endif
}

void SkeletonBatcher::RemoveAllAnimatedObjects()
{
#ifdef ENABLE_FORGE_ANIMATION_DEBUG
	memset(mAnimatedObjects, 0, sizeof(AnimatedObject*) * mNumAnimatedObjects);
	memset(mCumulativeAnimatedObjectInstanceCount, 0, sizeof(mCumulativeAnimatedObjectInstanceCount[0]) * mNumAnimatedObjects);
	mNumAnimatedObjects = 0;
	mNumActiveAnimatedObjects = 0;
#endif
}

void SkeletonBatcher::Draw(Cmd* cmd, const uint32_t frameIndex)
{
#ifdef ENABLE_FORGE_ANIMATION_DEBUG
	// Get the number of batches to draw for this frameindex
	uint32_t numBatches = tfrg_atomic32_store_relaxed(&mBatchCounts[frameIndex], 0);

	cmdBindPipeline(cmd, mSkeletonPipeline);

	// Joints
	cmdBeginDebugMarker(cmd, 1, 0, 1, "Draw Skeletons Joints");
	cmdBindVertexBuffer(cmd, 1, &mJointVertexBuffer, &mJointVertexStride, NULL);

	// for each batch of joints
	for (uint32_t batchIndex = 0; batchIndex < numBatches; batchIndex++)
	{
		cmdBindDescriptorSet(cmd, (frameIndex * (MAX_SKELETON_BATCHES * 2)) + (batchIndex * 2 + 0), pDescriptorSet);
		cmdDrawInstanced(cmd, mNumJointPoints / 6, 0, mBatchSize[frameIndex][batchIndex], 0);
		if (!mDrawBones) mBatchSize[frameIndex][batchIndex] = 0;
	}
	cmdEndDebugMarker(cmd);

	// Bones
	if (mDrawBones)
	{
		cmdBindVertexBuffer(cmd, 1, &mBoneVertexBuffer, &mBoneVertexStride, NULL);
		cmdBeginDebugMarker(cmd, 1, 0, 1, "Draw Skeletons Bones");

		// for each batch of bones
		for (uint32_t batchIndex = 0; batchIndex < numBatches; batchIndex++)
		{
			cmdBindDescriptorSet(cmd, (frameIndex * (MAX_SKELETON_BATCHES * 2)) + (batchIndex * 2 + 1), pDescriptorSet);
			cmdDrawInstanced(cmd, mNumBonePoints / 6, 0, mBatchSize[frameIndex][batchIndex], 0);
			mBatchSize[frameIndex][batchIndex] = 0;
		}
		cmdEndDebugMarker(cmd);
	}
#endif
}