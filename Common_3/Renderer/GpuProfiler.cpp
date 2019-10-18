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

#include "GpuProfiler.h"
#include "IRenderer.h"
#include "ResourceLoader.h"
#if (PROFILE_ENABLED)
#include "../ThirdParty/OpenSource/MicroProfile/ProfilerBase.h"
#endif
#include "../OS/Interfaces/IThread.h"
#include "../OS/Interfaces/ILog.h"
#include "../OS/Interfaces/ITime.h"

#if __linux__
#include <linux/limits.h>    //PATH_MAX declaration
#define MAX_PATH PATH_MAX
#endif
#include "../OS/Interfaces/IMemory.h"

extern void mapBuffer(Renderer* pRenderer, Buffer* pBuffer, ReadRange* pRange);
extern void unmapBuffer(Renderer* pRenderer, Buffer* pBuffer);

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

#if defined(DIRECT3D12) || defined(VULKAN) || defined(DIRECT3D11) || defined(METAL)
	ASSERT(pGpuProfiler->pTimeStamp != NULL && "Time stamp readback buffer is not mapped");
#endif

	if (pRoot != &pGpuProfiler->mRoot)
	{
        int64_t  elapsedTime = 0;
		const uint32_t historyIndex = pRoot->mGpuTimer.mHistoryIndex;
        
#if defined(DIRECT3D12) || defined(VULKAN) || defined(DIRECT3D11) || defined(METAL)
		const uint32_t id = pRoot->mGpuTimer.mIndex;
		const uint64_t timeStamp1 = pGpuProfiler->pTimeStamp[id * 2];
		const uint64_t timeStamp2 = pGpuProfiler->pTimeStamp[id * 2 + 1];

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

void addGpuProfiler(Renderer* pRenderer, Queue* pQueue, GpuProfiler** ppGpuProfiler, const char * pName, uint32_t maxTimers)
{
	GpuProfiler* pGpuProfiler = (GpuProfiler*)conf_calloc(1, sizeof(*pGpuProfiler));
	ASSERT(pGpuProfiler);

	conf_placement_new<GpuProfiler>(pGpuProfiler);
	pGpuProfiler->mReset = true;

	// One for cmdBeginGpuFrameProfile
	if (maxTimers != GpuProfiler::MAX_TIMERS)
		++maxTimers;

#if defined(DIRECT3D12) || defined(VULKAN) || defined(DIRECT3D11) || defined(METAL)
	const uint32_t nodeIndex = pQueue->mQueueDesc.mNodeIndex;
	QueryPoolDesc  queryHeapDesc = {};
	queryHeapDesc.mNodeIndex = nodeIndex;
	queryHeapDesc.mQueryCount = maxTimers * 2;
	queryHeapDesc.mType = QUERY_TYPE_TIMESTAMP;

	BufferDesc bufDesc = {};
#if defined(DIRECT3D11) || defined(METAL)
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
		addQueryPool(pRenderer, &queryHeapDesc, &pGpuProfiler->pQueryPool[i]);
		BufferLoadDesc loadDesc = {};
		loadDesc.mDesc = bufDesc;
		loadDesc.ppBuffer = &pGpuProfiler->pReadbackBuffer[i];
		addResource(&loadDesc);
	}

	getTimestampFrequency(pQueue, &pGpuProfiler->mGpuTimeStampFrequency);
	pGpuProfiler->mCpuTimeStampFrequency = (double)getTimerFrequency();
#endif

	// Create buffer to sample from MicroProfile and log for current GpuProfiler
#if (PROFILE_ENABLED)
	pGpuProfiler->pTimeStampBuffer = static_cast<uint64_t *>(conf_malloc(maxTimers * sizeof(uint64_t)));
	memset(pGpuProfiler->pTimeStampBuffer, 0, maxTimers * sizeof(uint64_t));
	memcpy(pGpuProfiler->mGroupName, pName, strlen(pName));
	pGpuProfiler->pLog = ProfileCreateThreadLog(pName);
	pGpuProfiler->pLog->pGpuProfiler = pGpuProfiler;
	pGpuProfiler->pLog->nGpu = 1;
#endif

	pGpuProfiler->mMaxTimerCount = maxTimers;
	pGpuProfiler->pGpuTimerPool = (GpuTimerTree*)conf_calloc(maxTimers, sizeof(*pGpuProfiler->pGpuTimerPool));
	pGpuProfiler->pCurrentNode = &pGpuProfiler->mRoot;
	pGpuProfiler->mCurrentPoolIndex = 0;

	*ppGpuProfiler = pGpuProfiler;
}

void removeGpuProfiler(Renderer* pRenderer, GpuProfiler* pGpuProfiler)
{
#if defined(DIRECT3D12) || defined(VULKAN) || defined(DIRECT3D11) || defined(METAL)
	for (uint32_t i = 0; i < GpuProfiler::NUM_OF_FRAMES; ++i)
	{
		removeResource(pGpuProfiler->pReadbackBuffer[i]);
		removeQueryPool(pRenderer, pGpuProfiler->pQueryPool[i]);
	}
#endif

	for (uint32_t i = 0; i < pGpuProfiler->mMaxTimerCount; ++i)
	{
		pGpuProfiler->pGpuTimerPool[i].mChildren.~vector();
		pGpuProfiler->pGpuTimerPool[i].mGpuTimer.mName.~basic_string();
	}

	pGpuProfiler->mRoot.mChildren.~vector();
	pGpuProfiler->mGpuPoolHash.~string_hash_map();

#if (PROFILE_ENABLED)
	ProfileRemoveThreadLog(pGpuProfiler->pLog);
	conf_free(pGpuProfiler->pTimeStampBuffer);
#endif
	conf_free(pGpuProfiler->pGpuTimerPool);
	conf_free(pGpuProfiler);
}

void cmdBeginGpuTimestampQuery(Cmd* pCmd, struct GpuProfiler* pGpuProfiler, const char* pName, bool addMarker, const float3& color, bool isRoot)
{
	// hash name
	char _buffer[128] = {};    //Initialize to empty
	sprintf(_buffer, "%s_%u", pName, pGpuProfiler->mCurrentTimerCount);
	GpuTimerTree* node = NULL;
	if (pGpuProfiler->mGpuPoolHash.find(_buffer) == pGpuProfiler->mGpuPoolHash.end())
	{
		// frist time seeing this
		node = &pGpuProfiler->pGpuTimerPool[pGpuProfiler->mCurrentPoolIndex];
		pGpuProfiler->mGpuPoolHash[_buffer] = pGpuProfiler->mCurrentPoolIndex;

		++pGpuProfiler->mCurrentPoolIndex;

		node->mGpuTimer.mName = pName;
		node->mGpuTimer.mHistoryIndex = 0;
		memset(node->mGpuTimer.mGpuHistory, 0, sizeof(node->mGpuTimer.mGpuHistory));
		memset(node->mGpuTimer.mCpuHistory, 0, sizeof(node->mGpuTimer.mCpuHistory));
	}
	else
	{
		uint32_t index = pGpuProfiler->mGpuPoolHash[_buffer];
		node = &pGpuProfiler->pGpuTimerPool[index];
	}

	// Record gpu time
	node->mGpuTimer.mIndex = pGpuProfiler->mCurrentTimerCount;
	node->pParent = pGpuProfiler->pCurrentNode;
	node->mDebugMarker = addMarker;

	pGpuProfiler->pCurrentNode->mChildren.emplace_back(node);
	pGpuProfiler->pCurrentNode = pGpuProfiler->pCurrentNode->mChildren.back();

#if defined(DIRECT3D12) || defined(VULKAN) || defined(DIRECT3D11) || defined(METAL)
#if defined(METAL)
    if (isRoot)
    {
#endif
        QueryDesc desc = { 2 * node->mGpuTimer.mIndex };
        cmdBeginQuery(pCmd, pGpuProfiler->pQueryPool[pGpuProfiler->mBufferIndex], &desc);

#if (PROFILE_ENABLED)
				// Send data to MicroProfile
				uint32 scope_color = static_cast<uint32>(color.getX() * 255) << 16
						| static_cast<uint32>(color.getY() * 255) << 8
						| static_cast<uint32>(color.getZ() * 255);
				node->mMicroProfileToken = ProfileGetToken(pGpuProfiler->mGroupName, pName, scope_color, ProfileTokenTypeGpu);
				if (ProfileEnterGpu(node->mMicroProfileToken, desc.mIndex) == PROFILE_INVALID_TICK)
					node->mMicroProfileToken = PROFILE_INVALID_TOKEN;
#endif
#if defined(METAL)
    }
#endif
#endif

	if (addMarker)
	{
		cmdBeginDebugMarker(pCmd, color.getX(), color.getY(), color.getZ(), pName);
	}

	// Record gpu time
	node->mGpuTimer.mStartCpuTime = getUSec();

	++pGpuProfiler->mCurrentTimerCount;
}

void cmdEndGpuTimestampQuery(Cmd* pCmd, struct GpuProfiler* pGpuProfiler, GpuTimer** ppGpuTimer, bool isRoot)
{
	// Record cpu time
	pGpuProfiler->pCurrentNode->mGpuTimer.mEndCpuTime = getUSec();

#if defined(DIRECT3D12) || defined(VULKAN) || defined(DIRECT3D11) || defined(METAL)
#if defined(METAL)
    if (isRoot)
    {
#endif
	// Record gpu time
	QueryDesc desc = { 2 * pGpuProfiler->pCurrentNode->mGpuTimer.mIndex + 1 };
	cmdEndQuery(pCmd, pGpuProfiler->pQueryPool[pGpuProfiler->mBufferIndex], &desc);

#if (PROFILE_ENABLED)
	// Send data to MicroProfile
	ProfileLeaveGpu(pGpuProfiler->pCurrentNode->mMicroProfileToken, desc.mIndex);
#endif
#if defined(METAL)
    }
#endif
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
#if defined(DIRECT3D12) || defined(VULKAN) || defined(DIRECT3D11)|| defined(METAL)
	// Reset the query pool completely once
	// After this we only reset the number of queries used during that frame to keep GPU and CPU overhead minimum
	if (pGpuProfiler->mReset)
	{
		for (uint32_t i = 0; i < GpuProfiler::NUM_OF_FRAMES; ++i)
			cmdResetQueryPool(pCmd, pGpuProfiler->pQueryPool[i], 0, pGpuProfiler->pQueryPool[i]->mDesc.mQueryCount);

		pGpuProfiler->mReset = false;
	}

	// resolve last frame
	cmdResolveQuery(
		pCmd, pGpuProfiler->pQueryPool[pGpuProfiler->mBufferIndex], pGpuProfiler->pReadbackBuffer[pGpuProfiler->mBufferIndex], 0,
		pGpuProfiler->mCurrentTimerCount * 2);
	cmdResetQueryPool(pCmd, pGpuProfiler->pQueryPool[pGpuProfiler->mBufferIndex], 0, pGpuProfiler->mCurrentTimerCount * 2);
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

#if (PROFILE_ENABLED)
	// Attach GpuProfiler to MicroProfile log of the current thread
	ProfileGpuSetContext(pGpuProfiler);
#endif
	cmdBeginGpuTimestampQuery(pCmd, pGpuProfiler, "GPU", bUseMarker, {1, 1, 0}, true);
}

void cmdEndGpuFrameProfile(Cmd* pCmd, GpuProfiler* pGpuProfiler)
{
	cmdEndGpuTimestampQuery(pCmd, pGpuProfiler, NULL, true);

	for (uint32_t i = 0; i < (uint32_t)pGpuProfiler->mRoot.mChildren.size(); ++i)
	{
		pGpuProfiler->mCumulativeTimeInternal += getAverageGpuTime(pGpuProfiler, &pGpuProfiler->mRoot.mChildren[i]->mGpuTimer);

		pGpuProfiler->mCumulativeCpuTimeInternal += getAverageCpuTime(pGpuProfiler, &pGpuProfiler->mRoot.mChildren[i]->mGpuTimer);
	}

#if defined(DIRECT3D12) || defined(VULKAN) || defined(DIRECT3D11) || defined(METAL)
	// readback n + 1 frame
	ReadRange range = {};
	range.mOffset = 0;
	range.mSize = max(sizeof(uint64_t) * 2, (pGpuProfiler->mCurrentTimerCount) * sizeof(uint64_t) * 2);
	mapBuffer(pCmd->pRenderer, pGpuProfiler->pReadbackBuffer[pGpuProfiler->mBufferIndex], &range);
	pGpuProfiler->pTimeStamp = (uint64_t*)pGpuProfiler->pReadbackBuffer[pGpuProfiler->mBufferIndex]->pCpuMappedAddress;

#if (PROFILE_ENABLED)
	// Copy to data to buffer so we can sample from MicroProfile
	memcpy(pGpuProfiler->pTimeStampBuffer, pGpuProfiler->pTimeStamp, (pGpuProfiler->mCurrentTimerCount) * sizeof(uint64_t) * 2);
#endif
#endif

	calculateTimes(pCmd, pGpuProfiler, &pGpuProfiler->mRoot);

#if defined(DIRECT3D12) || defined(VULKAN) || defined(DIRECT3D11) || defined(METAL)
	unmapBuffer(pCmd->pRenderer, pGpuProfiler->pReadbackBuffer[pGpuProfiler->mBufferIndex]);
	pGpuProfiler->pTimeStamp = NULL;
#endif
}
