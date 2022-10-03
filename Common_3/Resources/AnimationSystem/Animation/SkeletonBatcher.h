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

#pragma once

#include "../../../Graphics/Interfaces/IGraphics.h"
#include "../../../Resources/ResourceLoader/Interfaces/IResourceLoader.h"
#include "../../../Application/Interfaces/ICameraController.h"

#include "Rig.h"
#include "AnimatedObject.h"

#define MAX_SKELETON_BLOCK_INSTANCES 815    // For allocating space in uniform block. Must match with shader and application.

#define MAX_SKELETON_BATCHES 512    // Batch count must always be less than this

// #nocheckin MAke this a define, customizable in OS Config
const uint32_t ImageCount = 3;    // must match the application

// Uniform data to send
struct UniformSkeletonBlock
{
    CameraMatrix mProjectView;

	vec4 mColor[MAX_SKELETON_BLOCK_INSTANCES];
	// Point Light Information
	vec4 mLightPosition;
	vec4 mLightColor;

	mat4 mToWorldMat[MAX_SKELETON_BLOCK_INSTANCES];
};

// Description needed to handle buffer updates and draw calls
struct SkeletonRenderDesc
{
	Renderer*           mRenderer;
	Pipeline*           mSkeletonPipeline;
	Buffer*             mJointVertexBuffer;
	uint32_t            mJointVertexStride;
	uint32_t            mNumJointPoints;
	Buffer*             mBoneVertexBuffer;
	uint32_t            mBoneVertexStride;
	uint32_t            mNumBonePoints;
	BufferCreationFlags mCreationFlag;
	bool                mDrawBones;
	uint32_t			mMaxAnimatedObjects;
};

// Allows for efficiently instance rendering all joints and bones of all skeletons in the scene
// Will eventually be a debug option and a part of a much larger Animation System's draw functionalities
class FORGE_API SkeletonBatcher
{
	public:
	// Set up the pipeline and initialize the buffers
	void Initialize(const SkeletonRenderDesc& skeletonRenderDesc);

	// Must be called to clean up the object if initialize was called
	void Exit();

	// Update the mSkeletonPipeline pointer now that the pipeline has been loaded
	void LoadPipeline(Pipeline* pipeline);

	void AddDescriptorSets(RootSignature* pRootSignature);

	void RemoveDescriptorSets();

	void PrepareDescriptorSets();

	// Add a rig to the list of skeletons to draw
	void AddAnimatedObject(AnimatedObject* animatedObject);

	void RemoveAnimatedObject(AnimatedObject* animatedObject);
	void RemoveAllAnimatedObjects();

	void SetActiveRigs(uint32_t activeRigs);

	// Update uniforms that will be shared between all skeletons
	void SetSharedUniforms(const CameraMatrix& projViewMat, const Vector3& lightPos, const Vector3& lightColor);

	// Update all the instanced uniform data for each batch of joints and bones
	void SetPerInstanceUniforms(const uint32_t frameIndex, int32_t numRigs = -1, uint32_t rigsOffset = 0);

	// Instance draw all the skeletons
	void Draw(Cmd* cmd, const uint32_t frameIndex);

	private:

#ifdef ENABLE_FORGE_ANIMATION_DEBUG
	uint32_t mMaxAnimatedObjects = 0;

	// List of Rigs whose skeletons need to be rendered
	AnimatedObject** mAnimatedObjects = NULL;
	uint32_t* mCumulativeAnimatedObjectInstanceCount = NULL;

	uint32_t            mNumAnimatedObjects = 0;
	uint32_t            mNumActiveAnimatedObjects = 0;

	// Application variables used to be able to update buffers
	Renderer*      mRenderer = NULL;
	Pipeline*      mSkeletonPipeline = NULL;
	Buffer*        mJointVertexBuffer = NULL;
	Buffer*        mBoneVertexBuffer = NULL;
	uint32_t       mJointVertexStride = 0;
	uint32_t       mBoneVertexStride = 0;
	uint32_t       mNumJointPoints = 0;
	uint32_t       mNumBonePoints = 0;

	// Descriptor binder with all required memory allocation space
	DescriptorSet*  pDescriptorSet = NULL;

	// Buffer pointers that will get updated for each batch to be rendered
	Buffer*  mProjViewUniformBufferJoints[ImageCount][MAX_SKELETON_BATCHES] = { { {} } };
	Buffer*  mProjViewUniformBufferBones[ImageCount][MAX_SKELETON_BATCHES] = { { {} } };

	// Uniform data for the joints and bones
	UniformSkeletonBlock mUniformDataJoints[MAX_SKELETON_BATCHES];
	UniformSkeletonBlock mUniformDataBones[MAX_SKELETON_BATCHES];

	// Keeps track of the number of batches we will send for instanced rendering
	// for each frame index
	tfrg_atomic32_t mBatchCounts[ImageCount] = {};

	tfrg_atomic32_t mInstanceCount = 0;

	// Keeps track of the size of the last batch as it can be less than MAX_INSTANCES
	tfrg_atomic32_t mBatchSize[ImageCount][MAX_SKELETON_BATCHES] = {};

	// Determines if this renderer will need to draw bones between each joint
	// Set in initialize
	bool mDrawBones = false;
#endif
};
