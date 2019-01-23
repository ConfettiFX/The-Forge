/*
 * Copyright (c) 2018-2019 Confetti Interactive Inc.
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
#include "../OS/Interfaces/IMemoryManager.h"
#if __linux__
#include <linux/limits.h>    //PATH_MAX declaration
#define MAX_PATH PATH_MAX
#endif

#if !defined(ENABLE_RENDERER_RUNTIME_SWITCH)
extern void mapBuffer(Renderer* pRenderer, Buffer* pBuffer, ReadRange* pRange);
extern void unmapBuffer(Renderer* pRenderer, Buffer* pBuffer);
#endif

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

#if defined(DIRECT3D12) || defined(VULKAN) || defined(DIRECT3D11)
	ASSERT(pGpuProfiler->pTimeStamp != NULL && "Time stamp readback buffer is not mapped");
#endif

	if (pRoot != &pGpuProfiler->mRoot)
	{
		uint32_t historyIndex = pRoot->mGpuTimer.mHistoryIndex;
		int64_t  elapsedTime = 0;
#if defined(DIRECT3D12) || defined(VULKAN) || defined(DIRECT3D11)
		uint32_t id = pRoot->mGpuTimer.mIndex;
		uint64_t timeStamp1 = pGpuProfiler->pTimeStamp[id * 2];
		uint64_t timeStamp2 = pGpuProfiler->pTimeStamp[id * 2 + 1];

		elapsedTime = int64_t(timeStamp2 - timeStamp1);
		if (timeStamp2 <= timeStamp1)
		{
			elapsedTime = 0;
		}

		pRoot->mGpuTimer.mStartGpuTime = timeStamp1;
		pRoot->mGpuTimer.mEndGpuTime = timeStamp2;
		pRoot->mGpuTimer.mGpuTime = elapsedTime;
		pRoot->mGpuTimer.mGpuHistory[historyIndex] = elapsedTime;
#endif

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

	return ((double)elapsedTime / GpuTimer::LENGTH_OF_HISTORY) / 1e6;
}

void addGpuProfiler(Renderer* pRenderer, Queue* pQueue, GpuProfiler** ppGpuProfiler, uint32_t maxTimers)
{
	GpuProfiler* pGpuProfiler = (GpuProfiler*)conf_calloc(1, sizeof(*pGpuProfiler));
	ASSERT(pGpuProfiler);

#if defined(DIRECT3D12) || defined(VULKAN) || defined(DIRECT3D11)
	const uint32_t nodeIndex = pQueue->mQueueDesc.mNodeIndex;
	QueryHeapDesc  queryHeapDesc = {};
	queryHeapDesc.mNodeIndex = nodeIndex;
	queryHeapDesc.mQueryCount = maxTimers * 2;
	queryHeapDesc.mType = QUERY_TYPE_TIMESTAMP;

	BufferDesc bufDesc = {};
#if defined(DIRECT3D11)
	bufDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_ONLY;
#else
	bufDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_TO_CPU;
#endif
	bufDesc.mFlags = BUFFER_CREATION_FLAG_OWN_MEMORY_BIT;
	bufDesc.mSize = maxTimers * sizeof(uint64_t) * 2;
	bufDesc.pDebugName = (wchar_t*)L"GPU Profiler ReadBack Buffer";
	bufDesc.mNodeIndex = nodeIndex;
	bufDesc.mStartState = RESOURCE_STATE_COPY_DEST;

	for (uint32_t i = 0; i < GpuProfiler::NUM_OF_FRAMES; ++i)
	{
		addQueryHeap(pRenderer, &queryHeapDesc, &pGpuProfiler->pQueryHeap[i]);
		BufferLoadDesc loadDesc = {};
		loadDesc.mDesc = bufDesc;
		loadDesc.ppBuffer = &pGpuProfiler->pReadbackBuffer[i];
		addResource(&loadDesc);
	}

	getTimestampFrequency(pQueue, &pGpuProfiler->mGpuTimeStampFrequency);
	pGpuProfiler->mCpuTimeStampFrequency = (double)getTimerFrequency();
#endif

	pGpuProfiler->mMaxTimerCount = maxTimers;
	pGpuProfiler->pGpuTimerPool = (GpuTimerTree*)conf_calloc(maxTimers, sizeof(*pGpuProfiler->pGpuTimerPool));
	pGpuProfiler->pCurrentNode = &pGpuProfiler->mRoot;
	pGpuProfiler->mCurrentPoolIndex = 0;

	*ppGpuProfiler = pGpuProfiler;
}

void removeGpuProfiler(Renderer* pRenderer, GpuProfiler* pGpuProfiler)
{
#if defined(DIRECT3D12) || defined(VULKAN) || defined(DIRECT3D11)
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
	// hash name
	char _buffer[128] = {};    //Initialize to empty
	sprintf(_buffer, "%s_%u", pName, pGpuProfiler->mCurrentTimerCount);
	uint32_t _hash = tinystl::hash(_buffer);

	GpuTimerTree* node = NULL;
	if (pGpuProfiler->mGpuPoolHash.find(_hash) == pGpuProfiler->mGpuPoolHash.end())
	{
		// frist time seeing this
		node = &pGpuProfiler->pGpuTimerPool[pGpuProfiler->mCurrentPoolIndex];
		pGpuProfiler->mGpuPoolHash[_hash] = pGpuProfiler->mCurrentPoolIndex;

		++pGpuProfiler->mCurrentPoolIndex;

		node->mGpuTimer.mName = pName;
		node->mGpuTimer.mHistoryIndex = 0;
		memset(node->mGpuTimer.mGpuHistory, 0, sizeof(node->mGpuTimer.mGpuHistory));
		memset(node->mGpuTimer.mCpuHistory, 0, sizeof(node->mGpuTimer.mCpuHistory));
	}
	else
	{
		uint32_t index = pGpuProfiler->mGpuPoolHash[_hash];
		node = &pGpuProfiler->pGpuTimerPool[index];
	}

	// Record gpu time
	node->mGpuTimer.mIndex = pGpuProfiler->mCurrentTimerCount;
	node->pParent = pGpuProfiler->pCurrentNode;
	node->mDebugMarker = addMarker;

	pGpuProfiler->pCurrentNode->mChildren.emplace_back(node);
	pGpuProfiler->pCurrentNode = pGpuProfiler->pCurrentNode->mChildren.back();

#if defined(DIRECT3D12) || defined(VULKAN) || defined(DIRECT3D11)
	QueryDesc desc = { 2 * node->mGpuTimer.mIndex };
	cmdBeginQuery(pCmd, pGpuProfiler->pQueryHeap[pGpuProfiler->mBufferIndex], &desc);
#endif

	if (addMarker)
	{
		cmdBeginDebugMarker(pCmd, color.getX(), color.getY(), color.getZ(), pName);
	}

	// Record gpu time
	node->mGpuTimer.mStartCpuTime = getUSec();

	++pGpuProfiler->mCurrentTimerCount;
}

void cmdEndGpuTimestampQuery(Cmd* pCmd, struct GpuProfiler* pGpuProfiler, GpuTimer** ppGpuTimer)
{
	// Record cpu time
	pGpuProfiler->pCurrentNode->mGpuTimer.mEndCpuTime = getUSec();

#if defined(DIRECT3D12) || defined(VULKAN) || defined(DIRECT3D11)
	// Record gpu time
	QueryDesc desc = { 2 * pGpuProfiler->pCurrentNode->mGpuTimer.mIndex + 1 };
	cmdEndQuery(pCmd, pGpuProfiler->pQueryHeap[pGpuProfiler->mBufferIndex], &desc);
#endif

	if (pGpuProfiler->pCurrentNode->mDebugMarker)
	{
		cmdEndDebugMarker(pCmd);
	}

	if (ppGpuTimer)
		*ppGpuTimer = &pGpuProfiler->pCurrentNode->mGpuTimer;

	pGpuProfiler->pCurrentNode = pGpuProfiler->pCurrentNode->pParent;
}

void cmdBeginGpuFrameProfile(Cmd* pCmd, GpuProfiler* pGpuProfiler, bool bUseMarker)
{
#if defined(DIRECT3D12) || defined(VULKAN) || defined(DIRECT3D11)
	// resolve last frame
	cmdResolveQuery(
		pCmd, pGpuProfiler->pQueryHeap[pGpuProfiler->mBufferIndex], pGpuProfiler->pReadbackBuffer[pGpuProfiler->mBufferIndex], 0,
		pGpuProfiler->mCurrentTimerCount * 2);
#endif

	uint32_t nextIndex = (pGpuProfiler->mBufferIndex + 1) % GpuProfiler::NUM_OF_FRAMES;
	pGpuProfiler->mBufferIndex = nextIndex;

	clearChildren(&pGpuProfiler->mRoot);

	pGpuProfiler->mCurrentTimerCount = 0;
	pGpuProfiler->mCumulativeTime = pGpuProfiler->mCumulativeTimeInternal;
	pGpuProfiler->mCumulativeTimeInternal = 0.0;
	pGpuProfiler->mCumulativeCpuTime = pGpuProfiler->mCumulativeCpuTimeInternal;
	pGpuProfiler->mCumulativeCpuTimeInternal = 0.0;
	pGpuProfiler->pCurrentNode = &pGpuProfiler->mRoot;

	cmdBeginGpuTimestampQuery(pCmd, pGpuProfiler, "ROOT", bUseMarker);
}

void cmdEndGpuFrameProfile(Cmd* pCmd, GpuProfiler* pGpuProfiler)
{
	cmdEndGpuTimestampQuery(pCmd, pGpuProfiler);

	for (uint32_t i = 0; i < (uint32_t)pGpuProfiler->mRoot.mChildren.size(); ++i)
	{
		pGpuProfiler->mCumulativeTimeInternal += getAverageGpuTime(pGpuProfiler, &pGpuProfiler->mRoot.mChildren[i]->mGpuTimer);

		pGpuProfiler->mCumulativeCpuTimeInternal += getAverageCpuTime(pGpuProfiler, &pGpuProfiler->mRoot.mChildren[i]->mGpuTimer);
	}

#if defined(DIRECT3D12) || defined(VULKAN) || defined(DIRECT3D11)
	// readback n + 1 frame
	ReadRange range = {};
	range.mOffset = 0;
	range.mSize = max(sizeof(uint64_t) * 2, (pGpuProfiler->mCurrentTimerCount) * sizeof(uint64_t) * 2);
	mapBuffer(pCmd->pRenderer, pGpuProfiler->pReadbackBuffer[pGpuProfiler->mBufferIndex], &range);
	pGpuProfiler->pTimeStamp = (uint64_t*)pGpuProfiler->pReadbackBuffer[pGpuProfiler->mBufferIndex]->pCpuMappedAddress;
#endif

	calculateTimes(pCmd, pGpuProfiler, &pGpuProfiler->mRoot);

#if defined(DIRECT3D12) || defined(VULKAN) || defined(DIRECT3D11)
	unmapBuffer(pCmd->pRenderer, pGpuProfiler->pReadbackBuffer[pGpuProfiler->mBufferIndex]);
	pGpuProfiler->pTimeStamp = NULL;

#endif
}
