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

#include "IRenderer.h"
#include "GpuProfiler.h"
#include "ResourceLoader.h"
#include "../OS/Interfaces/IThread.h"
#include "../OS/Interfaces/ILogManager.h"
#include "../OS/Interfaces/IUIManager.h"
#include "../OS/Interfaces/IMemoryManager.h"

extern void getTimestampFrequency(Queue* pQueue, double* pFrequency);
extern void addQueryHeap(Renderer* pRenderer, const QueryHeapDesc* pDesc, QueryHeap** ppQueryHeap);
extern void removeQueryHeap(Renderer* pRenderer, QueryHeap* pQueryHeap);
extern void cmdBeginQuery(Cmd* pCmd, QueryHeap* pQueryHeap, QueryDesc* pQuery);
extern void cmdEndQuery(Cmd* pCmd, QueryHeap* pQueryHeap, QueryDesc* pQuery);
extern void cmdResolveQuery(Cmd* pCmd, QueryHeap* pQueryHeap, Buffer* pReadbackBuffer, uint32_t startQuery, uint32_t queryCount);

extern void mapBuffer(Renderer* pRenderer, Buffer* pBuffer, ReadRange* pRange /* = NULL */);
extern void unmapBuffer(Renderer* pRenderer, Buffer* pBuffer);

#if defined(DIRECT3D12) || defined(VULKAN)
void clearChildren(GpuTimerTree* pRoot)
{
	if (!pRoot)
		return;

	for (uint32_t i = 0; i < (uint32_t)pRoot->mChildren.size(); ++i)
		clearChildren(pRoot->mChildren[i]);

	pRoot->mChildren.clear();
}

static void calculateTimes(Cmd* pCmd, GpuProfiler* pGpuProfiler, GpuTimerTree* pRoot)
{
	if (!pRoot)
		return;

	ASSERT(pGpuProfiler->pTimeStamp != NULL && "Time stamp readback buffer is not mapped");
	if (pRoot != &pGpuProfiler->mRoot)
	{
		uint32_t id = pRoot->mGpuTimer.mIndex;
		uint64_t timeStamp1 = pGpuProfiler->pTimeStamp[id * 2];
		uint64_t timeStamp2 = pGpuProfiler->pTimeStamp[id * 2 + 1];

		int64_t elapsedTime = int64_t(timeStamp2 - timeStamp1);
		if (timeStamp2 <= timeStamp1)
		{
			elapsedTime = 0;
		}

		uint32_t historyIndex = pRoot->mGpuTimer.mHistoryIndex;
		
		pRoot->mGpuTimer.mGpuTime = elapsedTime;
		pRoot->mGpuTimer.mGpuHistory[historyIndex] = elapsedTime;

		elapsedTime = pRoot->mGpuTimer.mEndCpuTime - pRoot->mGpuTimer.mStartCpuTime;
		if (elapsedTime < 0)
		{
			elapsedTime = 0;
		}

		pRoot->mGpuTimer.mCpuTime = elapsedTime;
		pRoot->mGpuTimer.mCpuHistory[historyIndex] = elapsedTime;

		pRoot->mGpuTimer.mHistoryIndex = (historyIndex + 1) % GpuTimer::LENGTH_OF_HISTORY;
	}

	for (uint32_t i = 0; i < (uint32_t)pRoot->mChildren.size(); ++i)
		calculateTimes(pCmd, pGpuProfiler, pRoot->mChildren[i]);
}
#endif

double getAverageGpuTime(struct GpuProfiler* pGpuProfiler, struct GpuTimer* pGpuTimer)
{
	int64_t elapsedTime = 0;

	for (uint32_t i = 0; i < GpuTimer::LENGTH_OF_HISTORY; ++i)
	{
		elapsedTime += pGpuTimer->mGpuHistory[i];
	}

	// check for overflow
	if (elapsedTime < 0)
		elapsedTime = 0;

	return (elapsedTime / GpuTimer::LENGTH_OF_HISTORY) / pGpuProfiler->mGpuTimeStampFrequency;
}

double getAverageCpuTime(struct GpuProfiler* pGpuProfiler, struct GpuTimer* pGpuTimer)
{
	int64_t elapsedTime = 0;

	for (uint32_t i = 0; i < GpuTimer::LENGTH_OF_HISTORY; ++i)
	{
		elapsedTime += pGpuTimer->mCpuHistory[i];
	}

	// check for overflow
	if (elapsedTime < 0)
		elapsedTime = 0;

	return (elapsedTime / GpuTimer::LENGTH_OF_HISTORY) / pGpuProfiler->mCpuTimeStampFrequency;
}

void addGpuProfiler(Renderer* pRenderer, Queue* pQueue, GpuProfiler** ppGpuProfiler, uint32_t maxTimers)
{
	GpuProfiler* pGpuProfiler = (GpuProfiler*)conf_calloc(1, sizeof(*pGpuProfiler));

#if defined(DIRECT3D12) || defined(VULKAN)
	QueryHeapDesc queryHeapDesc = { QUERY_TYPE_TIMESTAMP, maxTimers * 2 };

	for (uint32_t i = 0; i < GpuProfiler::NUM_OF_FRAMES; ++i)
	{
		addQueryHeap(pRenderer, &queryHeapDesc, &pGpuProfiler->pQueryHeap[i]);
	}

	BufferLoadDesc bufDesc = {};
	bufDesc.mDesc.mUsage = BUFFER_USAGE_UPLOAD;
	bufDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_TO_CPU;
	bufDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_OWN_MEMORY_BIT;
	bufDesc.mDesc.mSize = maxTimers * sizeof(uint64_t) * 2;
	bufDesc.pData = NULL;
	bufDesc.mDesc.pDebugName = L"GPU Profilter ReadBack Buffer Desc";

	for (uint32_t i = 0; i < GpuProfiler::NUM_OF_FRAMES; ++i)
	{
		bufDesc.ppBuffer = &pGpuProfiler->pReadbackBuffer[i];
		addResource(&bufDesc);
	}

	getTimestampFrequency(pQueue, &pGpuProfiler->mGpuTimeStampFrequency);
	pGpuProfiler->mCpuTimeStampFrequency = (double)getTimerFrequency();
#endif

#if defined(VULKAN)
	pGpuProfiler->mGpuTimeStampFrequency *= 1e+9;
#endif

	pGpuProfiler->mMaxTimerCount = maxTimers;
	pGpuProfiler->pGpuTimerPool = (GpuTimerTree*)conf_calloc(maxTimers, sizeof(*pGpuProfiler->pGpuTimerPool));
	pGpuProfiler->pCurrentNode = &pGpuProfiler->mRoot;

	*ppGpuProfiler = pGpuProfiler;
}

void removeGpuProfiler(Renderer* pRenderer, GpuProfiler* pGpuProfiler)
{
#if defined(DIRECT3D12) || defined(VULKAN)
	for (uint32_t i = 0; i < GpuProfiler::NUM_OF_FRAMES; ++i)
	{
		removeResource(pGpuProfiler->pReadbackBuffer[i]);
		removeQueryHeap(pRenderer, pGpuProfiler->pQueryHeap[i]);
	}
#endif

	for (uint32_t i = 0; i < pGpuProfiler->mMaxTimerCount; ++i)
	{
		pGpuProfiler->pGpuTimerPool[i].mChildren.~vector();
		pGpuProfiler->pGpuTimerPool[i].mGpuTimer.mName.~string();
	}

	pGpuProfiler->mRoot.mChildren.~vector();
	pGpuProfiler->mGpuPoolHash.~unordered_map();

	conf_free(pGpuProfiler->pGpuTimerPool);
	conf_free(pGpuProfiler);
}

void cmdBeginGpuTimestampQuery(Cmd* pCmd, struct GpuProfiler* pGpuProfiler, const char* pName, bool addMarker, const float3& color)
{
#if defined(DIRECT3D12) || defined(VULKAN)

	// hash name
	char buffer[MAX_PATH];
	sprintf(buffer, "%s_%u", pName, pGpuProfiler->mCurrentTimerCount);
	uint32_t hash = tinystl::hash(buffer);

	GpuTimerTree* node = nullptr;
	if (pGpuProfiler->mGpuPoolHash.find(hash) == pGpuProfiler->mGpuPoolHash.end())
	{
		// frist time seeing this
		node = &pGpuProfiler->pGpuTimerPool[pGpuProfiler->mCurrentPoolIndex];
		pGpuProfiler->mGpuPoolHash[hash] = pGpuProfiler->mCurrentPoolIndex;

		++pGpuProfiler->mCurrentPoolIndex;

		node->mGpuTimer.mName = pName;
		node->mGpuTimer.mHistoryIndex = 0;
		memset(node->mGpuTimer.mGpuHistory, 0, sizeof(node->mGpuTimer.mGpuHistory));
		memset(node->mGpuTimer.mCpuHistory, 0, sizeof(node->mGpuTimer.mCpuHistory));
	}
	else
	{
		uint32_t index = pGpuProfiler->mGpuPoolHash[hash];
		node = &pGpuProfiler->pGpuTimerPool[index];
	}

	// Record gpu time
	node->mGpuTimer.mIndex = pGpuProfiler->mCurrentTimerCount;
	node->pParent = pGpuProfiler->pCurrentNode;
	node->mDebugMarker = addMarker;

	pGpuProfiler->pCurrentNode->mChildren.emplace_back(node);
	pGpuProfiler->pCurrentNode = pGpuProfiler->pCurrentNode->mChildren.back();

	QueryDesc desc = { 2 * node->mGpuTimer.mIndex };
	cmdBeginQuery(pCmd, pGpuProfiler->pQueryHeap[pGpuProfiler->mBufferIndex], &desc);

	if (addMarker)
	{
		cmdBeginDebugMarker(pCmd, color.getX(), color.getY(), color.getZ(), pName);
	}

	// Record gpu time
	node->mGpuTimer.mStartCpuTime = getUSec();

	++pGpuProfiler->mCurrentTimerCount;
#endif
}

void cmdEndGpuTimestampQuery(Cmd* pCmd, struct GpuProfiler* pGpuProfiler, GpuTimer** ppGpuTimer)
{
#if defined(DIRECT3D12) || defined(VULKAN)
	// Record cpu time
	pGpuProfiler->pCurrentNode->mGpuTimer.mEndCpuTime = getUSec();

	// Record gpu time
	QueryDesc desc = { 2 * pGpuProfiler->pCurrentNode->mGpuTimer.mIndex + 1 };
	cmdEndQuery(pCmd, pGpuProfiler->pQueryHeap[pGpuProfiler->mBufferIndex], &desc);

	if (pGpuProfiler->pCurrentNode->mDebugMarker)
	{
		cmdEndDebugMarker(pCmd);
	}

	if (ppGpuTimer)
		*ppGpuTimer = &pGpuProfiler->pCurrentNode->mGpuTimer;

	pGpuProfiler->pCurrentNode = pGpuProfiler->pCurrentNode->pParent;
#endif
}

void cmdBeginGpuFrameProfile(Cmd* pCmd, GpuProfiler* pGpuProfiler, bool bUseMarker)
{
#if defined(DIRECT3D12) || defined(VULKAN)
	// resolve last frame
	cmdResolveQuery(pCmd, 
		pGpuProfiler->pQueryHeap[pGpuProfiler->mBufferIndex],
		pGpuProfiler->pReadbackBuffer[pGpuProfiler->mBufferIndex], 
		0, pGpuProfiler->mCurrentTimerCount * 2);


	ReadRange range = {};
	range.mOffset = 0;
	range.mSize = max(sizeof(uint64_t) * 2, (pGpuProfiler->mCurrentTimerCount) * sizeof(uint64_t) * 2);

	// readback n + 1 frame
	uint32_t nextIndex = (pGpuProfiler->mBufferIndex + 1) % GpuProfiler::NUM_OF_FRAMES;
	mapBuffer(pCmd->pCmdPool->pRenderer, pGpuProfiler->pReadbackBuffer[nextIndex], &range);
	pGpuProfiler->pTimeStamp = (uint64_t*)pGpuProfiler->pReadbackBuffer[nextIndex]->pCpuMappedAddress;

	pGpuProfiler->mBufferIndex = nextIndex;

	clearChildren(&pGpuProfiler->mRoot);

	pGpuProfiler->mCurrentTimerCount = 0;
	pGpuProfiler->mCumulativeTime = pGpuProfiler->mCumulativeTimeInternal;
	pGpuProfiler->mCumulativeTimeInternal = 0.0;
	pGpuProfiler->mCumulativeCpuTime = pGpuProfiler->mCumulativeCpuTimeInternal;
	pGpuProfiler->mCumulativeCpuTimeInternal = 0.0;
	pGpuProfiler->pCurrentNode = &pGpuProfiler->mRoot;

	cmdBeginGpuTimestampQuery(pCmd, pGpuProfiler, "ROOT", bUseMarker);
#endif
}

void cmdEndGpuFrameProfile(Cmd* pCmd, GpuProfiler* pGpuProfiler)
{
#if defined(DIRECT3D12) || defined(VULKAN)
	cmdEndGpuTimestampQuery(pCmd, pGpuProfiler);

	for (uint32_t i = 0; i < (uint32_t)pGpuProfiler->mRoot.mChildren.size(); ++i)
	{
		pGpuProfiler->mCumulativeTimeInternal += getAverageGpuTime(pGpuProfiler, &pGpuProfiler->mRoot.mChildren[i]->mGpuTimer);

		pGpuProfiler->mCumulativeCpuTimeInternal += getAverageCpuTime(pGpuProfiler, &pGpuProfiler->mRoot.mChildren[i]->mGpuTimer);
	}

	calculateTimes(pCmd, pGpuProfiler, &pGpuProfiler->mRoot);

	unmapBuffer(pCmd->pCmdPool->pRenderer, pGpuProfiler->pReadbackBuffer[pGpuProfiler->mBufferIndex]);
	pGpuProfiler->pTimeStamp = NULL;

#endif
}
