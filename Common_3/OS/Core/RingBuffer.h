/*
 * Copyright (c) 2018 Confetti Interactive Inc.
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

#include "../../Renderer/IRenderer.h"
#include "../../Renderer/ResourceLoader.h"
#include "../Interfaces/ILogManager.h"
#include "../Interfaces/IMemoryManager.h"

/************************************************************************/
/* RING BUFFER MANAGEMENT                                               */
/************************************************************************/
typedef struct MeshRingBuffer
{
	Buffer** ppVertexBufferPool;
	Buffer** ppIndexBufferPool;

	uint32_t mMaxDrawCallCount;
	uint32_t mMaxVertexCount;
	uint32_t mMaxIndexCount;
	uint32_t mCurrentVertexBufferCount;
	uint32_t mCurrentIndexBufferCount;
} MeshRingBuffer;

typedef struct UniformRingBuffer
{
	Buffer** ppUniformBuffers;

	uint32_t mUniformBufferAlignment;
	uint32_t mMaxUniformBufferSize;
	uint32_t mUniformBufferCount;
	uint32_t mUniformIndex;
	uint32_t mUniformOffset;
} UniformRingBuffer;

typedef struct UniformBufferOffset
{
	Buffer*		pUniformBuffer;
	uint64_t	mOffset;
} UniformBufferOffset;

static inline void addMeshRingBuffer(uint32_t maxDraws, const BufferDesc* pVertexBufferDesc, const BufferDesc* pIndexBufferDesc, MeshRingBuffer** ppRingBuffer)
{
	MeshRingBuffer* pRingBuffer = (MeshRingBuffer*)conf_calloc(1, sizeof(MeshRingBuffer));

	pRingBuffer->mMaxDrawCallCount = maxDraws;
	pRingBuffer->mMaxVertexCount = (uint32_t)(pVertexBufferDesc->mSize / pVertexBufferDesc->mVertexStride);
	pRingBuffer->mMaxIndexCount = (uint32_t)(pIndexBufferDesc ? pIndexBufferDesc->mSize / (pIndexBufferDesc->mIndexType == INDEX_TYPE_UINT16 ? sizeof(uint16_t) : sizeof(uint32_t)) : 0);

	BufferLoadDesc vbDesc = {};
	BufferLoadDesc ibDesc = {};

	vbDesc.mDesc = *pVertexBufferDesc;
	vbDesc.pData = NULL;

	pRingBuffer->ppVertexBufferPool = (Buffer**)conf_calloc(maxDraws, sizeof(Buffer*));
	if (pIndexBufferDesc)
	{
		pRingBuffer->ppIndexBufferPool = (Buffer**)conf_calloc(maxDraws, sizeof(Buffer*));
		ibDesc.mDesc = *pIndexBufferDesc;
	}

	for (uint32_t i = 0; i < maxDraws; ++i)
	{
		vbDesc.ppBuffer = &pRingBuffer->ppVertexBufferPool[i];
		addResource(&vbDesc);

		if (pRingBuffer->ppIndexBufferPool)
		{
			ibDesc.ppBuffer = &pRingBuffer->ppIndexBufferPool[i];
			addResource(&ibDesc);
		}
	}

	*ppRingBuffer = pRingBuffer;
}

static inline void removeMeshRingBuffer(MeshRingBuffer* pRingBuffer)
{
	for (uint32_t i = 0; i < pRingBuffer->mMaxDrawCallCount; ++i)
	{
		removeResource(pRingBuffer->ppVertexBufferPool[i]);
		if (pRingBuffer->ppIndexBufferPool)
			removeResource(pRingBuffer->ppIndexBufferPool[i]);
	}

	conf_free(pRingBuffer->ppVertexBufferPool);
	if (pRingBuffer->ppIndexBufferPool)
		conf_free(pRingBuffer->ppIndexBufferPool);
	conf_free(pRingBuffer);
}

static inline void resetMeshRingBuffer(MeshRingBuffer* pRingBuffer)
{
	pRingBuffer->mCurrentVertexBufferCount = 0;
	pRingBuffer->mCurrentIndexBufferCount = 0;
}

static inline Buffer* getVertexBuffer(MeshRingBuffer* pRingBuffer)
{
	if (pRingBuffer->mCurrentVertexBufferCount == pRingBuffer->mMaxDrawCallCount)
		pRingBuffer->mCurrentVertexBufferCount = 0;

	return pRingBuffer->ppVertexBufferPool[pRingBuffer->mCurrentVertexBufferCount++];
}

static inline Buffer* getIndexBuffer(MeshRingBuffer* pRingBuffer)
{
	ASSERT(pRingBuffer->ppIndexBufferPool);
	if (pRingBuffer->mCurrentIndexBufferCount == pRingBuffer->mMaxDrawCallCount)
		pRingBuffer->mCurrentIndexBufferCount = 0;

	return pRingBuffer->ppIndexBufferPool[pRingBuffer->mCurrentIndexBufferCount++];
}

static void addUniformRingBuffer(Renderer* pRenderer, uint32_t requiredUniformBufferSize, UniformRingBuffer** ppRingBuffer)
{
	UniformRingBuffer* pRingBuffer = (UniformRingBuffer*)conf_calloc(1, sizeof(UniformRingBuffer));

	const uint32_t uniformBufferAlignment = (uint32_t)pRenderer->pActiveGpuSettings->mUniformBufferAlignment;
#if !defined(DIRECT3D12)
	const uint32_t maxUniformBufferSize = requiredUniformBufferSize;
#else
	const uint32_t maxUniformBufferSize = 65536U;
#endif
	pRingBuffer->mUniformBufferCount = max(1U, requiredUniformBufferSize / maxUniformBufferSize);
	pRingBuffer->mUniformBufferAlignment = uniformBufferAlignment;
	pRingBuffer->mMaxUniformBufferSize = maxUniformBufferSize;
	pRingBuffer->ppUniformBuffers = (Buffer**)conf_calloc(pRingBuffer->mUniformBufferCount, sizeof(Buffer*));

	BufferLoadDesc ubDesc = {};
	ubDesc.mDesc.mUsage = BUFFER_USAGE_UNIFORM;
	ubDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
	ubDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
	ubDesc.mDesc.mSize = maxUniformBufferSize;
	ubDesc.pData = NULL;
	for (uint32_t i = 0; i < pRingBuffer->mUniformBufferCount; ++i)
	{
		ubDesc.ppBuffer = &pRingBuffer->ppUniformBuffers[i];
		addResource(&ubDesc);
	}

	*ppRingBuffer = pRingBuffer;
}

static void removeUniformRingBuffer(UniformRingBuffer* pRingBuffer)
{
	for (uint32_t i = 0; i < pRingBuffer->mUniformBufferCount; ++i)
	{
		removeResource(pRingBuffer->ppUniformBuffers[i]);
	}

	conf_free(pRingBuffer->ppUniformBuffers);
	conf_free(pRingBuffer);
}

static inline void resetUniformRingBuffer(UniformRingBuffer* pRingBuffer)
{
	pRingBuffer->mUniformIndex = 0;
	pRingBuffer->mUniformOffset = 0;
}

static UniformBufferOffset getUniformBufferOffset(UniformRingBuffer* pRingBuffer, uint32_t memoryRequirement)
{
	uint32_t alignedSize = round_up(memoryRequirement, (uint32_t)pRingBuffer->mUniformBufferAlignment);

	if (pRingBuffer->mUniformOffset + alignedSize >= pRingBuffer->mMaxUniformBufferSize)
	{
		if (pRingBuffer->mUniformIndex + 1 < pRingBuffer->mUniformBufferCount)
			++pRingBuffer->mUniformIndex;
		else
			pRingBuffer->mUniformIndex = 0;

		pRingBuffer->mUniformOffset = 0;
	}
	else
	{
		pRingBuffer->mUniformOffset += alignedSize;
	}

	return{ pRingBuffer->ppUniformBuffers[pRingBuffer->mUniformIndex], (uint64_t)pRingBuffer->mUniformOffset };
}
