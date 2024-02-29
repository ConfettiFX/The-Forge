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

#include "../../../Common_3/Application/Interfaces/IProfiler.h"
#include "../../../Common_3/Graphics/Interfaces/IGraphics.h"
#include "../../../Common_3/Utilities/Interfaces/ILog.h"
#include "../../../Common_3/Utilities/Interfaces/ITime.h"
#include "../Interfaces/IVisibilityBuffer.h"

#include "../../../Common_3/Utilities/RingBuffer.h"

#define NO_FSL_DEFINITIONS
#include "../VisibilityBuffer/Shaders/FSL/vb_structs.h.fsl"

#include "../../../Common_3/Utilities/Interfaces/IMemory.h"

/************************************************************************/
// Settings
/************************************************************************/
typedef struct VisibilityBufferSettings
{
    // This defines the amount of viewports that are going to be culled in parallel.
    uint32_t mNumViews;
    // Define different geometry sets (opaque and alpha tested geometry)
    uint32_t mNumGeomSets;

    uint32_t mFilterBatchCount;
    uint32_t mMaxDrawsIndirect;
    uint32_t mMaxDrawsIndirectElements;
    uint32_t mMaxPrimitivesPerDrawIndirect;
    uint32_t mNumFrames;
    uint32_t mNumBuffers;
    uint32_t mFilterBatchSize;
    uint32_t mNumFilterBatchChunks;

    uint32_t mUniformBufferAlignment;
    uint32_t mUseIndirectCommandBuffer : 1;
    uint32_t mUseIndirectRootConstant : 1;

    bool     mEnablePreSkinPass;
    uint32_t mPreSkinBatchSize;
    uint32_t mPreSkinBatchCount;
    uint32_t mNumPreSkinBatchChunks;
} VisibilityBufferSettings;

VisibilityBufferSettings gVBSettings;

/************************************************************************/
// Pre skin vertexes data
/************************************************************************/
typedef struct PreSkinBatchChunk
{
    uint32_t mCurrentBatchCount;
} PreSkinBatchChunk;

/************************************************************************/
// Triangle filtering data
/************************************************************************/
typedef struct FilterBatchChunk
{
    uint32_t mCurrentBatchCount;
} FilterBatchChunk;

GPURingBuffer gFilterBatchDataBuffer = {};
GPURingBuffer gPreSkinBatchDataBuffer = {};

typedef struct Cluster
{
    float3 aabbMin, aabbMax;
    float3 coneCenter, coneAxis;
    float  coneAngleCosine;
    float  distanceFromCamera;
    bool   valid;
} Cluster;

// Determines if the cluster can be safely culled performing quick cone-based test on the CPU.
// Since the triangle filtering kernel operates with 2 views in the same pass, this method must
// only cull those clusters that are not visible from ANY of the views (camera and shadow views).
bool cullCluster(const Cluster* cluster, vec3* pViewPositions, uint32_t numViews)
{
    // Invalid clusters can't be safely culled using the cone based test
    if (cluster->valid)
    {
        uint32_t visibility = 0;
        for (uint32_t i = 0; i < numViews; i++)
        {
            // We move camera position into object space
            vec3 testVec = normalize(pViewPositions[i] - f3Tov3(cluster->coneCenter));

            // Check if we are inside the cone
            if (dot(testVec, f3Tov3(cluster->coneAxis)) < cluster->coneAngleCosine)
                visibility |= (1 << i);
        }
        return (visibility == 0);
    }
    return false;
}

static void DispatchPreSkinVertexes(Cmd* pCmd, PreSkinBatchChunk* pBatchChunk, DescriptorSet* pDescriptorSetPreSkinVertexes,
                                    GPURingBufferOffset* ringBufferOffset, uint32_t batchDataOffsetBytes)
{
    ASSERT(pBatchChunk->mCurrentBatchCount > 0);
    ASSERT(ringBufferOffset);

    DescriptorDataRange range = { (uint32_t)ringBufferOffset->mOffset + batchDataOffsetBytes,
                                  gVBSettings.mPreSkinBatchCount * (uint32_t)sizeof(PreSkinBatchData) };
    DescriptorData      params[1] = {};
    params[0].pName = "batchData_rootcbv";
    params[0].pRanges = &range;
    params[0].ppBuffers = &ringBufferOffset->pBuffer;
    cmdBindDescriptorSetWithRootCbvs(pCmd, 0, pDescriptorSetPreSkinVertexes, 1, params);
    cmdDispatch(pCmd, pBatchChunk->mCurrentBatchCount, 1, 1);

    // Reset batch chunk to start adding vertexes to it
    pBatchChunk->mCurrentBatchCount = 0;
}

PreSkinVertexesStats cmdVisibilityBufferPreSkinVertexesPass(VisibilityBuffer* pVisibilityBuffer, Cmd* pCmd, PreSkinVertexesPassDesc* pDesc)
{
    ASSERT(gVBSettings.mEnablePreSkinPass);
    ASSERT(gVBSettings.mPreSkinBatchSize > 0);
    ASSERT(gVBSettings.mPreSkinBatchCount > 0);

    PreSkinVertexesStats stats = {};

    cmdBeginGpuTimestampQuery(pCmd, pDesc->mGpuProfileToken, "Pre Skin Vertexes");

    cmdBindPipeline(pCmd, pDesc->pPipelinePreSkinVertexes);
    cmdBindDescriptorSet(pCmd, 0, pDesc->pDescriptorSetPreSkinVertexes);
    cmdBindDescriptorSet(pCmd, pDesc->mFrameIndex, pDesc->pDescriptorSetPreSkinVertexesPerFrame);

    PreSkinBatchChunk batchChunk = {};
    batchChunk.mCurrentBatchCount = 0;

    PreSkinBatchData skinBatchData = {};

    const uint32_t      maxTotalPreSkinBatches = gVBSettings.mPreSkinBatchCount * gVBSettings.mNumPreSkinBatchChunks;
    const uint64_t      size = maxTotalPreSkinBatches * sizeof(PreSkinBatchData);
    GPURingBufferOffset offset = getGPURingBufferOffset(&gPreSkinBatchDataBuffer, (uint32_t)size, (uint32_t)size);
    BufferUpdateDesc    updateDesc = { offset.pBuffer, offset.mOffset };
    beginUpdateResource(&updateDesc);

    PreSkinBatchData* batches = (PreSkinBatchData*)updateDesc.pMappedData;
    PreSkinBatchData* origin = batches;

    for (uint32_t container = 0; container < pDesc->mPreSkinContainerCount; ++container)
    {
        const PreSkinContainer* pPreSkinContainer = &pDesc->pPreSkinContainers[container];

        const uint32_t batchCount = (pPreSkinContainer->mVertexCount + gVBSettings.mPreSkinBatchSize - 1) / gVBSettings.mPreSkinBatchSize;
        stats.mTotalVertexBatches += batchCount;

        for (uint32_t batch = 0; batch < batchCount; ++batch)
        {
            const uint32_t firstVertex = batch * gVBSettings.mFilterBatchSize;
            const uint32_t lastVertex = min(firstVertex + gVBSettings.mPreSkinBatchSize, pPreSkinContainer->mVertexCount);
            const uint32_t vertexesInBatch = lastVertex - firstVertex;
            stats.mTotalVertexes += vertexesInBatch;

            skinBatchData.outputVertexOffset = pPreSkinContainer->mOutputVertexOffset + firstVertex;
            skinBatchData.vertexCount = vertexesInBatch;
            skinBatchData.vertexPositionOffset = pPreSkinContainer->mVertexPositionOffset + firstVertex;
            skinBatchData.vertexJointsOffset = pPreSkinContainer->mJointOffset + firstVertex;
            skinBatchData.jointMatrixOffset = pPreSkinContainer->mJointMatrixOffset;

            memcpy(&batches[batchChunk.mCurrentBatchCount++], &skinBatchData, sizeof(skinBatchData));

            // If batcher are full dispatch and start a new one
            if (batchChunk.mCurrentBatchCount >= gVBSettings.mPreSkinBatchCount)
            {
                stats.mTotalShaderDispatches++;

                const uint32_t batchDataOffset = (uint32_t)(batches - origin);
                ASSERT(batchDataOffset + batchChunk.mCurrentBatchCount <= maxTotalPreSkinBatches);
                const uint32_t dispatchedBatchCount = batchChunk.mCurrentBatchCount;
                DispatchPreSkinVertexes(pCmd, &batchChunk, pDesc->pDescriptorSetPreSkinVertexes, &offset,
                                        batchDataOffset * sizeof(PreSkinBatchData));

                ASSERT(round_up(dispatchedBatchCount, gVBSettings.mUniformBufferAlignment) == dispatchedBatchCount &&
                       "batches pointer will end up with wrong alignment expected by the GPU");
                batches += dispatchedBatchCount;
            }
        }
    }

    if (batchChunk.mCurrentBatchCount > 0)
    {
        stats.mTotalShaderDispatches++;

        const uint32_t batchDataOffset = (uint32_t)(batches - origin);
        ASSERT(batchDataOffset + batchChunk.mCurrentBatchCount <= maxTotalPreSkinBatches);
        DispatchPreSkinVertexes(pCmd, &batchChunk, pDesc->pDescriptorSetPreSkinVertexes, &offset,
                                batchDataOffset * sizeof(PreSkinBatchData));
    }

    endUpdateResource(&updateDesc);
    cmdEndGpuTimestampQuery(pCmd, pDesc->mGpuProfileToken);

    return stats;
}

static void DispatchFilterTriangles(Cmd* pCmd, FilterBatchChunk* pBatchChunk, DescriptorSet* pDescriptorSetTriangleFiltering,
                                    GPURingBufferOffset* ringBufferOffset, uint32_t batchDataOffsetBytes)
{
    ASSERT(pBatchChunk->mCurrentBatchCount > 0);
    ASSERT(ringBufferOffset);

    DescriptorDataRange range = { (uint32_t)ringBufferOffset->mOffset + batchDataOffsetBytes,
                                  gVBSettings.mFilterBatchCount * (uint32_t)sizeof(FilterBatchData) };
    DescriptorData      params[1] = {};
    params[0].pName = "batchData_rootcbv";
    params[0].pRanges = &range;
    params[0].ppBuffers = &ringBufferOffset->pBuffer;
    cmdBindDescriptorSetWithRootCbvs(pCmd, 0, pDescriptorSetTriangleFiltering, 1, params);
    cmdDispatch(pCmd, pBatchChunk->mCurrentBatchCount, 1, 1);

    // Reset batch chunk to start adding triangles to it
    pBatchChunk->mCurrentBatchCount = 0;
}

// Executes the compute shader that performs triangle filtering on the GPU.
// This step performs different visibility tests per triangle to determine whether they
// potentially affect to the final image or not.
// The results of executing this shader are stored in:
// - pFilteredTriangles: list of triangle IDs that passed the culling tests
// - pIndirectDrawArguments: the vertexCount member of this structure is calculated in order to
// indicate the renderer the amount of vertices per batch to render.
FilteringStats cmdVisibilityBufferTriangleFilteringPass(VisibilityBuffer* pVisibilityBuffer, Cmd* pCmd, TriangleFilteringPassDesc* pDesc)
{
    ASSERT(pVisibilityBuffer);
    ASSERT(pDesc->mFrameIndex < gVBSettings.mNumFrames);
    ASSERT(pDesc->mBuffersIndex < gVBSettings.mNumBuffers);
    ASSERT(pDesc->pViewportObjectSpace || !pDesc->mCullClusters);

    if (gVBSettings.mUseIndirectCommandBuffer)
    {
        CommandSignature resetCmd = {};
        resetCmd.mDrawType = INDIRECT_COMMAND_BUFFER_RESET;
        resetCmd.mStride = 1;
        cmdExecuteIndirect(pCmd, &resetCmd, gVBSettings.mMaxDrawsIndirect * gVBSettings.mNumGeomSets * gVBSettings.mNumViews,
                           pVisibilityBuffer->ppFilteredIndirectDrawArgumentsBuffers[pDesc->mBuffersIndex], 0, NULL, 0);
    }

    FilterPassStats stats = {};

    BufferBarrier barrier[2] = {};

    /************************************************************************/
    // Barriers to transition uncompacted draw buffer to uav
    /************************************************************************/
    uint32_t bufferIndex = 0;
    barrier[bufferIndex++] = { pVisibilityBuffer->ppUncompactedDrawArgumentsBuffer[pDesc->mBuffersIndex],
                               RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, RESOURCE_STATE_UNORDERED_ACCESS };
    cmdResourceBarrier(pCmd, bufferIndex, barrier, 0, nullptr, 0, nullptr);
    /************************************************************************/
    // Clear previous indirect arguments
    /************************************************************************/
    cmdBeginGpuTimestampQuery(pCmd, pDesc->mGpuProfileToken, "Clear Buffers");
    cmdBindPipeline(pCmd, pDesc->pPipelineClearBuffers);
    cmdBindDescriptorSet(pCmd, pDesc->mBuffersIndex, pDesc->pDescriptorSetClearBuffers);
    uint32_t numGroups = (gVBSettings.mMaxDrawsIndirect / pDesc->mClearThreadCount) + 1;
    cmdDispatch(pCmd, numGroups, 1, 1);
    cmdEndGpuTimestampQuery(pCmd, pDesc->mGpuProfileToken);

    /************************************************************************/
    // Synchronization
    /************************************************************************/
    cmdBeginGpuTimestampQuery(pCmd, pDesc->mGpuProfileToken, "Clear Buffers Synchronization");
    bufferIndex = 0;
    barrier[bufferIndex++] = { pVisibilityBuffer->ppFilteredIndirectDrawArgumentsBuffers[pDesc->mBuffersIndex],
                               RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS };
    barrier[bufferIndex++] = { pVisibilityBuffer->ppUncompactedDrawArgumentsBuffer[pDesc->mBuffersIndex], RESOURCE_STATE_UNORDERED_ACCESS,
                               RESOURCE_STATE_UNORDERED_ACCESS };
    cmdResourceBarrier(pCmd, bufferIndex, barrier, 0, nullptr, 0, nullptr);

    cmdEndGpuTimestampQuery(pCmd, pDesc->mGpuProfileToken);

    /************************************************************************/
    // Run triangle filtering shader
    /************************************************************************/
    cmdBeginGpuTimestampQuery(pCmd, pDesc->mGpuProfileToken, "Filter Triangles");
    cmdBindPipeline(pCmd, pDesc->pPipelineTriangleFiltering);
    cmdBindDescriptorSet(pCmd, 0, pDesc->pDescriptorSetTriangleFiltering);
    cmdBindDescriptorSet(pCmd, pDesc->mFrameIndex, pDesc->pDescriptorSetTriangleFilteringPerFrame);

    const uint32_t      maxTotalFilterBatches = gVBSettings.mFilterBatchCount * gVBSettings.mNumFilterBatchChunks;
    const uint64_t      size = maxTotalFilterBatches * sizeof(FilterBatchData);
    GPURingBufferOffset offset = getGPURingBufferOffset(&gFilterBatchDataBuffer, (uint32_t)size, (uint32_t)size);
    BufferUpdateDesc    updateDesc = { offset.pBuffer, offset.mOffset };
    beginUpdateResource(&updateDesc);

    FilterBatchData* batches = (FilterBatchData*)updateDesc.pMappedData;
    FilterBatchData* origin = batches;

    uint32_t accumNumTriangles = 0;
    uint32_t accumNumTrianglesAtStartOfBatch = 0;
    uint32_t accumNumTrianglesOnBatch = 0;
    uint32_t batchStart = 0;

    FilterBatchChunk batchChunk = {};
    FilterBatchData  filterBatchData = {};

    for (uint32_t i = 0; i < pDesc->mNumContainers; ++i)
    {
        FilterContainer* pFilterContainer = &pDesc->pFilterContainers[i];

        for (uint32_t batch = 0; batch < pFilterContainer->mFilterBatchCount; ++batch)
        {
            ++stats.mTotalProcessedTriangleBatches;

            const uint32_t firstTriangle = batch * gVBSettings.mFilterBatchSize;
            const uint32_t lastTriangle = min(firstTriangle + gVBSettings.mFilterBatchSize, pFilterContainer->mTriangleCount);
            const uint32_t trianglesInBatch = lastTriangle - firstTriangle;

            if (pFilterContainer->mType == FILTER_CONTAINER_TYPE_CLUSTER)
            {
                ASSERT(pFilterContainer->pClusters);
                ++stats.mProcessedTriangleClusters;
                if (pDesc->mCullClusters &&
                    cullCluster(&pFilterContainer->pClusters[batch], pDesc->pViewportObjectSpace, pDesc->mNumCullingViewports))
                {
                    ++stats.mCulledTriangleClusters;
                    continue;
                }
            }

            ++stats.mTotalSubmittedTriangleBatches;

            // Fill GPU filter batch data
            filterBatchData.accumDrawIndex = stats.mTotalDrawCount;
            filterBatchData.faceCount = trianglesInBatch;
            filterBatchData.meshIndex = pFilterContainer->mMeshIndex;
            filterBatchData.geometrySet = pFilterContainer->mGeometrySet;
            filterBatchData.instanceDataIndex = pFilterContainer->mInstanceIndex;

            // Offset relative to the start of the mesh
            filterBatchData.indexOffset = firstTriangle * 3;
            filterBatchData.outputIndexOffset = accumNumTrianglesAtStartOfBatch * 3;
            filterBatchData.drawBatchStart = batchStart;

            memcpy(&batches[batchChunk.mCurrentBatchCount++], &filterBatchData, sizeof(FilterBatchData));

            accumNumTriangles += trianglesInBatch;
            accumNumTrianglesOnBatch += trianglesInBatch;

            // Check to see if we filled the batch, two options for filled batch:
            //    - We filled all FilterBatchData structs we can process in on one triangle filtering compute shader dispatch
            //    - We filled enough triangles that if we put more into this dispatch we won't be able to store their PrimitiveID in the
            //      Visibility Buffer because we don't have enough bits to represent them. PrimitiveID would be clamped and geometry would
            //      flicker because we would access incorrect primitive data during the shading stage.
            if (batchChunk.mCurrentBatchCount >= gVBSettings.mFilterBatchCount ||
                accumNumTrianglesOnBatch + gVBSettings.mFilterBatchSize > gVBSettings.mMaxPrimitivesPerDrawIndirect)
            {
                ++stats.mTotalDrawCount;

                ASSERT(pFilterContainer->mGeometrySet < TF_ARRAY_COUNT(stats.mGeomsetDrawCounts));
                ++stats.mGeomsetDrawCounts[pFilterContainer->mGeometrySet];

                uint32_t batchCount = batchChunk.mCurrentBatchCount;

                // run the triangle filtering and switch to the next small batch chunk
                if (batchChunk.mCurrentBatchCount > 0)
                {
                    stats.mTotalShaderDispatches++;

                    const uint32_t batchDataOffset = (uint32_t)(batches - origin);
                    ASSERT(batchDataOffset + batchChunk.mCurrentBatchCount <= maxTotalFilterBatches);
                    DispatchFilterTriangles(pCmd, &batchChunk, pDesc->pDescriptorSetTriangleFiltering, &offset,
                                            batchDataOffset * sizeof(FilterBatchData));

                    accumNumTrianglesOnBatch = 0;
                }

                // Make sure we advance to a proper aligned batch count so that the next dispatch call has the memory properly aligned.
                // (this can happen if we reached gVBSettings.mMaxPrimitivesPerDrawIndirect before gVBSettings.mBatchCount)
                const uint32_t alignedBatchCount =
                    round_up(batchCount, gVBSettings.mUniformBufferAlignment >= sizeof(FilterBatchData)
                                             ? gVBSettings.mUniformBufferAlignment / (uint32_t)sizeof(FilterBatchData)
                                             : 1);
                batches += alignedBatchCount;

                batchStart = 0;
                accumNumTrianglesAtStartOfBatch = accumNumTriangles;
            }
        }

        // end of that mesh, set it up so we can add the next mesh to this culling batch
        if (batchChunk.mCurrentBatchCount > 0)
        {
            ++stats.mTotalDrawCount;

            ASSERT(pFilterContainer->mGeometrySet < TF_ARRAY_COUNT(stats.mGeomsetDrawCounts));
            ++stats.mGeomsetDrawCounts[pFilterContainer->mGeometrySet];

            batchStart = batchChunk.mCurrentBatchCount;
            accumNumTrianglesAtStartOfBatch = accumNumTriangles;
        }
    }

    if (batchChunk.mCurrentBatchCount > 0)
    {
        stats.mTotalShaderDispatches++;

        const uint32_t batchDataOffset = (uint32_t)(batches - origin);
        ASSERT(batchDataOffset + batchChunk.mCurrentBatchCount <= maxTotalFilterBatches);
        DispatchFilterTriangles(pCmd, &batchChunk, pDesc->pDescriptorSetTriangleFiltering, &offset,
                                batchDataOffset * sizeof(FilterBatchData));
    }

    endUpdateResource(&updateDesc);

    cmdEndGpuTimestampQuery(pCmd, pDesc->mGpuProfileToken);

    /************************************************************************/
    // Synchronization
    /************************************************************************/
    bufferIndex = 0;
    barrier[bufferIndex++] = { pVisibilityBuffer->ppUncompactedDrawArgumentsBuffer[pDesc->mBuffersIndex], RESOURCE_STATE_UNORDERED_ACCESS,
                               RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE };
    cmdResourceBarrier(pCmd, bufferIndex, barrier, 0, nullptr, 0, nullptr);

    /************************************************************************/
    // Batch compaction
    /************************************************************************/
    cmdBeginGpuTimestampQuery(pCmd, pDesc->mGpuProfileToken, "Batch Compaction");
    cmdBindPipeline(pCmd, pDesc->pPipelineBatchCompaction);
    cmdBindDescriptorSet(pCmd, pDesc->mBuffersIndex, pDesc->pDescriptorSetBatchCompaction);
    numGroups = (gVBSettings.mMaxDrawsIndirect / pDesc->mClearThreadCount) + 1;
    cmdDispatch(pCmd, numGroups, 1, 1);
    cmdEndGpuTimestampQuery(pCmd, pDesc->mGpuProfileToken);
    /************************************************************************/
    /************************************************************************/
    return stats;
}

// UNUSED
/************************************************************************/
// Culling intrinsic data
/************************************************************************/
// const uint32_t pdep_lut[8] = { 0x0, 0x1, 0x4, 0x5, 0x10, 0x11, 0x14, 0x15 };

// static inline int genClipMask(__m128 v)
//{
//	//this checks a vertex against the 6 planes, and stores if they are inside
//	// or outside of the plane
//
//	//w contains the w component of the vector in all 4 slots
//	const __m128 w0 = _mm_shuffle_ps(v, v, _MM_SHUFFLE(3, 3, 3, 3));
//	const __m128 w1 = _mm_shuffle_ps(v, _mm_setzero_ps(), _MM_SHUFFLE(3, 3, 3, 3));
//
//	//subtract the vector from w, and store in a
//	const __m128 a = _mm_sub_ps(w0, v);
//	//add the vector to w, and store in b
//	const __m128 b = _mm_add_ps(w1, v);
//
//	//compare if a and b are less than zero,
//	// and store the result in fmaska, and fmaskk
//	const __m128 fmaska = _mm_cmplt_ps(a, _mm_setzero_ps());
//	const __m128 fmaskb = _mm_cmplt_ps(b, _mm_setzero_ps());
//
//	//convert those masks to integers, and spread the bits using pdep
//	//const int maska = _pdep_u32(_mm_movemask_ps(fmaska), 0x55);
//	//const int maskb = _pdep_u32(_mm_movemask_ps(fmaskb), 0xAA);
//	const int maska = pdep_lut[(_mm_movemask_ps(fmaska) & 0x7)];
//	const int maskb = pdep_lut[(_mm_movemask_ps(fmaskb) & 0x7)] << 1;
//
//	//or the masks together and and the together with all bits set to 1
//	// NOTE only the bits 0x3f are actually used
//	return (maska | maskb) & 0x3f;
// }

// static inline uint32_t genClipMask(float4 f)
//{
//   uint32_t result = 0;
//
//   //X
//   if (f.x <= f.w)  result |=  0x1;
//   if (f.x >= -f.w) result |=  0x2;
//
//   //Y
//   if (f.y <= f.w)  result |=  0x4;
//   if (f.y >= -f.w) result |=  0x8;
//
//   //Z
//   if (f.z <= f.w)  result |= 0x10;
//   if (f.z >= 0)   result |= 0x20;
//   return result;
// }

// UNUSED
// void sortClusters(Cluster** clusters, uint32_t len)
//{
//	struct StackItem
//	{
//		Cluster** a;
//		uint32_t  l;
//	};
//	StackItem stack[512];
//	int32_t   stackidx = 0;
//
//	Cluster** current_a = clusters;
//	uint32_t  current_l = len;
//
//	for (;;)
//	{
//		Cluster* pivot = current_a[current_l / 2];
//
//		int32_t i, j;
//		for (i = 0, j = current_l - 1;; ++i, --j)
//		{
//			while (current_a[i]->distanceFromCamera < pivot->distanceFromCamera)
//				i++;
//			while (current_a[j]->distanceFromCamera > pivot->distanceFromCamera)
//				j--;
//
//			if (i >= j)
//				break;
//
//			Cluster* temp = current_a[i];
//			current_a[i] = current_a[j];
//			current_a[j] = temp;
//		}
//
//		if (i > 1)
//		{
//			stack[stackidx].a = current_a;
//			stack[stackidx++].l = i;
//		}
//		if (current_l - i > 1)
//		{
//			stack[stackidx].a = current_a + i;
//			stack[stackidx++].l = current_l - i;
//		}
//
//		if (stackidx == 0)
//			break;
//
//		--stackidx;
//		current_a = stack[stackidx].a;
//		current_l = stack[stackidx].l;
//	}
// }

// Compute an array of clusters from the mesh vertices. Clusters are sub batches of the original mesh limited in number
// for more efficient CPU / GPU culling. CPU culling operates per cluster, while GPU culling operates per triangle for
// all the clusters that passed the CPU test.
void createClusters(FilterContainer* pContainer, FilterContainerDescriptor* pDesc)
{
    ASSERT(pContainer);
    ASSERT(pDesc->pIndices);
    ASSERT(pDesc->pPositions);
    ASSERT(!pContainer->pClusters);

#define makeVec3(v) (vec3((v).x, (v).y, (v).z))

    // 12 KiB stack space
    struct Triangle
    {
        vec3 vtx[3];
    };

    Triangle* triangleCache = (Triangle*)tf_calloc(gVBSettings.mFilterBatchSize * 3, sizeof(Triangle));

    pContainer->pClusters = (Cluster*)tf_calloc(pContainer->mFilterBatchCount, sizeof(Cluster));

    for (uint32_t i = 0; i < pContainer->mFilterBatchCount; ++i)
    {
        const uint32_t clusterStart = i * gVBSettings.mFilterBatchSize;
        const uint32_t clusterEnd = min(clusterStart + gVBSettings.mFilterBatchSize, pContainer->mTriangleCount);

        const uint32_t clusterTriangleCount = clusterEnd - clusterStart;

        // Load all triangles into our local cache
        for (uint32_t triangleIndex = clusterStart; triangleIndex < clusterEnd; ++triangleIndex)
        {
            triangleCache[triangleIndex - clusterStart].vtx[0] =
                makeVec3(pDesc->pPositions[pDesc->pIndices[pDesc->mBaseIndex + triangleIndex * 3]]);
            triangleCache[triangleIndex - clusterStart].vtx[1] =
                makeVec3(pDesc->pPositions[pDesc->pIndices[pDesc->mBaseIndex + triangleIndex * 3 + 1]]);
            triangleCache[triangleIndex - clusterStart].vtx[2] =
                makeVec3(pDesc->pPositions[pDesc->pIndices[pDesc->mBaseIndex + triangleIndex * 3 + 2]]);
        }

        vec3 aabbMin = vec3(INFINITY, INFINITY, INFINITY);
        vec3 aabbMax = -aabbMin;

        vec3 coneAxis = vec3(0, 0, 0);

        for (uint32_t triangleIndex = 0; triangleIndex < clusterTriangleCount; ++triangleIndex)
        {
            const auto& triangle = triangleCache[triangleIndex];
            for (uint32_t j = 0; j < 3; ++j)
            {
                aabbMin = minPerElem(aabbMin, triangle.vtx[j]);
                aabbMax = maxPerElem(aabbMax, triangle.vtx[j]);
            }

            vec3 triangleNormal = cross(triangle.vtx[1] - triangle.vtx[0], triangle.vtx[2] - triangle.vtx[0]);

            if (!(triangleNormal == vec3(0, 0, 0)))
                triangleNormal = normalize(triangleNormal);

            // coneAxis = DirectX::XMVectorAdd(coneAxis, DirectX::XMVectorNegate(triangleNormal));
            coneAxis = coneAxis - triangleNormal;
        }

        // This is the cosine of the cone opening angle - 1 means it's 0?,
        // we're minimizing this value (at 0, it would mean the cone is 90?
        // open)
        float coneOpening = 1;
        // dont cull two sided meshes
        bool  validCluster = !pDesc->mIsTwoSided;

        const vec3 center = (aabbMin + aabbMax) / 2;
        // if the axis is 0 then we have a invalid cluster
        if (coneAxis == vec3(0, 0, 0))
            validCluster = false;

        coneAxis = normalize(coneAxis);

        float t = -INFINITY;

        // cant find a cluster for 2 sided objects
        if (validCluster)
        {
            // We nee a second pass to find the intersection of the line center + t * coneAxis with the plane defined by each triangle
            for (uint32_t triangleIndex = 0; triangleIndex < clusterTriangleCount; ++triangleIndex)
            {
                const Triangle& triangle = triangleCache[triangleIndex];
                // Compute the triangle plane from the three vertices

                const vec3 triangleNormal = normalize(cross(triangle.vtx[1] - triangle.vtx[0], triangle.vtx[2] - triangle.vtx[0]));

                const float directionalPart = dot(coneAxis, -triangleNormal);

                if (directionalPart <= 0) // AMD BUG?: changed to <= 0 because directionalPart is used to divide a quantity
                {
                    // No solution for this cluster - at least two triangles are facing each other
                    validCluster = false;
                    break;
                }

                // We need to intersect the plane with our cone ray which is center + t * coneAxis, and find the max
                // t along the cone ray (which points into the empty space) See:
                // https://en.wikipedia.org/wiki/Line%E2%80%93plane_intersection
                const float td = dot(center - triangle.vtx[0], triangleNormal) / -directionalPart;

                t = max(t, td);

                coneOpening = min(coneOpening, directionalPart);
            }
        }

        pContainer->pClusters[i].aabbMax = v3ToF3(aabbMax);
        pContainer->pClusters[i].aabbMin = v3ToF3(aabbMin);

        pContainer->pClusters[i].coneAngleCosine = sqrtf(1 - coneOpening * coneOpening);
        pContainer->pClusters[i].coneCenter = v3ToF3(center + coneAxis * t);
        pContainer->pClusters[i].coneAxis = v3ToF3(coneAxis);

        // #if AMD_GEOMETRY_FX_ENABLE_CLUSTER_CENTER_SAFETY_CHECK
        //  If distance of coneCenter to the bounding box center is more than 16x the bounding box extent, the cluster is also invalid
        //  This is mostly a safety measure - if triangles are nearly parallel to coneAxis, t may become very large and unstable
        const float aabbSize = length(aabbMax - aabbMin);
        const float coneCenterToCenterDistance = length(f3Tov3(pContainer->pClusters[i].coneCenter) - center);

        if (coneCenterToCenterDistance > (16 * aabbSize))
            validCluster = false;

        pContainer->pClusters[i].valid = validCluster;
    }

    tf_free(triangleCache);
}

void addVBFilterContainer(FilterContainerDescriptor* pDesc, FilterContainer* pContainer)
{
    ASSERT(pDesc);
    ASSERT(pContainer);
    ASSERT(pDesc->mIndexCount > 0);
    ASSERT(gVBSettings.mFilterBatchSize > 0 && "Visibility Buffer not initialized yet!");
    ASSERT(pDesc->mGeometrySet < gVBSettings.mNumGeomSets);

    COMPILE_ASSERT(FILTER_CONTAINER_TYPE_MAX == 2 &&
                   "Make sure to update bits used for FilterContainer::mType and other fields when adding a new FilterContainerType");

    pContainer->mType = pDesc->mType;
    pContainer->mGeometrySet = pDesc->mGeometrySet;
    pContainer->mMeshIndex = pDesc->mMeshIndex;
    pContainer->mTriangleCount = pDesc->mIndexCount / 3;
    pContainer->mFilterBatchCount = (pContainer->mTriangleCount + gVBSettings.mFilterBatchSize - 1) / gVBSettings.mFilterBatchSize;
    pContainer->mInstanceIndex = pDesc->mInstanceIndex;
    pContainer->pClusters = NULL;

    if (pContainer->mType == FILTER_CONTAINER_TYPE_CLUSTER)
    {
        createClusters(pContainer, pDesc);
    }
}

void removeVBFilterContainer(FilterContainer* pContainer)
{
    if (pContainer->mType == FILTER_CONTAINER_TYPE_CLUSTER)
    {
        tf_free(pContainer->pClusters);
        pContainer->pClusters = NULL;
    }
}

static BufferLoadDesc MainVertexBufferLoadDesc(PreSkinACVertexBuffersDesc* pDesc, uint32_t structStride)
{
    BufferLoadDesc vertexBufferDesc = {};
    vertexBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER | (DESCRIPTOR_TYPE_BUFFER_RAW | DESCRIPTOR_TYPE_RW_BUFFER_RAW);
    vertexBufferDesc.mDesc.mSize =
        (pDesc->mMaxStaticVertexCount + pDesc->mMaxPreSkinnedVertexCountPerFrame * pDesc->mNumBuffers) * structStride;
    vertexBufferDesc.mDesc.mElementCount = (uint32_t)(vertexBufferDesc.mDesc.mSize / sizeof(uint32_t));
    vertexBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
    vertexBufferDesc.mDesc.mStructStride = structStride;
    return vertexBufferDesc;
}

static void addPreSkinACAliasedBuffer(Renderer* pRenderer, PreSkinACVertexBuffersDesc* pDesc, ResourceHeap* pHeap, uint64_t heapOffset,
                                      uint64_t vbSize, PreSkinACAliasedBuffer* pOut, uint32_t attrStride, const char* pPreSkinBufferName)
{
    // Placement for the VertexBuffers
    pOut->mVBSize = vbSize;
    pOut->mVBPlacement.pHeap = pHeap;
    pOut->mVBPlacement.mOffset = heapOffset;

    // Aliased Pre-Skin Output Buffers span the entire Vertex Buffer, this makes code simpler and we just have to bind one buffer in the
    // shader
    ResourcePlacement preSkinnedVertexPlacement = {};
    preSkinnedVertexPlacement.pHeap = pHeap;
    preSkinnedVertexPlacement.mOffset = heapOffset;

    BufferLoadDesc bufferDesc = {};
    bufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER_RAW | DESCRIPTOR_TYPE_RW_BUFFER_RAW;
    bufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
    bufferDesc.mDesc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;
    bufferDesc.mDesc.mElementCount = (uint32_t)(vbSize / sizeof(uint32_t));
    bufferDesc.mDesc.mSize = vbSize;
    bufferDesc.mDesc.mStructStride = attrStride;
    bufferDesc.mDesc.pName = pPreSkinBufferName;
    bufferDesc.mDesc.pPlacement = &preSkinnedVertexPlacement;
    for (uint32_t i = 0; i < pDesc->mNumBuffers; ++i)
    {
        bufferDesc.ppBuffer = &pOut->pPreSkinBuffers[i];
        addResource(&bufferDesc, NULL);

        const uint64_t shaderOffsetBytes = attrStride * (pDesc->mMaxStaticVertexCount + pDesc->mMaxPreSkinnedVertexCountPerFrame * i);

        // We'll need to reserver this memory in the BufferChunkAllocator so that it's not used for meshes
        {
            if (i == 0)
                pOut->mOutputMemoryStartOffset = shaderOffsetBytes;

            const uint64_t thisBufferOutputMemorySize = shaderOffsetBytes + pDesc->mMaxPreSkinnedVertexCountPerFrame * attrStride;
            pOut->mOutputMemorySize = thisBufferOutputMemorySize - pOut->mOutputMemoryStartOffset;
            ASSERT(pOut->mOutputMemoryStartOffset + pOut->mOutputMemorySize <= vbSize);
        }
    }
}

static void removePreSkinACAliasedBuffer(Renderer* pRenderer, PreSkinACAliasedBuffer* pBuffers)
{
    for (uint32_t i = 0; i < gVBSettings.mNumBuffers; ++i)
        removeResource(pBuffers->pPreSkinBuffers[i]);
}

void initVBAsyncComputePreSkinVertexBuffers(Renderer* pRenderer, PreSkinACVertexBuffersDesc* pDesc, PreSkinACVertexBuffers** pOut)
{
    ASSERT(pDesc);
    ASSERT(pOut);
    ASSERT(pDesc->mNumBuffers > 0);
    ASSERT(pDesc->mMaxStaticVertexCount > 0);
    ASSERT(pDesc->mMaxPreSkinnedVertexCountPerFrame > 0);

    PreSkinACVertexBuffers* pBuffers = (PreSkinACVertexBuffers*)tf_calloc(1, sizeof(PreSkinACVertexBuffers));

    ResourceSizeAlign sizeAligns[3] = {};

    // Create Heap to store all the main Vertex Buffers for skinned attributes.
    // This memory includes extra memory for pre-skinned vertex output.
    {
        // Make sure we allocate enoough memory to hold the VB for all atributes with correct alignments
        const BufferLoadDesc VBDescs[] = {
            MainVertexBufferLoadDesc(pDesc, sizeof(float3)),   // Position
            MainVertexBufferLoadDesc(pDesc, sizeof(uint32_t)), // Normal
        };

        getResourceSizeAlign(&VBDescs[0], &sizeAligns[0]);
        getResourceSizeAlign(&VBDescs[1], &sizeAligns[1]);

        // TODO: There are issues when buffer size is greater than uint32_max, the value is truncated in D3D12 by one call to
        // ID3D12Device::GetCopyableFootprints.
        //       For now 4Gb of vertex buffer memory per attribute seems more than enough
        ASSERT(sizeAligns[0].mSize <= UINT32_MAX);
        ASSERT(sizeAligns[1].mSize <= UINT32_MAX);

        const uint64_t totalRequiredSize = sizeAligns[0].mSize + sizeAligns[0].mAlignment + sizeAligns[1].mAlignment + sizeAligns[1].mSize;

        ResourceHeapDesc desc = {};
        desc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
        desc.mDescriptors = DESCRIPTOR_TYPE_BUFFER | (DESCRIPTOR_TYPE_BUFFER_RAW | DESCRIPTOR_TYPE_RW_BUFFER_RAW);
        desc.mFlags = RESOURCE_HEAP_FLAG_ALLOW_ONLY_BUFFERS;

        desc.mAlignment = sizeAligns[0].mAlignment; // Align it to the alignment of the first vertex buffer
        desc.mSize = totalRequiredSize;
        desc.pName = "Skinned VertexBuffer Heap";
        addResourceHeap(pRenderer, &desc, &pBuffers->pHeap);
    }

    const uint64_t posOffset = 0; // Heap is already aligned to the alignment of this vertex buffer
    const uint64_t normalOffset = round_up_64(posOffset + sizeAligns[0].mSize, sizeAligns[1].mAlignment);

    addPreSkinACAliasedBuffer(pRenderer, pDesc, pBuffers->pHeap, posOffset, sizeAligns[0].mSize, &pBuffers->mPositions, sizeof(float3),
                              "PreSkinBuffer Positions");
    addPreSkinACAliasedBuffer(pRenderer, pDesc, pBuffers->pHeap, normalOffset, sizeAligns[1].mSize, &pBuffers->mNormals, sizeof(uint32_t),
                              "PreSkinBuffer Normals");

    static PreSkinBufferOffsets offsets[VISIBILITY_BUFFER_MAX_NUM_BUFFERS] = {};

    for (uint32_t i = 0; i < pDesc->mNumBuffers; ++i)
    {
        const uint64_t vertexOffset = (uint64_t)pDesc->mMaxStaticVertexCount + (uint64_t)pDesc->mMaxPreSkinnedVertexCountPerFrame * i;
        ASSERT(vertexOffset <= UINT32_MAX &&
               "If we support vertex buffers of more than UINT32_MAX bytes we need to either pass this variable to the shader as uint64 or "
               "place the Pre-Skin buffers at the beggining so that the offset is within limits.");

        offsets[i].vertexOffset = (uint32_t)vertexOffset;

        BufferLoadDesc desc = {};
        desc.pData = &offsets[i];
        desc.ppBuffer = &pBuffers->pShaderOffsets[i];
        desc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER | DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        desc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
        desc.mDesc.mElementCount = 1;
        desc.mDesc.mStructStride = sizeof(PreSkinBufferOffsets);
        desc.mDesc.mSize = desc.mDesc.mElementCount * desc.mDesc.mStructStride;
        desc.mDesc.pName = "PreSkinACShaderBufferOffsets";
        addResource(&desc, NULL);
    }

    *pOut = pBuffers;
}

void exitVBAsyncComputePreSkinVertexBuffers(Renderer* pRenderer, PreSkinACVertexBuffers* pBuffers)
{
    ASSERT(pRenderer);
    ASSERT(pBuffers);

    for (uint32_t i = 0; i < gVBSettings.mNumBuffers; ++i)
        removeResource(pBuffers->pShaderOffsets[i]);

    removePreSkinACAliasedBuffer(pRenderer, &pBuffers->mPositions);
    removePreSkinACAliasedBuffer(pRenderer, &pBuffers->mNormals);

    removeResourceHeap(pRenderer, pBuffers->pHeap);

    tf_free(pBuffers);
}

//************************************************************************/
//
//************************************************************************/
bool initVisibilityBuffer(Renderer* pRenderer, const VisibilityBufferDesc* pDesc, VisibilityBuffer** ppVisibilityBuffer)
{
    ASSERT(ppVisibilityBuffer);
    ASSERT(pDesc);
    ASSERT(pDesc->mMaxPrimitivesPerDrawIndirect > 0);
    ASSERT(pDesc->mFilterBatchSize > 0);
    ASSERT(pDesc->mMaxPrimitivesPerDrawIndirect >= pDesc->mFilterBatchSize);
    ASSERT(pDesc->mMaxDrawsIndirect > 0);
    ASSERT(pDesc->mNumFrames > 0);
    ASSERT(pDesc->mNumBuffers > 0);
    ASSERT(pDesc->mNumGeometrySets > 0);
    ASSERT(pDesc->mNumGeometrySets <= VISIBILITY_BUFFER_MAX_GEOMETRY_SETS &&
           "Please update the configuration macro named VISIBILITY_BUFFER_MAX_GEOMETRY_SETS to be of the proper value");
    ASSERT(pDesc->mNumViews > 0);
    ASSERT(pDesc->mFilterBatchCount > 0);
    ASSERT(pDesc->mIndirectElementCount > 0);

    VisibilityBuffer* pVisibilityBuffer = (VisibilityBuffer*)tf_malloc(sizeof(VisibilityBuffer));
    gVBSettings.mUseIndirectCommandBuffer = pRenderer->pGpu->mSettings.mIndirectCommandBuffer;
    gVBSettings.mUseIndirectRootConstant = pRenderer->pGpu->mSettings.mIndirectRootConstant;
    gVBSettings.mUniformBufferAlignment = pRenderer->pGpu->mSettings.mUniformBufferAlignment;
    gVBSettings.mNumGeomSets = pDesc->mNumGeometrySets;
    gVBSettings.mNumViews = pDesc->mNumViews;
    gVBSettings.mFilterBatchCount = pDesc->mFilterBatchCount;
    gVBSettings.mMaxPrimitivesPerDrawIndirect = pDesc->mMaxPrimitivesPerDrawIndirect;
    gVBSettings.mMaxDrawsIndirect = pDesc->mMaxDrawsIndirect;
    gVBSettings.mMaxDrawsIndirectElements = pDesc->mIndirectElementCount;
    gVBSettings.mNumFrames = pDesc->mNumFrames;
    gVBSettings.mNumBuffers = pDesc->mNumBuffers;
    gVBSettings.mFilterBatchSize = pDesc->mFilterBatchSize;
    gVBSettings.mNumFilterBatchChunks = max(1U, 512U / pDesc->mFilterBatchSize) * 16U; // number of batch chunks for triangle filtering

    if (pDesc->mEnablePreSkinPass)
    {
        ASSERT(pDesc->mPreSkinBatchSize > 0);
        ASSERT(pDesc->mPreSkinBatchCount > 0);

        gVBSettings.mEnablePreSkinPass = pDesc->mEnablePreSkinPass;
        gVBSettings.mPreSkinBatchSize = pDesc->mPreSkinBatchSize;
        gVBSettings.mPreSkinBatchCount = pDesc->mPreSkinBatchCount;
        gVBSettings.mNumPreSkinBatchChunks =
            max(1U, 512U / pDesc->mPreSkinBatchSize) * 16U; // number of batch chunks for vertex pre skinning
    }

    // Create uncompacted draw argument buffers
    pVisibilityBuffer->ppUncompactedDrawArgumentsBuffer = (Buffer**)tf_malloc(sizeof(Buffer*) * pDesc->mNumBuffers);
    BufferLoadDesc uncompactedDesc = {};
    uncompactedDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER | DESCRIPTOR_TYPE_RW_BUFFER;
    uncompactedDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
    uncompactedDesc.mDesc.mElementCount = gVBSettings.mMaxDrawsIndirect * gVBSettings.mNumViews;
    uncompactedDesc.mDesc.mStructStride = sizeof(UncompactedDrawArguments);
    uncompactedDesc.mDesc.mSize = uncompactedDesc.mDesc.mElementCount * uncompactedDesc.mDesc.mStructStride;
    uncompactedDesc.mDesc.mStartState = RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    uncompactedDesc.mDesc.pName = "UncompactedDrawArgumentsBuffer";
    uncompactedDesc.pData = NULL;

    for (uint32_t i = 0; i < pDesc->mNumBuffers; ++i)
    {
        uncompactedDesc.ppBuffer = &pVisibilityBuffer->ppUncompactedDrawArgumentsBuffer[i];
        addResource(&uncompactedDesc, NULL);
    }

    // Create indirect argument buffers
    pVisibilityBuffer->ppFilteredIndirectDrawArgumentsBuffers = (Buffer**)tf_malloc(sizeof(Buffer*) * pDesc->mNumBuffers);
    const uint32_t indirectArgumentsBufferElementCount =
        gVBSettings.mMaxDrawsIndirect * pDesc->mIndirectElementCount * gVBSettings.mNumGeomSets * gVBSettings.mNumViews;
    const uint32_t argOffset = pRenderer->pGpu->mSettings.mIndirectRootConstant ? 1 : 0;
    uint32_t*      indirectArgsDwords = (uint32_t*)tf_malloc(indirectArgumentsBufferElementCount * sizeof(uint32_t));
    memset(indirectArgsDwords, 0, indirectArgumentsBufferElementCount * sizeof(uint32_t));
    for (uint32_t view = 0; view < gVBSettings.mNumViews; ++view)
    {
        for (uint32_t geomset = 0; geomset < gVBSettings.mNumGeomSets; ++geomset)
        {
            for (uint32_t draw = 0; draw < gVBSettings.mMaxDrawsIndirect; ++draw)
            {
                uint32_t indirectArgsDwordsIndex =
                    ((view * gVBSettings.mNumGeomSets) + geomset) * gVBSettings.mMaxDrawsIndirect * pDesc->mIndirectElementCount +
                    draw * pDesc->mIndirectElementCount + argOffset;
                IndirectDrawIndexArguments* arg = (IndirectDrawIndexArguments*)&indirectArgsDwords[indirectArgsDwordsIndex];
                if (pRenderer->pGpu->mSettings.mIndirectRootConstant)
                {
                    indirectArgsDwords[indirectArgsDwordsIndex - argOffset] = draw;
                }
                else
                {
                    // No drawId or gl_DrawId but instance id works as expected so use that as the draw id
                    arg->mStartInstance = draw;
                }
                arg->mInstanceCount = (draw < pDesc->mDrawArgCount) ? 1 : 0;
            }
        }
    }

    BufferLoadDesc filterIndirectDesc = {};
    filterIndirectDesc.mDesc.mDescriptors =
        DESCRIPTOR_TYPE_INDIRECT_BUFFER | DESCRIPTOR_TYPE_BUFFER | DESCRIPTOR_TYPE_RW_BUFFER | DESCRIPTOR_TYPE_INDIRECT_COMMAND_BUFFER;
    filterIndirectDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
    filterIndirectDesc.mDesc.mElementCount = indirectArgumentsBufferElementCount;
    filterIndirectDesc.mDesc.mStructStride = sizeof(uint32_t);
    filterIndirectDesc.mDesc.mSize = filterIndirectDesc.mDesc.mElementCount * filterIndirectDesc.mDesc.mStructStride;
    filterIndirectDesc.mDesc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;
    filterIndirectDesc.mDesc.mICBDrawType = INDIRECT_DRAW_INDEX;
    filterIndirectDesc.mDesc.mICBMaxCommandCount = gVBSettings.mMaxDrawsIndirect * gVBSettings.mNumViews * gVBSettings.mNumGeomSets;
    filterIndirectDesc.mDesc.pName = "FilteredIndirectDrawArgumentsBuffer";
    filterIndirectDesc.pData = indirectArgsDwords;
    SyncToken token = {};
    for (uint32_t i = 0; i < pDesc->mNumBuffers; ++i)
    {
        filterIndirectDesc.ppBuffer = &pVisibilityBuffer->ppFilteredIndirectDrawArgumentsBuffers[i];
        addResource(&filterIndirectDesc, &token);
    }

    // Create filteredIndexBuffers
    pVisibilityBuffer->ppFilteredIndexBuffer = (Buffer**)tf_malloc(sizeof(Buffer*) * pDesc->mNumBuffers * pDesc->mNumViews);

    BufferLoadDesc filterIbDesc = {};
    filterIbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_INDEX_BUFFER | DESCRIPTOR_TYPE_BUFFER_RAW | DESCRIPTOR_TYPE_RW_BUFFER_RAW;
    filterIbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
    filterIbDesc.mDesc.mElementCount = pDesc->mIndexCount;
    filterIbDesc.mDesc.mStructStride = sizeof(uint32_t);
    filterIbDesc.mDesc.mSize = filterIbDesc.mDesc.mElementCount * filterIbDesc.mDesc.mStructStride;
    filterIbDesc.mDesc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;
    filterIbDesc.mDesc.pName = "FilteredIndexBuffer";
    filterIbDesc.pData = NULL;

    for (uint32_t i = 0; i < pDesc->mNumBuffers * pDesc->mNumViews; ++i)
    {
        filterIbDesc.ppBuffer = &pVisibilityBuffer->ppFilteredIndexBuffer[i];
        addResource(&filterIbDesc, NULL);
    }

    // Create filteredDataIndexBuffers
    pVisibilityBuffer->ppIndirectDataIndexBuffer = (Buffer**)tf_malloc(sizeof(Buffer*) * pDesc->mNumBuffers);

    BufferLoadDesc indirectDataDesc = {};
    indirectDataDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER | DESCRIPTOR_TYPE_RW_BUFFER;
    indirectDataDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
    indirectDataDesc.mDesc.mElementCount = pDesc->mMaxDrawsIndirect * pDesc->mNumGeometrySets * pDesc->mNumViews;
    indirectDataDesc.mDesc.mStructStride = sizeof(uint32_t);
    indirectDataDesc.mDesc.mSize = indirectDataDesc.mDesc.mElementCount * indirectDataDesc.mDesc.mStructStride;
    indirectDataDesc.mDesc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;
    indirectDataDesc.mDesc.pName = "IndirectDataIndexBuffer";
    indirectDataDesc.pData = nullptr;

    for (uint32_t i = 0; i < pDesc->mNumBuffers; ++i)
    {
        indirectDataDesc.ppBuffer = &pVisibilityBuffer->ppIndirectDataIndexBuffer[i];
        addResource(&indirectDataDesc, NULL);
    }

    // Take into account alignment because otherwise the ring buffer will wrap around if it cannot align the memory at the correct address,
    // using just pDesc->mNumFrames-1 instead of pDesc->mNumFrames as requested.
    const uint32_t filterBatchRingBufferSizeTotal =
        pDesc->mNumFrames *
        (gVBSettings.mUniformBufferAlignment + gVBSettings.mNumFilterBatchChunks * pDesc->mFilterBatchCount * sizeof(FilterBatchData));
    addUniformGPURingBuffer(pRenderer, filterBatchRingBufferSizeTotal, &gFilterBatchDataBuffer);

    if (pDesc->mEnablePreSkinPass)
    {
        const uint32_t skinBatchRingBufferSizeTotal =
            pDesc->mNumFrames * (gVBSettings.mUniformBufferAlignment +
                                 gVBSettings.mNumPreSkinBatchChunks * pDesc->mPreSkinBatchCount * sizeof(PreSkinBatchData));
        addUniformGPURingBuffer(pRenderer, skinBatchRingBufferSizeTotal, &gPreSkinBatchDataBuffer);
    }

    waitForToken(&token);
    tf_free(indirectArgsDwords);

    *ppVisibilityBuffer = pVisibilityBuffer;
    return true;
}

void exitVisibilityBuffer(VisibilityBuffer* pVisibilityBuffer)
{
    ASSERT(pVisibilityBuffer);

    for (uint32_t i = 0; i < gVBSettings.mNumBuffers; ++i)
    {
        removeResource(pVisibilityBuffer->ppIndirectDataIndexBuffer[i]);
        removeResource(pVisibilityBuffer->ppFilteredIndirectDrawArgumentsBuffers[i]);
        removeResource(pVisibilityBuffer->ppUncompactedDrawArgumentsBuffer[i]);

        for (uint32_t v = 0; v < gVBSettings.mNumViews; ++v)
        {
            removeResource(pVisibilityBuffer->ppFilteredIndexBuffer[i * gVBSettings.mNumViews + v]);
        }
    }

    removeGPURingBuffer(&gFilterBatchDataBuffer);

    if (gVBSettings.mEnablePreSkinPass)
    {
        removeGPURingBuffer(&gPreSkinBatchDataBuffer);
    }

    tf_free(pVisibilityBuffer->ppUncompactedDrawArgumentsBuffer);
    pVisibilityBuffer->ppUncompactedDrawArgumentsBuffer = NULL;
    tf_free(pVisibilityBuffer->ppFilteredIndirectDrawArgumentsBuffers);
    pVisibilityBuffer->ppFilteredIndirectDrawArgumentsBuffers = NULL;
    tf_free(pVisibilityBuffer->ppFilteredIndexBuffer);
    pVisibilityBuffer->ppFilteredIndexBuffer = NULL;
    tf_free(pVisibilityBuffer->ppIndirectDataIndexBuffer);
    pVisibilityBuffer->ppIndirectDataIndexBuffer = NULL;

    tf_free(pVisibilityBuffer);
    pVisibilityBuffer = NULL;
}
