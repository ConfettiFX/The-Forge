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

#if !defined(ENABLE_RENDERER_RUNTIME_SWITCH)
API_INTERFACE void CALLTYPE addBuffer(Renderer* pRenderer, const BufferDesc* pDesc, Buffer** pp_buffer);
API_INTERFACE void CALLTYPE removeBuffer(Renderer* pRenderer, Buffer* pBuffer);
#endif
/************************************************************************/
/* RING BUFFER MANAGEMENT                                               */
/************************************************************************/
typedef struct MeshRingBuffer
{
	Renderer*	pRenderer;
	Buffer*		pVertexBuffer;
	Buffer*		pIndexBuffer;

	uint64_t	mMaxVertexBufferSize;
	uint64_t	mMaxIndexBufferSize;
	uint64_t	mCurrentVertexBufferOffset;
	uint64_t	mCurrentIndexBufferOffset;
} MeshRingBuffer;

typedef struct UniformRingBuffer
{
	Renderer*	pRenderer;
	Buffer*		pUniformBuffer;
	uint32_t	mUniformBufferAlignment;
	uint32_t	mMaxUniformBufferSize;
	uint32_t	mUniformOffset;
} UniformRingBuffer;

typedef struct RingBufferOffset
{
	Buffer*		pBuffer;
	uint64_t	mOffset;
} RingBufferOffset;

static inline void addMeshRingBuffer(Renderer* pRenderer, const BufferDesc* pVertexBufferDesc, const BufferDesc* pIndexBufferDesc, MeshRingBuffer** ppRingBuffer)
{
	MeshRingBuffer* pRingBuffer = (MeshRingBuffer*)conf_calloc(1, sizeof(MeshRingBuffer));
	pRingBuffer->pRenderer = pRenderer;
	pRingBuffer->mMaxVertexBufferSize = pVertexBufferDesc->mSize;

	addBuffer(pRenderer, pVertexBufferDesc, &pRingBuffer->pVertexBuffer);

	if (pIndexBufferDesc)
	{
		pRingBuffer->mMaxIndexBufferSize = pIndexBufferDesc->mSize;
		addBuffer(pRenderer, pIndexBufferDesc, &pRingBuffer->pIndexBuffer);
	}

	*ppRingBuffer = pRingBuffer;
}

static inline void removeMeshRingBuffer(MeshRingBuffer* pRingBuffer)
{
	removeBuffer(pRingBuffer->pRenderer, pRingBuffer->pVertexBuffer);
	if (pRingBuffer->pIndexBuffer)
		removeBuffer(pRingBuffer->pRenderer, pRingBuffer->pIndexBuffer);

	conf_free(pRingBuffer);
}

static inline void resetMeshRingBuffer(MeshRingBuffer* pRingBuffer)
{
	pRingBuffer->mCurrentVertexBufferOffset = 0;
	pRingBuffer->mCurrentIndexBufferOffset = 0;
}

static inline RingBufferOffset getVertexBufferOffset(MeshRingBuffer* pRingBuffer, uint32_t memoryRequirement)
{
	uint32_t alignedSize = round_up(memoryRequirement, (uint32_t)sizeof(float[4]));

	if (alignedSize > pRingBuffer->mMaxVertexBufferSize)
		return { NULL, 0 };

	if (pRingBuffer->mCurrentVertexBufferOffset + alignedSize >= pRingBuffer->mMaxVertexBufferSize)
		pRingBuffer->mCurrentVertexBufferOffset = 0;

	RingBufferOffset ret = { pRingBuffer->pVertexBuffer, pRingBuffer->mCurrentVertexBufferOffset };
	pRingBuffer->mCurrentVertexBufferOffset += alignedSize;

	return ret;
}

static inline RingBufferOffset getIndexBufferOffset(MeshRingBuffer* pRingBuffer, uint32_t memoryRequirement)
{
	uint32_t alignedSize = round_up(memoryRequirement, (uint32_t)sizeof(float[4]));

	if (alignedSize > pRingBuffer->mMaxIndexBufferSize)
		return { NULL, 0 };

	if (pRingBuffer->mCurrentIndexBufferOffset + alignedSize >= pRingBuffer->mMaxIndexBufferSize)
		pRingBuffer->mCurrentIndexBufferOffset = 0;

	RingBufferOffset ret = { pRingBuffer->pIndexBuffer, pRingBuffer->mCurrentIndexBufferOffset };
	pRingBuffer->mCurrentIndexBufferOffset += alignedSize;

	return ret;
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

static RingBufferOffset getUniformBufferOffset(UniformRingBuffer* pRingBuffer, uint32_t memoryRequirement, uint32_t alignment = 0)
{
	uint32_t alignedSize = round_up(memoryRequirement, (alignment ? alignment : (uint32_t)pRingBuffer->mUniformBufferAlignment));

	if (alignedSize > pRingBuffer->mMaxUniformBufferSize)
	{
		ASSERT(false && "Ring Buffer too small for memory requirement");
		return { NULL, 0 };
	}

	if (pRingBuffer->mUniformOffset + alignedSize >= pRingBuffer->mMaxUniformBufferSize)
	{
		resetUniformRingBuffer(pRingBuffer);
	}

	RingBufferOffset ret = { pRingBuffer->pUniformBuffer, (uint64_t)pRingBuffer->mUniformOffset };
	pRingBuffer->mUniformOffset += alignedSize;

	return ret;
}
