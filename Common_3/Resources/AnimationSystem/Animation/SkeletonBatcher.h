/*
 * Copyright (c) 2017-2024 The Forge Interactive Inc.
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

#include "../../../Application/Interfaces/ICameraController.h"
#include "../../../Graphics/Interfaces/IGraphics.h"
#include "../../../Resources/ResourceLoader/Interfaces/IResourceLoader.h"

#include "AnimatedObject.h"
#include "Rig.h"

enum JointMeshType
{
    QuadSphere,
    Cube
};
// Uniform data to send
struct UniformSkeletonBlock
{
    CameraMatrix mProjectView;
    mat4         mViewMatrix;

    vec4  mColor[MAX_SKELETON_BATCHER_BLOCK_INSTANCES];
    // Point Light Information
    vec4  mLightPosition;
    vec4  mLightColor;
    vec4  mJointColor;
    uint4 mSkeletonInfo;
    mat4  mToWorldMat[MAX_SKELETON_BATCHER_BLOCK_INSTANCES];
};

// Description needed to handle buffer updates and draw calls
struct SkeletonRenderDesc
{
    Renderer* mRenderer;
    Buffer*   mJointVertexBuffer;
    Buffer*   mBoneVertexBuffer;

    uint32_t mFrameCount;
    uint32_t mMaxSkeletonBatches;

    uint32_t mJointVertexStride;
    uint32_t mNumJointPoints;

    uint32_t mBoneVertexStride;
    uint32_t mNumBonePoints;

    uint32_t mMaxAnimatedObjects;

    BufferCreationFlags mCreationFlag;
    bool                mDrawBones;
    JointMeshType       mJointMeshType;

    const char* mJointVertShaderName;
    const char* mJointFragShaderName;
};

typedef struct SkeletonBatcherLoadDesc
{
    ReloadType  mLoadType;
    uint32_t    mColorFormat; // enum TinyImageFormat
    uint32_t    mDepthFormat; // enum TinyImageFormat
    SampleCount mSampleCount;
    uint32_t    mSampleQuality;
} SkeletonBatcherLoadDesc;

// Allows for efficiently instance rendering all joints and bones of all skeletons in the scene
// Will eventually be a debug option and a part of a much larger Animation System's draw functionalities
class FORGE_API SkeletonBatcher
{
public:
    // Set up the pipeline and initialize the buffers
    void Initialize(const SkeletonRenderDesc& skeletonRenderDesc);

    // Must be called to clean up the object if initialize was called
    void Exit();

    void Load(const SkeletonBatcherLoadDesc* pDesc);

    void Unload(ReloadType reloadType);

    void PrepareDescriptorSets();

    // Add a rig to the list of skeletons to draw
    void AddAnimatedObject(AnimatedObject* animatedObject);

    void RemoveAnimatedObject(AnimatedObject* animatedObject);
    void RemoveAllAnimatedObjects();

    void SetActiveRigs(uint32_t activeRigs);

    // Update uniforms that will be shared between all skeletons
    void SetSharedUniforms(const CameraMatrix& projViewMat, const mat4& viewMat, const Vector3& lightPos, const Vector3& lightColor);

    // Must be called on a single thread before any call to SetPerInstanceUniforms
    void PreSetInstanceUniforms(const uint32_t frameIndex);

    // Update all the instanced uniform data for each batch of joints and bones
    // Can be called asyncronously for different object ranges
    void SetPerInstanceUniforms(const uint32_t frameIndex, int32_t numObjects = -1, uint32_t objectsOffset = 0);

    // Instance draw all the skeletons
    void Draw(Cmd* cmd, const uint32_t frameIndex);

    Shader*        mJointShader = NULL;
    Shader*        mBoneShader = NULL;
    Pipeline*      mJointPipeline = NULL;
    Pipeline*      mBonePipeline = NULL;
    RootSignature* mRootSignature = NULL;

private:
#ifdef ENABLE_FORGE_ANIMATION_DEBUG
    uint32_t mMaxAnimatedObjects = 0;

    // List of Rigs whose skeletons need to be rendered
    AnimatedObject** mAnimatedObjects = NULL;
    uint32_t*        mCumulativeAnimatedObjectInstanceCount = NULL;

    uint32_t mFrameCount = 0;
    uint32_t mMaxSkeletonBatches = 0;
    uint32_t mNumAnimatedObjects = 0;
    uint32_t mNumActiveAnimatedObjects = 0;

    // Application variables used to be able to update buffers
    Renderer* mRenderer = NULL;
    Buffer*   mJointVertexBuffer = NULL;
    Buffer*   mBoneVertexBuffer = NULL;
    uint32_t  mJointVertexStride = 0;
    uint32_t  mBoneVertexStride = 0;
    uint32_t  mNumJointPoints = 0;
    uint32_t  mNumBonePoints = 0;

    // Descriptor binder with all required memory allocation space
    DescriptorSet* pDescriptorSet = NULL;

    // Buffer pointers that will get updated for each batch to be rendered
    Buffer** mProjViewUniformBufferJoints = NULL;
    Buffer** mProjViewUniformBufferBones = NULL;

    // Uniform data for the joints and bones
    UniformSkeletonBlock* mUniformDataJoints = NULL;

    const char* mJointVertShaderName = NULL;
    const char* mJointFragShaderName = NULL;

    tfrg_atomic32_t mInstanceCount = 0;

    // Keeps track of the number of batches we will send for instanced rendering
    // for each frame index
    tfrg_atomic32_t* mBatchCounts = NULL;

    // Keeps track of the size of the last batch as it can be less than MAX_INSTANCES
    tfrg_atomic32_t* mBatchSize = NULL;

    // Determines if this renderer will need to draw bones between each joint
    // Set in initialize
    bool mDrawBones = false;

    JointMeshType mJointMeshType = QuadSphere;
#endif
};
