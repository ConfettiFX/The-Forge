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
#include "../Interfaces/ILogManager.h"
#include "../Interfaces/IMemoryManager.h"

#if !defined(RENDERER_DLL_IMPORT)
API_INTERFACE void CALLTYPE addBuffer(Renderer* pRenderer, const BufferDesc* pDesc, Buffer** pp_buffer);
API_INTERFACE void CALLTYPE removeBuffer(Renderer* pRenderer, Buffer* pBuffer);
#endif
/************************************************************************/
/* RING BUFFER MANAGEMENT                                               */
/************************************************************************/
typedef struct MeshRingBuffer
{
	Renderer*	pRenderer;
	Buffer**	ppVertexBufferPool;
	Buffer**	ppIndexBufferPool;

	uint32_t	mMaxDrawCallCount;
	uint32_t	mMaxVertexCount;
	uint32_t	mMaxIndexCount;
	uint32_t	mCurrentVertexBufferCount;
	uint32_t	mCurrentIndexBufferCount;
} MeshRingBuffer;

typedef struct UniformRingBuffer
{
	Renderer*	pRenderer;
	Buffer*		pUniformBuffer;
	uint32_t	mUniformBufferAlignment;
	uint32_t	mMaxUniformBufferSize;
	uint32_t	mUniformOffset;
} UniformRingBuffer;

typedef struct UniformBufferOffset
{
	Buffer*		pUniformBuffer;
	uint64_t	mOffset;
} UniformBufferOffset;

static inline void addMeshRingBuffer(Renderer* pRenderer, uint32_t maxDraws, const BufferDesc* pVertexBufferDesc, const BufferDesc* pIndexBufferDesc, MeshRingBuffer** ppRingBuffer)
{
	MeshRingBuffer* pRingBuffer = (MeshRingBuffer*)conf_calloc(1, sizeof(MeshRingBuffer));
	pRingBuffer->pRenderer = pRenderer;
	pRingBuffer->mMaxDrawCallCount = maxDraws;
	pRingBuffer->mMaxVertexCount = (uint32_t)(pVertexBufferDesc->mSize / pVertexBufferDesc->mVertexStride);
	pRingBuffer->mMaxIndexCount = (uint32_t)(pIndexBufferDesc ? pIndexBufferDesc->mSize / (pIndexBufferDesc->mIndexType == INDEX_TYPE_UINT16 ? sizeof(uint16_t) : sizeof(uint32_t)) : 0);

	pRingBuffer->ppVertexBufferPool = (Buffer**)conf_calloc(maxDraws, sizeof(Buffer*));
	if (pIndexBufferDesc)
	{
		pRingBuffer->ppIndexBufferPool = (Buffer**)conf_calloc(maxDraws, sizeof(Buffer*));
	}

	for (uint32_t i = 0; i < maxDraws; ++i)
	{
		addBuffer(pRenderer, pVertexBufferDesc, &pRingBuffer->ppVertexBufferPool[i]);

		if (pRingBuffer->ppIndexBufferPool)
		{
			addBuffer(pRenderer, pIndexBufferDesc, &pRingBuffer->ppIndexBufferPool[i]);
		}
	}

	*ppRingBuffer = pRingBuffer;
}

static inline void removeMeshRingBuffer(MeshRingBuffer* pRingBuffer)
{
	for (uint32_t i = 0; i < pRingBuffer->mMaxDrawCallCount; ++i)
	{
		removeBuffer(pRingBuffer->pRenderer, pRingBuffer->ppVertexBufferPool[i]);
		if (pRingBuffer->ppIndexBufferPool)
			removeBuffer(pRingBuffer->pRenderer, pRingBuffer->ppIndexBufferPool[i]);
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
	pRingBuffer->pRenderer = pRenderer;

	const uint32_t uniformBufferAlignment = (uint32_t)pRenderer->pActiveGpuSettings->mUniformBufferAlignment;
	const uint32_t maxUniformBufferSize = requiredUniformBufferSize;
	pRingBuffer->mUniformBufferAlignment = uniformBufferAlignment;
	pRingBuffer->mMaxUniformBufferSize = maxUniformBufferSize;

	BufferDesc ubDesc = {};
	ubDesc.mUsage = BUFFER_USAGE_UNIFORM;
	ubDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
	ubDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT | BUFFER_CREATION_FLAG_NO_DESCRIPTOR_VIEW_CREATION;
	ubDesc.mSize = maxUniformBufferSize;
	addBuffer(pRenderer, &ubDesc, &pRingBuffer->pUniformBuffer);

	*ppRingBuffer = pRingBuffer;
}

static void removeUniformRingBuffer(UniformRingBuffer* pRingBuffer)
{
	removeBuffer(pRingBuffer->pRenderer, pRingBuffer->pUniformBuffer);
	conf_free(pRingBuffer);
}

static inline void resetUniformRingBuffer(UniformRingBuffer* pRingBuffer)
{
	pRingBuffer->mUniformOffset = 0;
}

static UniformBufferOffset getUniformBufferOffset(UniformRingBuffer* pRingBuffer, uint32_t memoryRequirement)
{
	uint32_t alignedSize = round_up(memoryRequirement, (uint32_t)pRingBuffer->mUniformBufferAlignment);

	if (pRingBuffer->mUniformOffset + alignedSize >= pRingBuffer->mMaxUniformBufferSize)
	{
		pRingBuffer->mUniformOffset = 0;
	}
	else
	{
		pRingBuffer->mUniformOffset += alignedSize;
	}

	return{ pRingBuffer->pUniformBuffer, (uint64_t)pRingBuffer->mUniformOffset };
}
