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

#include "../../Common_3/Renderer/IRenderer.h"
#include "../../Common_3/Renderer/IResourceLoader.h"

#include "Rig.h"

#define MAX_INSTANCES 815    // For allocating space in uniform block. Must match with shader and application.

#define MAX_BATCHES 500    // Batch count must always be less than this

const uint32_t ImageCount = 3;    // must match the application

// Uniform data to send
struct UniformSkeletonBlock
{
	mat4 mProjectView;

	vec4 mColor[MAX_INSTANCES];
	// Point Light Information
	vec4 mLightPosition;
	vec4 mLightColor;

	mat4 mToWorldMat[MAX_INSTANCES];
};

// Description needed to handle buffer updates and draw calls
struct SkeletonRenderDesc
{
	Renderer*           mRenderer;
	Pipeline*           mSkeletonPipeline;
	RootSignature*      mRootSignature;
	Buffer*             mJointVertexBuffer;
	uint32_t            mJointVertexStride;
	uint32_t            mNumJointPoints;
	Buffer*             mBoneVertexBuffer;
	uint32_t            mBoneVertexStride;
	uint32_t            mNumBonePoints;
	BufferCreationFlags mCreationFlag;
	bool                mDrawBones;
};

// Allows for efficiently instance rendering all joints and bones of all skeletons in the scene
// Will eventually be a debug option and a part of a much larger Animation System's draw functionalities
class SkeletonBatcher
{
	public:
	// Set up the pipeline and initialize the buffers
	void Initialize(const SkeletonRenderDesc& skeletonRenderDesc);

	// Must be called to clean up the object if initialize was called
	void Destroy();

	// Update the mSkeletonPipeline pointer now that the pipeline has been loaded
	inline void LoadPipeline(Pipeline* pipeline) { mSkeletonPipeline = pipeline; };

	// Add a rig to the list of skeletons to draw
	void AddRig(Rig* rig);

	// Update uniforms that will be shared between all skeletons
	void SetSharedUniforms(const Matrix4& projViewMat, const Vector3& lightPos, const Vector3& lightColor);

	// Update all the instanced uniform data for each batch of joints and bones
	void SetPerInstanceUniforms(const uint32_t& frameIndex, int numRigs = -1);

	// Instance draw all the skeletons
	void Draw(Cmd* cmd, const uint32_t& frameIndex);

	private:
	// List of Rigs whose skeletons need to be rendered
	eastl::vector<Rig*> mRigs;
	uint32_t            mNumRigs = 0;

	// Application variables used to be able to update buffers
	Renderer*      mRenderer;
	Pipeline*      mSkeletonPipeline;
	RootSignature* mRootSignature;
	Buffer*        mJointVertexBuffer;
	Buffer*        mBoneVertexBuffer;
	uint32_t       mJointVertexStride;
	uint32_t       mBoneVertexStride;
	uint32_t       mNumJointPoints;
	uint32_t       mNumBonePoints;

	// Descriptor binder with all required memory allocation space
	DescriptorSet*  pDescriptorSet;

	// Buffer pointers that will get updated for each batch to be rendered
	Buffer*  mProjViewUniformBufferJoints[ImageCount][MAX_BATCHES] = { { {} } };
	Buffer*  mProjViewUniformBufferBones[ImageCount][MAX_BATCHES] = { { {} } };

	// Uniform data for the joints and bones
	UniformSkeletonBlock mUniformDataJoints;
	UniformSkeletonBlock mUniformDataBones;

	// Keeps track of the number of batches we will send for instanced rendering
	// for each frame index
	uint32_t mBatchCounts[ImageCount];

	// Keeps track of the size of the last batch as it can be less than MAX_INSTANCES
	uint32_t mLastBatchSize[ImageCount];

	// Determines if this renderer will need to draw bones between each joint
	// Set in initialize
	bool mDrawBones;
};
