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

#include "SkeletonBatcher.h"

void SkeletonBatcher::Initialize(const SkeletonRenderDesc& skeletonRenderDesc)
{
	// Set member render variables based on the description
	mSkeletonPipeline = skeletonRenderDesc.mSkeletonPipeline;
	mRootSignature = skeletonRenderDesc.mRootSignature;
	mJointVertexBuffer = skeletonRenderDesc.mJointVertexBuffer;
	mNumJointPoints = skeletonRenderDesc.mNumJointPoints;

	// Determine if we will ever expect to use this renderer to draw bones
	mDrawBones = skeletonRenderDesc.mDrawBones;
	if (mDrawBones)
	{
		mBoneVertexBuffer = skeletonRenderDesc.mBoneVertexBuffer;
		mNumBonePoints = skeletonRenderDesc.mNumBonePoints;
	}

	// Initialize all the buffer that will be used for each batch per each frame index
	BufferLoadDesc ubDesc = {};
	ubDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	ubDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
	ubDesc.mDesc.mSize = sizeof(UniformSkeletonBlock);
	ubDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
	ubDesc.pData = NULL;
	for (unsigned int i = 0; i < MAX_BATCHES; i++)
	{
		for (uint32_t j = 0; j < ImageCount; ++j)
		{
			ubDesc.ppBuffer = &mProjViewUniformBufferJoints[i][j];
			addResource(&ubDesc);
			if (mDrawBones)
			{
				ubDesc.ppBuffer = &mProjViewUniformBufferBones[i][j];
				addResource(&ubDesc);
			}
		}
	}
}

void SkeletonBatcher::Destroy()
{
	for (unsigned int i = 0; i < MAX_BATCHES; i++)
	{
		for (uint32_t j = 0; j < ImageCount; ++j)
		{
			removeResource(mProjViewUniformBufferJoints[i][j]);
			if (mDrawBones)
			{
				removeResource(mProjViewUniformBufferBones[i][j]);
			}
		}
	}
}

void SkeletonBatcher::SetSharedUniforms(const Matrix4& projViewMat, const Vector3& lightPos, const Vector3& lightColor)
{
	mUniformDataJoints.mProjectView = projViewMat;
	mUniformDataJoints.mLightPosition = lightPos;
	mUniformDataJoints.mLightColor = lightColor;

	if (mDrawBones)
	{
		mUniformDataBones.mProjectView = projViewMat;
		mUniformDataBones.mLightPosition = lightPos;
		mUniformDataBones.mLightColor = lightColor;
	}
}

void SkeletonBatcher::SetPerInstanceUniforms(const uint32_t& frameIndex, int numRigs)
{
	// Will keep track of the current batch we are setting the uniforms for
	// and will indicate how many catches to draw when draw is called for this frame index
	mBatchCounts[frameIndex] = 0;

	// Will keep track of the number of instances that have their data added
	unsigned int instanceCount = 0;

	// If the numRigs parameter was not initialized, used the data from all the rigs
	if (numRigs == -1)
	{
		numRigs = mNumRigs;
	}

	// For every rig
	for (int rigIndex = 0; rigIndex < numRigs; rigIndex++)
	{
		// Get the number of joints in the rig
		unsigned int numJoints = mRigs[rigIndex]->GetNumJoints();

		// For every joint in the rig
		for (unsigned int jointIndex = 0; jointIndex < numJoints; jointIndex++)
		{
			if (mDrawBones)
			{
				// add bones data to the uniform
				mUniformDataBones.mToWorldMat[instanceCount] = mRigs[rigIndex]->GetBoneWorldMat(jointIndex);
				mUniformDataBones.mColor[instanceCount] = mRigs[rigIndex]->GetBoneColor();

				// add joint data to the uniform while scaling the joints by their determined chlid bone length
				mUniformDataJoints.mToWorldMat[instanceCount] =
					mRigs[rigIndex]->GetJointWorldMatNoScale(jointIndex) * mat4::scale(mRigs[rigIndex]->GetJointScale(jointIndex));
			}
			else
			{
				// add joint data to the uniform without scaling
				mUniformDataJoints.mToWorldMat[instanceCount] = mRigs[rigIndex]->GetJointWorldMatNoScale(jointIndex);
			}
			mUniformDataJoints.mColor[instanceCount] = mRigs[rigIndex]->GetJointColor();

			// increment the count of uniform data that has been filled for this batch
			instanceCount++;

			// If we have reached our maximun amount of instances, or the end of our data
			if ((instanceCount == MAX_INSTANCES) || ((rigIndex == numRigs - 1) && (jointIndex == numJoints - 1)))
			{
				// Finalize the data for this batch

				unsigned int currBatch = mBatchCounts[frameIndex];

				BufferUpdateDesc viewProjCbvJoints = { mProjViewUniformBufferJoints[currBatch][frameIndex], &mUniformDataJoints };
				updateResource(&viewProjCbvJoints);

				if (mDrawBones)
				{
					BufferUpdateDesc viewProjCbvBones = { mProjViewUniformBufferBones[currBatch][frameIndex], &mUniformDataBones };
					updateResource(&viewProjCbvBones);
				}

				// Increment the total batch count for this frame index
				mBatchCounts[frameIndex]++;

				// If we have reached the end of our data, save the number of instances in this
				// last batch as it could be less than MAX_INSTANCES
				if ((rigIndex == numRigs - 1) && (jointIndex == numJoints - 1))
				{
					mLastBatchSize[frameIndex] = instanceCount;
				}
				// Otherwise reset the count so it can be used for the next batch
				else
				{
					instanceCount = 0;
				}
			}
		}
	}
}

void SkeletonBatcher::AddRig(Rig* rig)
{
	// Adds the rig so its data can be used and increments the rig count
	mRigs.push_back(rig);
	mNumRigs++;
}

void SkeletonBatcher::Draw(Cmd* cmd, const uint32_t& frameIndex)
{
	// Get the number of batches to draw for this frameindex
	unsigned int numBatches = mBatchCounts[frameIndex];

	cmdBindPipeline(cmd, mSkeletonPipeline);
	DescriptorData params[1] = {};
	params[0].pName = "uniformBlock";

	// Joints
	cmdBeginDebugMarker(cmd, 1, 0, 1, "Draw Skeletons Joints");
	cmdBindVertexBuffer(cmd, 1, &mJointVertexBuffer, NULL);

	// for each batch of joints
	for (unsigned int batchIndex = 0; batchIndex < numBatches; batchIndex++)
	{
		params[0].ppBuffers = &mProjViewUniformBufferJoints[batchIndex][frameIndex];
		cmdBindDescriptors(cmd, mRootSignature, 1, params);

		if (batchIndex < numBatches - 1)
		{
			// Draw MAX_INSTANCES number of joints
			cmdDrawInstanced(cmd, mNumJointPoints / 6, 0, MAX_INSTANCES, 0);
		}
		else
		{
			// For the last batch use its recorded number of instances for this frameindex
			cmdDrawInstanced(cmd, mNumJointPoints / 6, 0, mLastBatchSize[frameIndex], 0);
		}
	}
	cmdEndDebugMarker(cmd);

	// Bones
	if (mDrawBones)
	{
		cmdBindVertexBuffer(cmd, 1, &mBoneVertexBuffer, NULL);
		cmdBeginDebugMarker(cmd, 1, 0, 1, "Draw Skeletons Bones");

		// for each batch of bones
		for (unsigned int batchIndex = 0; batchIndex < numBatches; batchIndex++)
		{
			params[0].ppBuffers = &mProjViewUniformBufferBones[batchIndex][frameIndex];
			cmdBindDescriptors(cmd, mRootSignature, 1, params);

			if (batchIndex < numBatches - 1)
			{
				// Draw MAX_INSTANCES number of bones
				cmdDrawInstanced(cmd, mNumBonePoints / 6, 0, MAX_INSTANCES, 0);
			}
			else
			{
				// For the last batch use its recorded number of instances for this frameindex
				cmdDrawInstanced(cmd, mNumBonePoints / 6, 0, mLastBatchSize[frameIndex], 0);
			}
		}
		cmdEndDebugMarker(cmd);
	}
}