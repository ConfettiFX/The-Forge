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

#include "../Application/Config.h"

#include "../Graphics/Interfaces/IGraphics.h"
#include "../Resources/ResourceLoader/Interfaces/IResourceLoader.h"
#include "Interfaces/ILog.h"

/************************************************************************/
/* RING BUFFER MANAGEMENT											  */
/************************************************************************/
typedef struct GPURingBuffer
{
    Renderer* pRenderer;
    Buffer*   pBuffer;

    uint32_t mBufferAlignment;
    uint64_t mMaxBufferSize;
    uint64_t mCurrentBufferOffset;
} GPURingBuffer;

typedef struct GPURingBufferOffset
{
    Buffer*  pBuffer;
    uint64_t mOffset;
} GPURingBufferOffset;

#ifndef MAX_GPU_CMD_POOLS_PER_RING
#define MAX_GPU_CMD_POOLS_PER_RING 64u
#endif
#ifndef MAX_GPU_CMDS_PER_POOL
#define MAX_GPU_CMDS_PER_POOL 4u
#endif

typedef struct GpuCmdRingDesc
{
    // Queue used to create the command pools
    Queue*   pQueue;
    // Number of command pools in this ring
    uint32_t mPoolCount;
    // Number of command buffers to be created per command pool
    uint32_t mCmdPerPoolCount;
    // Whether to add fence, semaphore for this ring
    bool     mAddSyncPrimitives;
} GpuCmdRingDesc;

typedef struct GpuCmdRingElement
{
    CmdPool*   pCmdPool;
    Cmd**      pCmds;
    Fence*     pFence;
    Semaphore* pSemaphore;
} GpuCmdRingElement;

// Lightweight wrapper that works as a ring for command pools, command buffers
typedef struct GpuCmdRing
{
    CmdPool*   pCmdPools[MAX_GPU_CMD_POOLS_PER_RING];
    Cmd*       pCmds[MAX_GPU_CMD_POOLS_PER_RING][MAX_GPU_CMDS_PER_POOL];
    Fence*     pFences[MAX_GPU_CMD_POOLS_PER_RING][MAX_GPU_CMDS_PER_POOL];
    Semaphore* pSemaphores[MAX_GPU_CMD_POOLS_PER_RING][MAX_GPU_CMDS_PER_POOL];
    uint32_t   mPoolIndex;
    uint32_t   mCmdIndex;
    uint32_t   mFenceIndex;
    uint32_t   mPoolCount;
    uint32_t   mCmdPerPoolCount;
} GpuCmdRing;

static inline void addGPURingBuffer(Renderer* pRenderer, const BufferDesc* pBufferDesc, GPURingBuffer* pRingBuffer)
{
    *pRingBuffer = {};
    pRingBuffer->pRenderer = pRenderer;
    pRingBuffer->mMaxBufferSize = pBufferDesc->mSize;
    pRingBuffer->mBufferAlignment = sizeof(float[4]);
    BufferLoadDesc loadDesc = {};
    loadDesc.mDesc = *pBufferDesc;
    loadDesc.mDesc.pName = "GPURingBuffer";
    loadDesc.ppBuffer = &pRingBuffer->pBuffer;
    addResource(&loadDesc, NULL);
}

static inline void addUniformGPURingBuffer(Renderer* pRenderer, uint32_t requiredUniformBufferSize, GPURingBuffer* pRingBuffer,
                                           bool const ownMemory = false, ResourceMemoryUsage memoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU)
{
    *pRingBuffer = {};
    pRingBuffer->pRenderer = pRenderer;

    const uint32_t uniformBufferAlignment = (uint32_t)pRenderer->pGpu->mUniformBufferAlignment;
    const uint32_t maxUniformBufferSize = requiredUniformBufferSize;
    pRingBuffer->mBufferAlignment = uniformBufferAlignment;
    pRingBuffer->mMaxBufferSize = maxUniformBufferSize;

    BufferDesc ubDesc = {};
    ubDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ubDesc.mMemoryUsage = memoryUsage;
    ubDesc.mFlags =
        (ubDesc.mMemoryUsage != RESOURCE_MEMORY_USAGE_GPU_ONLY ? BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT : BUFFER_CREATION_FLAG_NONE) |
        BUFFER_CREATION_FLAG_NO_DESCRIPTOR_VIEW_CREATION;

    if (ownMemory)
        ubDesc.mFlags |= BUFFER_CREATION_FLAG_OWN_MEMORY_BIT;
    ubDesc.mSize = maxUniformBufferSize;
    ubDesc.pName = "UniformGPURingBuffer";
    BufferLoadDesc loadDesc = {};
    loadDesc.mDesc = ubDesc;
    loadDesc.ppBuffer = &pRingBuffer->pBuffer;
    addResource(&loadDesc, NULL);
}

static inline void removeGPURingBuffer(GPURingBuffer* pRingBuffer) { removeResource(pRingBuffer->pBuffer); }

static inline void resetGPURingBuffer(GPURingBuffer* pRingBuffer) { pRingBuffer->mCurrentBufferOffset = 0; }

static inline GPURingBufferOffset getGPURingBufferOffset(GPURingBuffer* pRingBuffer, uint32_t memoryRequirement, uint32_t alignment = 0)
{
    uint32_t alignedSize = round_up(memoryRequirement, alignment ? alignment : pRingBuffer->mBufferAlignment);

    if (alignedSize > pRingBuffer->mMaxBufferSize)
    {
        ASSERT(false && "Ring Buffer too small for memory requirement");
        return { NULL, 0 };
    }

    if (pRingBuffer->mCurrentBufferOffset + alignedSize >= pRingBuffer->mMaxBufferSize)
    {
        pRingBuffer->mCurrentBufferOffset = 0;
    }

    GPURingBufferOffset ret = { pRingBuffer->pBuffer, pRingBuffer->mCurrentBufferOffset };
    pRingBuffer->mCurrentBufferOffset += alignedSize;

    return ret;
}

static inline void initGpuCmdRing(Renderer* pRenderer, const GpuCmdRingDesc* pDesc, GpuCmdRing* pOut)
{
    ASSERT(pDesc->mPoolCount <= MAX_GPU_CMD_POOLS_PER_RING);
    ASSERT(pDesc->mCmdPerPoolCount <= MAX_GPU_CMDS_PER_POOL);

    pOut->mPoolCount = pDesc->mPoolCount;
    pOut->mCmdPerPoolCount = pDesc->mCmdPerPoolCount;

    CmdPoolDesc poolDesc = {};
    poolDesc.mTransient = false;
    poolDesc.pQueue = pDesc->pQueue;

    for (uint32_t pool = 0; pool < pDesc->mPoolCount; ++pool)
    {
        initCmdPool(pRenderer, &poolDesc, &pOut->pCmdPools[pool]);
        CmdDesc cmdDesc = {};
        cmdDesc.pPool = pOut->pCmdPools[pool];
        for (uint32_t cmd = 0; cmd < pDesc->mCmdPerPoolCount; ++cmd)
        {
#ifdef ENABLE_GRAPHICS_DEBUG_ANNOTATION
            static char buffer[MAX_DEBUG_NAME_LENGTH];
            snprintf(buffer, sizeof(buffer), "GpuCmdRing Pool %u Cmd %u", pool, cmd);
            cmdDesc.pName = buffer;
#endif // ENABLE_GRAPHICS_DEBUG_ANNOTATION
            initCmd(pRenderer, &cmdDesc, &pOut->pCmds[pool][cmd]);

            if (pDesc->mAddSyncPrimitives)
            {
                initFence(pRenderer, &pOut->pFences[pool][cmd]);
                initSemaphore(pRenderer, &pOut->pSemaphores[pool][cmd]);
            }
        }
    }

    pOut->mPoolIndex = UINT32_MAX;
    pOut->mCmdIndex = UINT32_MAX;
    pOut->mFenceIndex = UINT32_MAX;
}

static inline void exitGpuCmdRing(Renderer* pRenderer, GpuCmdRing* pRing)
{
    for (uint32_t pool = 0; pool < pRing->mPoolCount; ++pool)
    {
        for (uint32_t cmd = 0; cmd < pRing->mCmdPerPoolCount; ++cmd)
        {
            exitCmd(pRenderer, pRing->pCmds[pool][cmd]);
            if (pRing->pSemaphores[pool][cmd])
            {
                exitSemaphore(pRenderer, pRing->pSemaphores[pool][cmd]);
            }
            if (pRing->pFences[pool][cmd])
            {
                exitFence(pRenderer, pRing->pFences[pool][cmd]);
            }
        }
        exitCmdPool(pRenderer, pRing->pCmdPools[pool]);
    }
    *pRing = {};
}

static inline GpuCmdRingElement getNextGpuCmdRingElement(GpuCmdRing* pRing, bool cyclePool, uint32_t cmdCount)
{
    if (cyclePool)
    {
        pRing->mPoolIndex = (pRing->mPoolIndex + 1) % pRing->mPoolCount;
        pRing->mCmdIndex = 0;
        pRing->mFenceIndex = 0;
    }

    if (pRing->mCmdIndex + cmdCount > pRing->mCmdPerPoolCount)
    {
        ASSERT(false && "Out of command buffers for this pool");
        return GpuCmdRingElement{};
    }

    GpuCmdRingElement ret = {};
    ret.pCmdPool = pRing->pCmdPools[pRing->mPoolIndex];
    ret.pCmds = &pRing->pCmds[pRing->mPoolIndex][pRing->mCmdIndex];
    ret.pFence = pRing->pFences[pRing->mPoolIndex][pRing->mFenceIndex];
    ret.pSemaphore = pRing->pSemaphores[pRing->mPoolIndex][pRing->mFenceIndex];

    pRing->mCmdIndex += cmdCount;
    ++pRing->mFenceIndex;

    return ret;
}
