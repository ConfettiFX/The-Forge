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

void SkeletonBatcher::Initialize(const SkeletonRenderDesc& skeletonRenderDesc)
{
	// Set member render variables based on the description
	mRenderer = skeletonRenderDesc.mRenderer;
	mSkeletonPipeline = skeletonRenderDesc.mSkeletonPipeline;
	mRootSignature = skeletonRenderDesc.mRootSignature;
	mJointVertexBuffer = skeletonRenderDesc.mJointVertexBuffer;
	mNumJointPoints = skeletonRenderDesc.mNumJointPoints;
	mJointVertexStride = skeletonRenderDesc.mJointVertexStride;
	mBoneVertexStride = skeletonRenderDesc.mBoneVertexStride;

	// 2 because updates buffer twice per instanced draw call: one for joints and one for bones
	DescriptorSetDesc setDesc = { mRootSignature, DESCRIPTOR_UPDATE_FREQ_PER_DRAW, MAX_BATCHES * 2 * ImageCount };
	addDescriptorSet(mRenderer, &setDesc, &pDescriptorSet);

	// Determine if we will ever expect to use this renderer to draw bones
	mDrawBones = skeletonRenderDesc.mDrawBones;
	if (mDrawBones)
	{
		mBoneVertexBuffer = skeletonRenderDesc.mBoneVertexBuffer;
		mNumBonePoints = skeletonRenderDesc.mNumBonePoints;
	}

	mNumRigs = 0;
	mNumActiveRigs = 0; 
	mInstanceCount = 0;

	// Initialize all the buffer that will be used for each batch per each frame index
	BufferLoadDesc ubDesc = {};
	ubDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	ubDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
	ubDesc.mDesc.mSize = sizeof(UniformSkeletonBlock);
	ubDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT | skeletonRenderDesc.mCreationFlag;
	ubDesc.pData = NULL;

	DescriptorData params[1] = {};
	params[0].pName = "uniformBlock";

	for (uint32_t i = 0; i < ImageCount; ++i)
	{
		mBatchCounts[i] = 0;

		for (uint32_t j = 0; j < MAX_BATCHES; ++j)
		{
			ubDesc.ppBuffer = &mProjViewUniformBufferJoints[i][j];
			addResource(&ubDesc, NULL);

			params[0].ppBuffers = &mProjViewUniformBufferJoints[i][j];
			updateDescriptorSet(mRenderer, (i * (MAX_BATCHES * 2)) + (j * 2 + 0), pDescriptorSet, 1, params);

			if (mDrawBones)
			{
				ubDesc.ppBuffer = &mProjViewUniformBufferBones[i][j];
				addResource(&ubDesc, NULL);

				params[0].ppBuffers = &mProjViewUniformBufferBones[i][j];
				updateDescriptorSet(mRenderer, (i * (MAX_BATCHES * 2)) + (j * 2 + 1), pDescriptorSet, 1, params);
			}
		}
	}
}

void SkeletonBatcher::Exit()
{
	removeDescriptorSet(mRenderer, pDescriptorSet);
	for (uint32_t i = 0; i < ImageCount; ++i)
	{
		for (uint32_t j = 0; j < MAX_BATCHES; ++j)
		{
			removeResource(mProjViewUniformBufferJoints[i][j]);
			if (mDrawBones)
			{
				removeResource(mProjViewUniformBufferBones[i][j]);
			}
		}
	}
}

void SkeletonBatcher::SetSharedUniforms(const CameraMatrix& projViewMat, const Vector3& lightPos, const Vector3& lightColor)
{
	for (uint32_t i = 0; i < MAX_BATCHES; ++i)
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
}

void SkeletonBatcher::SetPerInstanceUniforms(const uint32_t& frameIndex, int numRigs, uint32_t rigsOffset)
{
	// If the numRigs parameter was not initialized, used the data from all the active rigs
	if (numRigs == -1)
	{
		numRigs = mNumActiveRigs;
	}

	const unsigned int lastBatchIndex = mCumulativeRigInstanceCount[mNumActiveRigs] / MAX_INSTANCES;
	const unsigned int lastBatchSize = mCumulativeRigInstanceCount[mNumActiveRigs] % MAX_INSTANCES;

	// Will keep track of the number of instances that have their data added
	const unsigned int totalInstanceCount = mCumulativeRigInstanceCount[rigsOffset + numRigs] - mCumulativeRigInstanceCount[rigsOffset];
	unsigned int instanceCount = tfrg_atomic32_add_relaxed(&mInstanceCount, totalInstanceCount);

	// Last resets mInstanceCount
	if (instanceCount + totalInstanceCount == mCumulativeRigInstanceCount[mNumActiveRigs])
		mInstanceCount = 0;

	unsigned int batchInstanceCount = 0;
	unsigned int batchIndex = instanceCount / MAX_INSTANCES;

	// For every rig
	for (uint32_t rigIndex = rigsOffset; rigIndex < numRigs + rigsOffset; ++rigIndex)
	{
		// Get the number of joints in the rig
		unsigned int numJoints = mRigs[rigIndex]->GetNumJoints();

		// For every joint in the rig
		for (unsigned int jointIndex = 0; jointIndex < numJoints; jointIndex++)
		{
			unsigned int instanceIndex = instanceCount % MAX_INSTANCES;
			UniformSkeletonBlock& uniformDataJoints = mUniformDataJoints[batchIndex];
			UniformSkeletonBlock& uniformDataBones = mUniformDataBones[batchIndex];

			if (mDrawBones)
			{
				// add bones data to the uniform
				uniformDataBones.mToWorldMat[instanceIndex] = mRigs[rigIndex]->GetBoneWorldMat(jointIndex);
				uniformDataBones.mColor[instanceIndex] = mRigs[rigIndex]->GetBoneColor();

				// add joint data to the uniform while scaling the joints by their determined chlid bone length
				uniformDataJoints.mToWorldMat[instanceIndex] =
					mRigs[rigIndex]->GetJointWorldMatNoScale(jointIndex) * mat4::scale(mRigs[rigIndex]->GetJointScale(jointIndex));
			}
			else
			{
				// add joint data to the uniform without scaling
				uniformDataJoints.mToWorldMat[instanceIndex] = mRigs[rigIndex]->GetJointWorldMatNoScale(jointIndex);
			}
			uniformDataJoints.mColor[instanceIndex] = mRigs[rigIndex]->GetJointColor();

			// increment the count of uniform data that has been filled for this batch
			++instanceCount;
			++batchInstanceCount;

			// If we have reached our maximun amount of instances, or the end of our data
			if ((instanceIndex == MAX_INSTANCES -1) || ((rigIndex - rigsOffset == (uint32_t)(numRigs - 1)) && (jointIndex == numJoints - 1)))
			{

				// Finalize the data for this batch by adding the batch instance to the batch total size
				unsigned int currBatchSize = tfrg_atomic32_add_relaxed(&mBatchSize[frameIndex][batchIndex], batchInstanceCount) + batchInstanceCount;
				
				// Only update if batch is full, or this is the last batch as it could be less than MAX_INSTANCES
				if(currBatchSize == MAX_INSTANCES ||
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
}

void SkeletonBatcher::AddRig(Rig* rig)
{
	// Adds the rig so its data can be used and increments the rig count
	ASSERT(mNumRigs < MAX_RIGS && "Exceed maximum amount of rigs");
	mRigs[mNumRigs] = rig;
	uint32_t joints = rig->GetNumJoints() + mCumulativeRigInstanceCount[mNumRigs];
	ASSERT(joints < MAX_INSTANCES * MAX_BATCHES && "Exceed maximum amount of instances");
	++mNumRigs;
	++mNumActiveRigs;
	mCumulativeRigInstanceCount[mNumRigs] = joints;
}

void SkeletonBatcher::Draw(Cmd* cmd, const uint32_t& frameIndex)
{
	// Get the number of batches to draw for this frameindex
	unsigned int numBatches = tfrg_atomic32_store_relaxed(&mBatchCounts[frameIndex], 0);

	cmdBindPipeline(cmd, mSkeletonPipeline);

	// Joints
	cmdBeginDebugMarker(cmd, 1, 0, 1, "Draw Skeletons Joints");
	cmdBindVertexBuffer(cmd, 1, &mJointVertexBuffer, &mJointVertexStride, NULL);

	// for each batch of joints
	for (unsigned int batchIndex = 0; batchIndex < numBatches; batchIndex++)
	{
		cmdBindDescriptorSet(cmd, (frameIndex * (MAX_BATCHES * 2)) + (batchIndex * 2 + 0), pDescriptorSet);
		cmdDrawInstanced(cmd, mNumJointPoints / 6, 0, mBatchSize[frameIndex][batchIndex], 0);
		if(!mDrawBones) mBatchSize[frameIndex][batchIndex] = 0;
	}
	cmdEndDebugMarker(cmd);

	// Bones
	if (mDrawBones)
	{
		cmdBindVertexBuffer(cmd, 1, &mBoneVertexBuffer, &mBoneVertexStride, NULL);
		cmdBeginDebugMarker(cmd, 1, 0, 1, "Draw Skeletons Bones");

		// for each batch of bones
		for (unsigned int batchIndex = 0; batchIndex < numBatches; batchIndex++)
		{
			cmdBindDescriptorSet(cmd, (frameIndex * (MAX_BATCHES * 2)) + (batchIndex * 2 + 1), pDescriptorSet);
			cmdDrawInstanced(cmd, mNumBonePoints / 6, 0, mBatchSize[frameIndex][batchIndex], 0);
			mBatchSize[frameIndex][batchIndex] = 0;
		}
		cmdEndDebugMarker(cmd);
	}
}