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

#include "SkeletonBatcher.h"

#include "../../../Utilities/ThirdParty/OpenSource/Nothings/stb_ds.h"

void SkeletonBatcher::Initialize(const SkeletonRenderDesc& skeletonRenderDesc)
{
#ifdef ENABLE_FORGE_ANIMATION_DEBUG
    // Set member render variables based on the description
    mRenderer = skeletonRenderDesc.mRenderer;
    mJointVertexBuffer = skeletonRenderDesc.mJointVertexBuffer;
    mNumJointPoints = skeletonRenderDesc.mNumJointPoints;
    mJointVertexStride = skeletonRenderDesc.mJointVertexStride;
    mBoneVertexStride = skeletonRenderDesc.mBoneVertexStride;
    mJointMeshType = skeletonRenderDesc.mJointMeshType;
    mFrameCount = skeletonRenderDesc.mFrameCount;
    mMaxSkeletonBatches = skeletonRenderDesc.mMaxSkeletonBatches;
    mJointVertShaderName = skeletonRenderDesc.mJointVertShaderName;
    mJointFragShaderName = skeletonRenderDesc.mJointFragShaderName;

    ASSERT(mFrameCount > 0);
    ASSERT(mMaxSkeletonBatches > 0);

    ASSERT(skeletonRenderDesc.mMaxAnimatedObjects > 0 && "Need to specify the maximum number of animated objects");
    mMaxAnimatedObjects = skeletonRenderDesc.mMaxAnimatedObjects;
    arrsetlen(mAnimatedObjects, skeletonRenderDesc.mMaxAnimatedObjects);
    arrsetlen(mCumulativeAnimatedObjectInstanceCount, skeletonRenderDesc.mMaxAnimatedObjects + 1);
    memset(mAnimatedObjects, 0, sizeof(*mAnimatedObjects) * skeletonRenderDesc.mMaxAnimatedObjects);
    memset(mCumulativeAnimatedObjectInstanceCount, 0,
           sizeof(*mCumulativeAnimatedObjectInstanceCount) * (skeletonRenderDesc.mMaxAnimatedObjects + 1));

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

    mProjViewUniformBufferJoints = (Buffer**)tf_calloc_memalign(mFrameCount * mMaxSkeletonBatches * 2, alignof(Buffer*), sizeof(Buffer*));
    mProjViewUniformBufferBones = mProjViewUniformBufferJoints + (mFrameCount * mMaxSkeletonBatches);
    mUniformDataJoints =
        (UniformSkeletonBlock*)tf_calloc_memalign(mMaxSkeletonBatches, alignof(UniformSkeletonBlock), sizeof(UniformSkeletonBlock));

    mBatchCounts = (tfrg_atomic32_t*)tf_calloc_memalign(mFrameCount + mFrameCount * mMaxSkeletonBatches, alignof(tfrg_atomic32_t),
                                                        sizeof(tfrg_atomic32_t));
    mBatchSize = mBatchCounts + mFrameCount;

    // Initialize all the buffer that will be used for each batch per each frame index
    BufferLoadDesc ubDesc = {};
    ubDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ubDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
    ubDesc.mDesc.mSize = sizeof(UniformSkeletonBlock);
    ubDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT | skeletonRenderDesc.mCreationFlag;
    ubDesc.pData = NULL;

    for (uint32_t i = 0; i < mFrameCount; ++i)
    {
        for (uint32_t j = 0; j < mMaxSkeletonBatches; ++j)
        {
            const uint32_t bufferIndex = i * mMaxSkeletonBatches + j;

            ubDesc.ppBuffer = &mProjViewUniformBufferJoints[bufferIndex];
            addResource(&ubDesc, NULL);

            if (mDrawBones)
            {
                ubDesc.ppBuffer = &mProjViewUniformBufferBones[bufferIndex];
                addResource(&ubDesc, NULL);
            }
        }
    }

#endif
}

void SkeletonBatcher::Exit()
{
#ifdef ENABLE_FORGE_ANIMATION_DEBUG

    for (uint32_t i = 0; i < mFrameCount; ++i)
    {
        for (uint32_t j = 0; j < mMaxSkeletonBatches; ++j)
        {
            const uint32_t bufferIndex = i * mMaxSkeletonBatches + j;
            removeResource(mProjViewUniformBufferJoints[bufferIndex]);
            if (mDrawBones)
            {
                removeResource(mProjViewUniformBufferBones[bufferIndex]);
            }
        }
    }

    arrfree(mAnimatedObjects);
    arrfree(mCumulativeAnimatedObjectInstanceCount);

    tf_free((void*)mBatchCounts);
    tf_free(mUniformDataJoints);
    tf_free(mProjViewUniformBufferJoints);
#endif
}

void SkeletonBatcher::Load(const SkeletonBatcherLoadDesc* pDesc)
{
#ifdef ENABLE_FORGE_ANIMATION_DEBUG

    if (pDesc->mLoadType & RELOAD_TYPE_SHADER)
    {
        ShaderLoadDesc jointShader = {};
        // if a custom shader is required for joints
        if (mJointVertShaderName && mJointFragShaderName)
        {
            jointShader.mVert.pFileName = mJointVertShaderName;
            jointShader.mFrag.pFileName = mJointFragShaderName;
        }
        else
        {
            jointShader.mVert.pFileName = "joint.vert";
            jointShader.mFrag.pFileName = "joint.frag";
        }

        ShaderLoadDesc boneShader = {};
        boneShader.mVert.pFileName = "bone.vert";
        boneShader.mFrag.pFileName = "bone.frag";

        addShader(mRenderer, &jointShader, &mJointShader);
        addShader(mRenderer, &boneShader, &mBoneShader);

        Shader*           shaders[] = { mJointShader, mBoneShader };
        RootSignatureDesc rootDesc = {};
        rootDesc.mShaderCount = 2;
        rootDesc.ppShaders = shaders;

        addRootSignature(mRenderer, &rootDesc, &mRootSignature);

        // 2 because updates buffer twice per instanced draw call: one for joints and one for bones
        DescriptorSetDesc setDesc = { mRootSignature, DESCRIPTOR_UPDATE_FREQ_PER_DRAW, mMaxSkeletonBatches * 2 * mFrameCount };
        addDescriptorSet(mRenderer, &setDesc, &pDescriptorSet);
    }

    PrepareDescriptorSets();

    if (pDesc->mLoadType & (RELOAD_TYPE_SHADER | RELOAD_TYPE_RENDERTARGET))
    {
        VertexLayout jointLayout = {};
        jointLayout.mBindingCount = 1;
        jointLayout.mAttribCount = 2;
        jointLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
        jointLayout.mAttribs[0].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
        jointLayout.mAttribs[0].mBinding = 0;
        jointLayout.mAttribs[0].mLocation = 0;
        jointLayout.mAttribs[0].mOffset = 0;
        jointLayout.mAttribs[1].mSemantic = SEMANTIC_NORMAL;
        jointLayout.mAttribs[1].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
        jointLayout.mAttribs[1].mBinding = 0;
        jointLayout.mAttribs[1].mLocation = 1;
        jointLayout.mAttribs[1].mOffset = 3 * sizeof(float);

        VertexLayout boneVertexLayout = {};
        boneVertexLayout.mBindingCount = 1;
        boneVertexLayout.mAttribCount = 3;
        boneVertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
        boneVertexLayout.mAttribs[0].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
        boneVertexLayout.mAttribs[0].mBinding = 0;
        boneVertexLayout.mAttribs[0].mLocation = 0;
        boneVertexLayout.mAttribs[0].mOffset = 0;
        boneVertexLayout.mAttribs[1].mSemantic = SEMANTIC_NORMAL;
        boneVertexLayout.mAttribs[1].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
        boneVertexLayout.mAttribs[1].mBinding = 0;
        boneVertexLayout.mAttribs[1].mLocation = 1;
        boneVertexLayout.mAttribs[1].mOffset = 3 * sizeof(float);
        boneVertexLayout.mAttribs[2].mSemantic = SEMANTIC_JOINTS;
        boneVertexLayout.mAttribs[2].mFormat = TinyImageFormat_R16G16B16A16_UINT;
        boneVertexLayout.mAttribs[2].mBinding = 0;
        boneVertexLayout.mAttribs[2].mLocation = 2;
        boneVertexLayout.mAttribs[2].mOffset = 6 * sizeof(float);

        RasterizerStateDesc skeletonRasterizerStateDesc = {};
        skeletonRasterizerStateDesc.mCullMode = CULL_MODE_FRONT;

        DepthStateDesc depthStateDesc = {};
        depthStateDesc.mDepthTest = true;
        depthStateDesc.mDepthWrite = true;
        depthStateDesc.mDepthFunc = CMP_GEQUAL;

        PipelineDesc desc = {};
        desc.mType = PIPELINE_TYPE_GRAPHICS;
        GraphicsPipelineDesc& pipelineSettings = desc.mGraphicsDesc;
        if (mJointMeshType == Cube)
        {
            pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
        }
        else
        {
            pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_STRIP;
        }
        pipelineSettings.mRenderTargetCount = 1;
        pipelineSettings.pDepthState = &depthStateDesc;
        pipelineSettings.pColorFormats = (TinyImageFormat*)&pDesc->mColorFormat;
        pipelineSettings.mSampleCount = pDesc->mSampleCount;
        pipelineSettings.mSampleQuality = pDesc->mSampleQuality;
        pipelineSettings.mDepthStencilFormat = (TinyImageFormat)pDesc->mDepthFormat;
        pipelineSettings.pRootSignature = mRootSignature;

        pipelineSettings.pShaderProgram = mJointShader;
        pipelineSettings.pVertexLayout = &jointLayout;
        pipelineSettings.pRasterizerState = &skeletonRasterizerStateDesc;
        addPipeline(mRenderer, &desc, &mJointPipeline);

        pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
        pipelineSettings.pShaderProgram = mBoneShader;
        pipelineSettings.pVertexLayout = &boneVertexLayout;
        addPipeline(mRenderer, &desc, &mBonePipeline);
    }
#endif
}
void SkeletonBatcher::Unload(ReloadType reloadType)
{
#ifdef ENABLE_FORGE_ANIMATION_DEBUG

    if (reloadType & (RELOAD_TYPE_SHADER | RELOAD_TYPE_RENDERTARGET))
    {
        removePipeline(mRenderer, mBonePipeline);
        removePipeline(mRenderer, mJointPipeline);
    }

    if (reloadType & RELOAD_TYPE_SHADER)
    {
        removeDescriptorSet(mRenderer, pDescriptorSet);
        removeRootSignature(mRenderer, mRootSignature);
        removeShader(mRenderer, mBoneShader);
        removeShader(mRenderer, mJointShader);
    }
#endif
}

void SkeletonBatcher::PrepareDescriptorSets()
{
#ifdef ENABLE_FORGE_ANIMATION_DEBUG
    DescriptorData params[1] = {};
    params[0].pName = "uniformBlock";

    for (uint32_t i = 0; i < mFrameCount; ++i)
    {
        for (uint32_t j = 0; j < mMaxSkeletonBatches; ++j)
        {
            const uint32_t bufferIndex = i * mMaxSkeletonBatches + j;

            params[0].ppBuffers = &mProjViewUniformBufferJoints[bufferIndex];
            updateDescriptorSet(mRenderer, (i * (mMaxSkeletonBatches * 2)) + (j * 2 + 0), pDescriptorSet, 1, params);

            if (mDrawBones)
            {
                params[0].ppBuffers = &mProjViewUniformBufferBones[bufferIndex];
                updateDescriptorSet(mRenderer, (i * (mMaxSkeletonBatches * 2)) + (j * 2 + 1), pDescriptorSet, 1, params);
            }
        }
    }
#endif
}

void SkeletonBatcher::SetSharedUniforms(const CameraMatrix& projViewMat, const mat4& viewMat, const Vector3& lightPos,
                                        const Vector3& lightColor)
{
#ifdef ENABLE_FORGE_ANIMATION_DEBUG
    // we only need the rotation of view matrix in shaders to calculate billboards and fake lighting
    mat4 viewMatNoTranslation = viewMat;
    viewMatNoTranslation.setCol(3, Vector4(0.0f, 0.0f, 0.0f, 1.0f));
    for (uint32_t i = 0; i < mMaxSkeletonBatches; ++i)
    {
        mUniformDataJoints[i].mViewMatrix = viewMatNoTranslation;
        mUniformDataJoints[i].mProjectView = projViewMat;
        mUniformDataJoints[i].mLightPosition = Vector4(lightPos);
        mUniformDataJoints[i].mLightColor = Vector4(lightColor);
    }
#endif
}

void SkeletonBatcher::SetActiveRigs(uint32_t activeRigs)
{
#ifdef ENABLE_FORGE_ANIMATION_DEBUG
    mNumActiveAnimatedObjects = min(activeRigs, mNumAnimatedObjects);
#endif
}

void SkeletonBatcher::PreSetInstanceUniforms(const uint32_t frameIndex)
{
#ifdef ENABLE_FORGE_ANIMATION_DEBUG
    // Reset batch counts
    tfrg_atomic32_t* pFrameBatchSize = &mBatchSize[frameIndex * mMaxSkeletonBatches];

    tfrg_atomic32_store_relaxed(&mBatchCounts[frameIndex], 0);
    for (uint32_t i = 0; i < mMaxSkeletonBatches; ++i)
    {
        tfrg_atomic32_store_relaxed(&pFrameBatchSize[i], 0);
    }
#endif
}

void SkeletonBatcher::SetPerInstanceUniforms(const uint32_t frameIndex, int32_t numObjects, uint32_t objectsOffset)
{
#ifdef ENABLE_FORGE_ANIMATION_DEBUG
    ASSERT(frameIndex < mFrameCount);

    // If the numObjects parameter was not initialized, used the data from all the active rigs
    if (numObjects == -1)
    {
        numObjects = mNumActiveAnimatedObjects;
    }

    const uint32_t lastBatchIndex =
        mCumulativeAnimatedObjectInstanceCount[mNumActiveAnimatedObjects] / MAX_SKELETON_BATCHER_BLOCK_INSTANCES;
    const uint32_t lastBatchSize = mCumulativeAnimatedObjectInstanceCount[mNumActiveAnimatedObjects] % MAX_SKELETON_BATCHER_BLOCK_INSTANCES;

    // Will keep track of the number of instances that have their data added
    const uint32_t totalInstanceCount =
        mCumulativeAnimatedObjectInstanceCount[objectsOffset + numObjects] - mCumulativeAnimatedObjectInstanceCount[objectsOffset];
    uint32_t instanceCount = tfrg_atomic32_add_relaxed(&mInstanceCount, totalInstanceCount);

    // Last resets mInstanceCount
    if (instanceCount + totalInstanceCount == mCumulativeAnimatedObjectInstanceCount[mNumActiveAnimatedObjects])
        mInstanceCount = 0;

    uint32_t batchInstanceCount = 0;
    uint32_t batchIndex = instanceCount / MAX_SKELETON_BATCHER_BLOCK_INSTANCES;

    tfrg_atomic32_t* pFrameBatchSize = &mBatchSize[frameIndex * mMaxSkeletonBatches];

    // For every rig
    for (uint32_t objIndex = objectsOffset; objIndex < numObjects + objectsOffset; ++objIndex)
    {
        const AnimatedObject* animObj = mAnimatedObjects[objIndex];

        // Get the number of joints in the rig
        uint32_t numJoints = animObj->mRig->mNumJoints;
        // For every joint in the rig
        for (uint32_t jointIndex = 0; jointIndex < numJoints; jointIndex++)
        {
            uint32_t              instanceIndex = instanceCount % MAX_SKELETON_BATCHER_BLOCK_INSTANCES;
            UniformSkeletonBlock& uniformDataJoints = mUniformDataJoints[batchIndex];

            mUniformDataJoints[batchIndex].mSkeletonInfo = uint4(numJoints, 1, 0, 0);
            uniformDataJoints.mToWorldMat[instanceIndex] =
                animObj->GetJointWorldMatNoScale(jointIndex) * mat4::scale(animObj->mJointScales[jointIndex]);
            uniformDataJoints.mColor[instanceIndex] = animObj->mBoneColor;
            uniformDataJoints.mJointColor = animObj->mJointColor;

            // increment the count of uniform data that has been filled for this batch
            ++instanceCount;
            ++batchInstanceCount;

            // If we have reached our maximun amount of instances, or the end of our data
            if ((instanceIndex == MAX_SKELETON_BATCHER_BLOCK_INSTANCES - 1) ||
                ((objIndex - objectsOffset == (uint32_t)(numObjects - 1)) && (jointIndex == numJoints - 1)))
            {
                // Finalize the data for this batch by adding the batch instance to the batch total size
                uint32_t currBatchSize = tfrg_atomic32_add_relaxed(&pFrameBatchSize[batchIndex], batchInstanceCount) + batchInstanceCount;

                // Only update if batch is full, or this is the last batch as it could be less than MAX_SKELETON_BATCHER_BLOCK_INSTANCES
                if (currBatchSize == MAX_SKELETON_BATCHER_BLOCK_INSTANCES ||
                    (lastBatchIndex == batchIndex && currBatchSize == lastBatchSize))
                {
                    const uint32_t bufferIndex = frameIndex * mMaxSkeletonBatches + batchIndex;

                    tfrg_atomic32_add_relaxed(&mBatchCounts[frameIndex], 1);
                    BufferUpdateDesc viewProjCbvJoints = { mProjViewUniformBufferJoints[bufferIndex] };
                    beginUpdateResource(&viewProjCbvJoints);
                    memcpy(viewProjCbvJoints.pMappedData, &uniformDataJoints, sizeof(uniformDataJoints));
                    endUpdateResource(&viewProjCbvJoints);
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
    ASSERT(joints < MAX_SKELETON_BATCHER_BLOCK_INSTANCES * mMaxSkeletonBatches && "Exceed maximum amount of instances");
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
            ASSERT(cumulativeJoints < MAX_SKELETON_BATCHER_BLOCK_INSTANCES * mMaxSkeletonBatches && "Exceed maximum amount of instances");
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
    if (numBatches == 0)
    {
        return;
    }

    tfrg_atomic32_t* pFrameBatchSize = &mBatchSize[frameIndex * mMaxSkeletonBatches];

    cmdBindPipeline(cmd, mJointPipeline);

    // Joints
    cmdBeginDebugMarker(cmd, 1, 0, 1, "Draw Skeletons Joints");
    cmdBindVertexBuffer(cmd, 1, &mJointVertexBuffer, &mJointVertexStride, NULL);

    // for each batch of joints
    for (uint32_t batchIndex = 0; batchIndex < numBatches; batchIndex++)
    {
        cmdBindDescriptorSet(cmd, (frameIndex * (mMaxSkeletonBatches * 2)) + (batchIndex * 2 + 0), pDescriptorSet);
        cmdDrawInstanced(cmd, mNumJointPoints / 6, 0, pFrameBatchSize[batchIndex], 0);
        if (!mDrawBones)
            pFrameBatchSize[batchIndex] = 0;
    }
    cmdEndDebugMarker(cmd);

    // Bones
    if (mDrawBones)
    {
        cmdBindPipeline(cmd, mBonePipeline);
        cmdBindVertexBuffer(cmd, 1, &mBoneVertexBuffer, &mBoneVertexStride, NULL);
        cmdBeginDebugMarker(cmd, 1, 0, 1, "Draw Skeletons Bones");

        // for each batch of bones

        const AnimatedObject* animObj = mAnimatedObjects[0];
        uint32_t              numJoints = animObj->mRig->mNumJoints;

        for (uint32_t batchIndex = 0; batchIndex < numBatches; batchIndex++)
        {
            uint32_t instanceCount = pFrameBatchSize[batchIndex] / numJoints;
            cmdBindDescriptorSet(cmd, (frameIndex * (mMaxSkeletonBatches * 2)) + (batchIndex * 2 + 0), pDescriptorSet);
            cmdDrawInstanced(cmd, mNumBonePoints / 8, 0, instanceCount, 0);

            pFrameBatchSize[batchIndex] = 0;
        }
        cmdEndDebugMarker(cmd);
    }
#endif
}
