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

#include "../../Application/Config.h"

#include "../../Graphics/Interfaces/IGraphics.h"

#include "../../Utilities/Math/MathTypes.h"

typedef enum FilterContainerType
{
    FILTER_CONTAINER_TYPE_CLUSTER, // Clusters will be generated for this container type, which are able to be culled before triangle filter
                                   // pass
    FILTER_CONTAINER_TYPE_LIST,

    FILTER_CONTAINER_TYPE_MAX
} FilterContainerType;

typedef struct Cluster Cluster;

typedef struct FilterContainer
{
    uint32_t mType : 1;
    uint32_t mGeometrySet: VISIBILITY_BUFFER_GEOMETRY_SET_BITS;
    uint32_t mMeshIndex: (32 - (1 + VISIBILITY_BUFFER_GEOMETRY_SET_BITS));
    uint32_t mInstanceIndex;
    uint32_t mFilterBatchCount;
    uint32_t mTriangleCount;
    Cluster* pClusters; // if valid points to an array of exactly mFilterBatchCount elements
} FilterContainer;

typedef struct FilterContainerDescriptor
{
    FilterContainerType mType;
    uint32_t            mMeshIndex;
    uint32_t            mIndexCount;
    uint32_t            mGeometrySet;

    uint32_t mBaseIndex;
    uint32_t mInstanceIndex;

    // Required data for FILTER_CONTAINER_TYPE_CLUSTER used for generating clusters
    const float3*   pPositions;
    const uint32_t* pIndices;
    bool            mIsTwoSided;

} FilterContainerDescriptor;

FORGE_RENDERER_API void addVBFilterContainer(FilterContainerDescriptor* pDesc, FilterContainer* pContainer);
FORGE_RENDERER_API void removeVBFilterContainer(FilterContainer* pContainer);

// In order to be able to use Async Compute with Animations we need to use Buffer Aliasing, which means we need to allocate the Heaps
// for the vertex buffers upfront and then use these to create the aliased buffers.
typedef struct PreSkinACVertexBuffersDesc
{
    uint32_t mNumBuffers;

    uint32_t mMaxStaticVertexCount;
    uint32_t mMaxPreSkinnedVertexCountPerFrame;
} PreSkinACVertexBuffersDesc;

// Buffer that alias memory in the main Vertex Buffer.
// This Buffer is used as input/output buffer during Pre-Skin pass on Async Compute queue.
// The reason we need this is because we cannot write to the main VertexBuffer that's used in the Graphics queue in parallel.
typedef struct PreSkinACAliasedBuffer
{
    uint64_t          mVBSize;      // Size for the main VB for this attribute (static geometry + pre-skin geometry)
    ResourcePlacement mVBPlacement; // Placement that allocates all memory used by the main Vertex Buffer

    uint64_t mOutputMemoryStartOffset; // Offset where the pre-skin data starts
    uint64_t mOutputMemorySize;        // Total size used by the pre-skin data, for all dataBuffers

    // Values filled of the array are [0, VisibilityBufferDesc::mNumBuffers)
    // Index i contains the buffers to be used when using dataBuffer i for rendering
    Buffer* pPreSkinBuffers[VISIBILITY_BUFFER_MAX_NUM_BUFFERS];
} PreSkinACAliasedBuffer;

typedef struct PreSkinACVertexBuffers
{
    // One Heap for all vertex buffers that contain animated attributes
    ResourceHeap* pHeap;

    PreSkinACAliasedBuffer mPositions;
    PreSkinACAliasedBuffer mNormals;

    // Buffers contain offsets to the first PreSkinned vertex (see outputBufferOffsets in pre_skin_vertexes.h.fsl)
    Buffer* pShaderOffsets[VISIBILITY_BUFFER_MAX_NUM_BUFFERS];
} PreSkinACVertexBuffers;

FORGE_RENDERER_API void initVBAsyncComputePreSkinVertexBuffers(Renderer* pRenderer, PreSkinACVertexBuffersDesc* pDesc,
                                                               PreSkinACVertexBuffers** pOut);
FORGE_RENDERER_API void exitVBAsyncComputePreSkinVertexBuffers(Renderer* pRenderer, PreSkinACVertexBuffers* pBuffers);

struct PreSkinContainer
{
    /**********************************/
    // Per mesh variables
    /**********************************/
    uint32_t mVertexCount;
    // Offset into the positions vertex buffer where positions for this mesh are stored
    uint32_t mVertexPositionOffset;
    // Offset into the joints/weights vertex buffers where joint/weight data is stored for this mesh.
    // This offset can be different to mVertexPositionOffset since all meshes in the scene have positions but not all have joints,
    // therefore the joints buffer is smaller.
    uint32_t mJointOffset;

    /**********************************/
    // Per instance variables
    /**********************************/
    // Offset into the matrix buffer that contains all matrixes for all animated objects,
    // jointIndex data read from the jointsBuffer will determine how far on this buffer we access
    uint32_t mJointMatrixOffset;
    // Base offset where to write skinned data, output will be stored in the range [mOutputVertexOffset, mOutputVertexOffset + mVertexCount)
    // Can be absolute offset into the vertex buffer used for output or relative to the start of the pre-skinned vertexes if
    // PreSkinBufferOffsets::vertexOffset is set to the pre-skinned base offset.
    uint32_t mOutputVertexOffset;
};

// Visibility buffer
typedef struct VisibilityBuffer
{
    Buffer** ppUncompactedDrawArgumentsBuffer;
    Buffer** ppFilteredIndirectDrawArgumentsBuffers;
    Buffer** ppFilteredIndexBuffer;
    Buffer** ppIndirectDataIndexBuffer;
} VisibilityBuffer;

typedef struct VisibilityBufferDesc
{
    // Number of images for double/triple buffering.
    // This is used to allocate buffers for data comming from the CPU.
    uint32_t mNumFrames;

    // If the app uses async compute for triangle filtering this should be the same as mNumFrames.
    // If the app doesn't use async compute, this should be 1 and TriangleFilteringPassDesc::mBufferIndex musht be always 0.
    // This is used to allocate GPU only buffers for data we genenrate on the GPU for Triangle Filtering and Batch Compaction stages.
    uint32_t mNumBuffers;

    uint32_t mIndirectElementCount;
    uint32_t mDrawArgCount;
    uint32_t mIndexCount;

    uint32_t mMaxDrawsIndirect;
    uint32_t mMaxPrimitivesPerDrawIndirect;
    uint32_t mFilterBatchCount; // Set as FILTER_BATCH_COUNT, defines the limit to the amount of triangle batches we can process on the GPU
    uint32_t mFilterBatchSize;  // Set as FILTER_BATCH_SIZE, this should be equal to the amount of triangles that will be processed in
                                // parallel by the triangle filter shader

    uint32_t mNumGeometrySets;
    uint32_t mNumViews;

    bool     mEnablePreSkinPass;
    uint32_t mPreSkinBatchSize;
    uint32_t mPreSkinBatchCount;

} VisibilityBufferDesc;

FORGE_RENDERER_API bool initVisibilityBuffer(Renderer* pRenderer, const VisibilityBufferDesc* pDesc, VisibilityBuffer** ppVisibilityBuffer);
FORGE_RENDERER_API void exitVisibilityBuffer(VisibilityBuffer* pVisibilityBuffer);

typedef struct PreSkinVertexesPassDesc
{
    uint64_t mGpuProfileToken;

    Pipeline*      pPipelinePreSkinVertexes;
    DescriptorSet* pDescriptorSetPreSkinVertexes;
    DescriptorSet* pDescriptorSetPreSkinVertexesPerFrame;

    Buffer**                ppPreSkinOutputVertexBuffers;
    const PreSkinContainer* pPreSkinContainers;
    uint32_t                mPreSkinOutputVertexBufferCount;
    uint32_t                mPreSkinContainerCount;

    uint32_t mFrameIndex;
} PreSkinVertexesPassDesc;

typedef struct PreSkinVertexesStats
{
    uint32_t mTotalVertexes;
    uint32_t mTotalVertexBatches;
    uint32_t mTotalShaderDispatches;
} PreSkinVertexesStats;

FORGE_RENDERER_API PreSkinVertexesStats cmdVisibilityBufferPreSkinVertexesPass(VisibilityBuffer* pVisibilityBuffer, Cmd* pCmd,
                                                                               PreSkinVertexesPassDesc* pDesc);

// TODO: reduce required items
typedef struct TriangleFilteringPassDesc
{
    uint32_t         mNumContainers;
    FilterContainer* pFilterContainers;
    bool             mCullClusters;
    uint32_t         mNumCullingViewports;
    vec3*            pViewportObjectSpace;

    Pipeline* pPipelineClearBuffers;
    Pipeline* pPipelineTriangleFiltering;
    Pipeline* pPipelineBatchCompaction;

    DescriptorSet* pDescriptorSetTriangleFiltering;
    DescriptorSet* pDescriptorSetTriangleFilteringPerFrame;
    DescriptorSet* pDescriptorSetClearBuffers;
    DescriptorSet* pDescriptorSetBatchCompaction;

    uint64_t mGpuProfileToken;
    uint32_t mClearThreadCount;

    uint32_t mFrameIndex;
    uint32_t mBuffersIndex; // See comment in VisibilityBufferDesc::mNumBuffers

} TriangleFilteringPassDesc;

typedef struct FilteringStats
{
    uint32_t mProcessedTriangleClusters; // Number of FilterContainers that used FILTER_CONTAINER_TYPE_CLUSTER
    uint32_t mCulledTriangleClusters;    // Number of clusters that were not sent to the GPU due to ClusterCulling. Always
                                         // (mCulledTriangleClusters <= mProcessedTriangleClusters)

    uint32_t mTotalProcessedTriangleBatches; // Total processed triangle batches
    uint32_t mTotalSubmittedTriangleBatches; // Number of batches submitted to the GPU -> (mTotalProcessedTriangleBatches -
                                             // mCulledTriangleClusters)
    uint32_t mTotalShaderDispatches;         // Number of triangle_filtering shader dispatches

    uint32_t mGeomsetDrawCounts[VISIBILITY_BUFFER_MAX_GEOMETRY_SETS]; // DrawCount per geometry set
    uint32_t mTotalDrawCount;                                         // Summed draw count for all elements in mGeomsetDrawCounts
} FilterPassStats;

FORGE_RENDERER_API FilteringStats cmdVisibilityBufferTriangleFilteringPass(VisibilityBuffer* pVisibilityBuffer, Cmd* pCmd,
                                                                           TriangleFilteringPassDesc* pDesc);
