/*
 * Copyright (c) 2018-2020 The Forge Interactive Inc.
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
#include "../Interfaces/IProfiler.h"

#if 0 == GPU_PROFILER_SUPPORTED
ProfileToken addGpuProfiler(Renderer* pRenderer, Queue* pQueue, const char* pName) { return PROFILE_INVALID_TOKEN; }
void cmdBeginGpuFrameProfile(Cmd* pCmd, ProfileToken nProfileToken, bool bUseMarker) {}
void cmdEndGpuFrameProfile(Cmd* pCmd, ProfileToken nProfileToken) {}
ProfileToken cmdBeginGpuTimestampQuery(Cmd* pCmd, ProfileToken nProfileToken, const char* pName, bool bUseMarker) { return PROFILE_INVALID_TOKEN; }
void cmdEndGpuTimestampQuery(Cmd* pCmd, ProfileToken nProfileToken) {}
float getGpuProfileTime(ProfileToken nProfileToken) { return -1.0f; }
float getGpuProfileAvgTime(ProfileToken nProfileToken) { return -1.0f; }
float getGpuProfileMinTime(ProfileToken nProfileToken) { return -1.0f; }
float getGpuProfileMaxTime(ProfileToken nProfileToken) { return -1.0f; }
uint64_t getGpuProfileTicksPerSecond(ProfileToken nProfileToken) { return 0; }
GpuProfiler* getGpuProfiler(ProfileToken nProfileToken) { return NULL; }
#else

#include "ProfilerBase.h"

#include "../../Renderer/IRenderer.h"
#include "../../Renderer/IResourceLoader.h"

#include "../Interfaces/ILog.h"
#include "../Interfaces/ITime.h"
#include "../Interfaces/IMemory.h"

extern void mapBuffer(Renderer* pRenderer, Buffer* pBuffer, ReadRange* pRange);
extern void unmapBuffer(Renderer* pRenderer, Buffer* pBuffer);

GpuProfilerContainer* gGpuProfilerContainer = NULL;

inline ProfileToken getProfileToken(uint32_t nProfilerIndex, uint32_t nTimerIndex)
{
    return ((uint64_t)nProfilerIndex << 32) | nTimerIndex;
}

inline uint32_t getProfileIndex(ProfileToken nToken)
{
    return nToken >> 32;
}

inline uint32_t getTimerIndex(ProfileToken nToken)
{
    return nToken & 0xffff;
}

GpuProfiler* getGpuProfiler(ProfileToken nProfileToken)
{
    if (nProfileToken == PROFILE_INVALID_TOKEN)
        return NULL;

    return gGpuProfilerContainer->mProfilers[getProfileIndex(nProfileToken)];
}

static void calculateTimes(Cmd* pCmd, GpuProfiler* pGpuProfiler, uint32_t index)
{
    GpuTimer* pRoot = &pGpuProfiler->pGpuTimerPool[index];
    if (!pRoot || !pRoot->mStarted)
        return;

    ASSERT(pGpuProfiler->pTimeStamp != NULL && "Time stamp readback buffer is not mapped");

    uint64_t  elapsedTime = 0;
    const uint32_t historyIndex = pRoot->mHistoryIndex;

    const uint32_t id = pRoot->mIndex;
    const uint64_t timeStamp1 = pGpuProfiler->pTimeStamp[id * 2];
    const uint64_t timeStamp2 = pGpuProfiler->pTimeStamp[id * 2 + 1];

    elapsedTime = timeStamp2 - timeStamp1;
    if (timeStamp2 <= timeStamp1)
    {
        elapsedTime = 0;
    }
    else
    {
        pRoot->mStartGpuTime = timeStamp1;
        pRoot->mEndGpuTime = timeStamp2;
        pRoot->mGpuTime = elapsedTime;
        pRoot->mGpuMinTime = min(pRoot->mGpuMinTime, elapsedTime);
        pRoot->mGpuMaxTime = max(pRoot->mGpuMaxTime, elapsedTime);
    }
    pRoot->mGpuHistory[historyIndex] = elapsedTime;

    pRoot->mHistoryIndex = (historyIndex + 1) % GpuTimer::LENGTH_OF_HISTORY;

    // Send data to MicroProfile
    {
        MutexLock lock(ProfileGetMutex());
        Profile* S = ProfileGet();
        if (S->nRunning && pRoot->mMicroProfileToken != PROFILE_INVALID_TOKEN)
        {
            ProfileEnterGpu(pRoot->mMicroProfileToken, pRoot->mStartGpuTime, pGpuProfiler->pLog);

			uint16_t timerIndex = ProfileGetTimerIndex(pRoot->mMicroProfileToken);
			S->Frame[timerIndex].nCount = 1;
			S->Frame[timerIndex].nTicks = elapsedTime;
			S->AccumTimers[timerIndex].nTicks += S->Frame[timerIndex].nTicks;
			S->AccumTimers[timerIndex].nCount += S->Frame[timerIndex].nCount;
			S->AccumMinTimers[timerIndex] = ProfileMin(S->AccumMinTimers[timerIndex], S->Frame[timerIndex].nTicks);
			S->AccumMaxTimers[timerIndex] = ProfileMax(S->AccumMaxTimers[timerIndex], S->Frame[timerIndex].nTicks);
        }  
    }

    for (uint32_t i = index + 1; i < pGpuProfiler->mCurrentPoolIndex; ++i)
    {
        if (pGpuProfiler->pGpuTimerPool[i].pParent == pRoot)
        {
            calculateTimes(pCmd, pGpuProfiler, i);
        }
    }
    pRoot->mStarted = false; // Reset
    {
        MutexLock lock(ProfileGetMutex());
        Profile* S = ProfileGet();
        if (S->nRunning && pRoot->mMicroProfileToken != PROFILE_INVALID_TOKEN)
        {
            ProfileLeaveGpu(pRoot->mMicroProfileToken, pRoot->mEndGpuTime, pGpuProfiler->pLog);
        }
    }
}

double getAverageGpuTime(struct GpuProfiler* pGpuProfiler, struct GpuTimer* pGpuTimer)
{
	uint64_t elapsedTime = 0;

	for (uint32_t i = 0; i < GpuTimer::LENGTH_OF_HISTORY; ++i)
	{
		elapsedTime += pGpuTimer->mGpuHistory[i];
	}

	return ((elapsedTime / GpuTimer::LENGTH_OF_HISTORY) / pGpuProfiler->mGpuTimeStampFrequency) * 1000.0;
}

void addGpuProfiler(Renderer* pRenderer, Queue* pQueue, GpuProfiler** ppGpuProfiler, const char * pName)
{
	GpuProfiler* pGpuProfiler = (GpuProfiler*)tf_calloc(1, sizeof(*pGpuProfiler));
	ASSERT(pGpuProfiler);

	tf_placement_new<GpuProfiler>(pGpuProfiler);
	pGpuProfiler->mReset = true;
    pGpuProfiler->pRenderer = pRenderer;
    strncpy(pGpuProfiler->mGroupName, pName, 256);

	const uint32_t nodeIndex = pQueue->mNodeIndex;
	QueryPoolDesc  queryHeapDesc = {};
	queryHeapDesc.mNodeIndex = nodeIndex;
	queryHeapDesc.mQueryCount = GpuProfiler::MAX_TIMERS * 2;
	queryHeapDesc.mType = QUERY_TYPE_TIMESTAMP;

	BufferDesc bufDesc = {};
#if defined(DIRECT3D11) || defined(METAL)
	bufDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_ONLY;
#else
	bufDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_TO_CPU;
#endif
	bufDesc.mFlags = BUFFER_CREATION_FLAG_OWN_MEMORY_BIT;
	bufDesc.mSize = GpuProfiler::MAX_TIMERS * sizeof(uint64_t) * 2;
	bufDesc.pName = "GPU Profiler ReadBack Buffer";
	bufDesc.mNodeIndex = nodeIndex;
	bufDesc.mStartState = RESOURCE_STATE_COPY_DEST;

	for (uint32_t i = 0; i < GpuProfiler::NUM_OF_FRAMES; ++i)
	{
		addQueryPool(pRenderer, &queryHeapDesc, &pGpuProfiler->pQueryPool[i]);
		BufferLoadDesc loadDesc = {};
		loadDesc.mDesc = bufDesc;
		loadDesc.ppBuffer = &pGpuProfiler->pReadbackBuffer[i];
		addResource(&loadDesc, NULL);
	}

	getTimestampFrequency(pQueue, &pGpuProfiler->mGpuTimeStampFrequency);

	// Create buffer to sample from MicroProfile and log for current GpuProfiler
    pGpuProfiler->pLog = ProfileCreateThreadLog(pName);
	pGpuProfiler->pLog->nGpu = 1;
    pGpuProfiler->pLog->nGpuToken = getProfileToken(pGpuProfiler->mProfilerIndex, 0);

	pGpuProfiler->pGpuTimerPool = (GpuTimer*)tf_calloc(GpuProfiler::MAX_TIMERS, sizeof(*pGpuProfiler->pGpuTimerPool));
	pGpuProfiler->pCurrentNode = &pGpuProfiler->pGpuTimerPool[0];
	pGpuProfiler->mCurrentPoolIndex = 0;

	*ppGpuProfiler = pGpuProfiler;
}

void removeGpuProfiler(struct GpuProfiler* pGpuProfiler)
{
	for (uint32_t i = 0; i < GpuProfiler::NUM_OF_FRAMES; ++i)
	{
		removeResource(pGpuProfiler->pReadbackBuffer[i]);
		removeQueryPool(pGpuProfiler->pRenderer, pGpuProfiler->pQueryPool[i]);
	}


	ProfileRemoveThreadLog(pGpuProfiler->pLog);

	tf_free(pGpuProfiler->pGpuTimerPool);
	tf_free(pGpuProfiler);
}

ProfileToken cmdBeginGpuTimestampQuery(Cmd* pCmd, struct GpuProfiler* pGpuProfiler, const char* pName, bool addMarker = true, const float3& color = { 1,1,0 }, bool isRoot = false)
{
    GpuTimer* node = NULL;
    for (uint32_t i = 0; i < pGpuProfiler->mCurrentPoolIndex; ++i)
    {
        GpuTimer* tNode = &pGpuProfiler->pGpuTimerPool[i];
        if (!P_STRCASECMP(tNode->mName, pName))
        {
            node = tNode;
            break;
        }
    }

    if (!node)
    {
        // first time seeing this
        node = &pGpuProfiler->pGpuTimerPool[pGpuProfiler->mCurrentPoolIndex];
        strncpy(node->mName, pName, 64);
        node->mHistoryIndex = 0;
        node->mGpuMaxTime = 0;
        node->mGpuMinTime = -1;
        node->mStartGpuTime = isRoot ? 0 : pGpuProfiler->pCurrentNode->mStartGpuTime;
        node->mEndGpuTime = 0;
        node->mToken = getProfileToken(pGpuProfiler->mProfilerIndex, pGpuProfiler->mCurrentPoolIndex);
        memset(node->mGpuHistory, 0, sizeof(node->mGpuHistory));
        uint32_t scope_color = static_cast<uint32_t>(color.getX() * 255) << 16
            | static_cast<uint32_t>(color.getY() * 255) << 8
            | static_cast<uint32_t>(color.getZ() * 255);

        node->mMicroProfileToken = ProfileGetToken(pGpuProfiler->mGroupName, pName, scope_color, ProfileTokenTypeGpu);

        if (isRoot)
        {
            Profile* S = ProfileGet();
            uint16_t groupIndex = ProfileGetGroupIndex(node->mMicroProfileToken);
            S->GroupInfo[groupIndex].nGpuProfileToken = getProfileToken(pGpuProfiler->mProfilerIndex, 0);
        }

        ++pGpuProfiler->mCurrentPoolIndex;
    }

    // Record gpu time
    node->mIndex = pGpuProfiler->mCurrentTimerCount;
    node->pParent = isRoot ? NULL : pGpuProfiler->pCurrentNode;
    node->mDepth = isRoot ? 0 : node->pParent->mDepth + 1;
    node->mStarted = true;
    node->mDebugMarker = addMarker;

    if (!isRoot)
    {
        pGpuProfiler->pCurrentNode = node;
    }

	// Metal only supports gpu timers on command buffer boundaries
#if defined(METAL)
	if (isRoot)
#endif
	{
		QueryDesc desc = { 2 * node->mIndex };
		cmdBeginQuery(pCmd, pGpuProfiler->pQueryPool[pGpuProfiler->mBufferIndex], &desc);
	}

    if (addMarker)
    {
        cmdBeginDebugMarker(pCmd, color.getX(), color.getY(), color.getZ(), pName);
    }

    ASSERT(pGpuProfiler->mCurrentTimerCount < pGpuProfiler->mCurrentPoolIndex && "Duplicate timers found in one gpu frame");
    ++pGpuProfiler->mCurrentTimerCount;
    return node->mToken;
}

void cmdEndGpuTimestampQuery(Cmd* pCmd, struct GpuProfiler* pGpuProfiler, bool isRoot = false)
{
	// Metal only supports gpu timers on command buffer boundaries
#if defined(METAL)
	if (isRoot)
#endif
	{
		// Record gpu time
		QueryDesc desc = { 2 * pGpuProfiler->pCurrentNode->mIndex + 1 };
		cmdEndQuery(pCmd, pGpuProfiler->pQueryPool[pGpuProfiler->mBufferIndex], &desc);
	}

    if (pGpuProfiler->pCurrentNode->mDebugMarker)
    {
        cmdEndDebugMarker(pCmd);
    }

    pGpuProfiler->pCurrentNode = pGpuProfiler->pCurrentNode->pParent;
}

void initGpuProfilers()
{
    gGpuProfilerContainer = (GpuProfilerContainer*)tf_calloc(1, sizeof(*gGpuProfilerContainer));
    ASSERT(gGpuProfilerContainer);
    tf_placement_new<GpuProfilerContainer>(gGpuProfilerContainer);
}

void exitGpuProfilers()
{
    for (uint32_t i = 0; i < GpuProfilerContainer::MAX_GPU_PROFILERS; ++i)
    {
        if (gGpuProfilerContainer->mProfilers[i])
        {
            removeGpuProfiler(gGpuProfilerContainer->mProfilers[i]);
            gGpuProfilerContainer->mProfilers[i] = NULL;
        }
    }
    gGpuProfilerContainer->mSize = 0;
    tf_free(gGpuProfilerContainer);
}

ProfileToken addGpuProfiler(Renderer* pRenderer, Queue* pQueue, const char* pName)
{
    if(gGpuProfilerContainer->mSize >= GpuProfilerContainer::MAX_GPU_PROFILERS)
    {
        Log::Write(LogLevel::eWARNING, __FILE__, __LINE__, "Reached maximum amount of Gpu Profilers");
            return PROFILE_INVALID_TOKEN;
    }

    GpuProfiler* pGpuProfiler;
    addGpuProfiler(pRenderer, pQueue, &pGpuProfiler, pName);
    ASSERT(pGpuProfiler);

    for (uint32_t i = 0; i < GpuProfilerContainer::MAX_GPU_PROFILERS; ++i)
    {
        if (!gGpuProfilerContainer->mProfilers[i])
        {
            gGpuProfilerContainer->mProfilers[i] = pGpuProfiler;
            ++gGpuProfilerContainer->mSize;
            pGpuProfiler->mProfilerIndex = i;
            break;
        }
    }
    return getProfileToken(pGpuProfiler->mProfilerIndex, 0);
}

void removeGpuProfiler(ProfileToken nProfileToken)
{
    GpuProfiler* pGpuProfiler = getGpuProfiler(nProfileToken);
    if (!pGpuProfiler)
        return;
    removeGpuProfiler(pGpuProfiler);
    gGpuProfilerContainer->mProfilers[getProfileIndex(nProfileToken)] = NULL;
    --gGpuProfilerContainer->mSize;
}

void cmdBeginGpuFrameProfile(Cmd * pCmd, ProfileToken nProfileToken, bool bUseMarker)
{
    GpuProfiler* pGpuProfiler = getGpuProfiler(nProfileToken);
    if (!pGpuProfiler)
        return;

    // Reset the query pool completely once
    // After this we only reset the number of queries used during that frame to keep GPU and CPU overhead minimum
    if (pGpuProfiler->mReset)
    {
        for (uint32_t i = 0; i < GpuProfiler::NUM_OF_FRAMES; ++i)
            cmdResetQueryPool(pCmd, pGpuProfiler->pQueryPool[i], 0, pGpuProfiler->pQueryPool[i]->mCount);

        pGpuProfiler->mReset = false;
    }

    // resolve last frame
    cmdResolveQuery(
        pCmd, pGpuProfiler->pQueryPool[pGpuProfiler->mBufferIndex], pGpuProfiler->pReadbackBuffer[pGpuProfiler->mBufferIndex], 0,
        pGpuProfiler->mCurrentTimerCount * 2);
    cmdResetQueryPool(pCmd, pGpuProfiler->pQueryPool[pGpuProfiler->mBufferIndex], 0, pGpuProfiler->mCurrentTimerCount * 2);

    uint32_t nextIndex = (pGpuProfiler->mBufferIndex + 1) % GpuProfiler::NUM_OF_FRAMES;
    pGpuProfiler->mBufferIndex = nextIndex;

    cmdResetQueryPool(pCmd, pGpuProfiler->pQueryPool[pGpuProfiler->mBufferIndex], 0, pGpuProfiler->mCurrentTimerCount * 2);

    pGpuProfiler->mCurrentTimerCount = 0;

    cmdBeginGpuTimestampQuery(pCmd, pGpuProfiler, pGpuProfiler->mGroupName, bUseMarker, { 1, 1, 0 }, true);
    pGpuProfiler->pCurrentNode = &pGpuProfiler->pGpuTimerPool[0];
}

void cmdEndGpuFrameProfile(Cmd* pCmd, ProfileToken nProfileToken)
{
    GpuProfiler* pGpuProfiler = getGpuProfiler(nProfileToken);
    if (!pGpuProfiler)
        return;

    cmdEndGpuTimestampQuery(pCmd, pGpuProfiler, true);

    // readback n + 1 frame
    ReadRange range = {};
    range.mOffset = 0;
    range.mSize = max(sizeof(uint64_t) * 2, (pGpuProfiler->mCurrentTimerCount) * sizeof(uint64_t) * 2);
    mapBuffer(pCmd->pRenderer, pGpuProfiler->pReadbackBuffer[pGpuProfiler->mBufferIndex], &range);
    pGpuProfiler->pTimeStamp = (uint64_t*)pGpuProfiler->pReadbackBuffer[pGpuProfiler->mBufferIndex]->pCpuMappedAddress;

    calculateTimes(pCmd, pGpuProfiler, 0);

    unmapBuffer(pCmd->pRenderer, pGpuProfiler->pReadbackBuffer[pGpuProfiler->mBufferIndex]);
    pGpuProfiler->pTimeStamp = NULL;
}

ProfileToken cmdBeginGpuTimestampQuery(Cmd* pCmd, ProfileToken nProfileToken, const char* pName, bool bUseMarker)
{
    GpuProfiler* pGpuProfiler = getGpuProfiler(nProfileToken);
    if (!pGpuProfiler)
        return PROFILE_INVALID_TOKEN;

    return cmdBeginGpuTimestampQuery(pCmd, pGpuProfiler, pName, bUseMarker);
}

void cmdEndGpuTimestampQuery(Cmd* pCmd, ProfileToken nProfileToken)
{
    GpuProfiler* pGpuProfiler = getGpuProfiler(nProfileToken);
    if (!pGpuProfiler)
        return;

    cmdEndGpuTimestampQuery(pCmd, pGpuProfiler);
}

float getGpuProfileTime(ProfileToken nProfileToken)
{
    GpuProfiler* pGpuProfiler = getGpuProfiler(nProfileToken);
    if (!pGpuProfiler)
        return -1.0f;

    GpuTimer* pGpuTimer = &pGpuProfiler->pGpuTimerPool[getTimerIndex(nProfileToken)];
    if (!pGpuTimer)
        return -1.0f;

    return (float)(pGpuTimer->mGpuTime / pGpuProfiler->mGpuTimeStampFrequency) * 1000.0f;
}

float getGpuProfileAvgTime(ProfileToken nProfileToken)
{
    GpuProfiler* pGpuProfiler = getGpuProfiler(nProfileToken);
    if (!pGpuProfiler)
        return -1.0f;

    GpuTimer* pGpuTimer = &pGpuProfiler->pGpuTimerPool[getTimerIndex(nProfileToken)];
    if (!pGpuTimer)
        return -1.0f;

    return (float)getAverageGpuTime(pGpuProfiler, pGpuTimer);
}

float getGpuProfileMinTime(ProfileToken nProfileToken)
{
    GpuProfiler* pGpuProfiler = getGpuProfiler(nProfileToken);
    if (!pGpuProfiler)
        return -1.0f;

    GpuTimer* pGpuTimer = &pGpuProfiler->pGpuTimerPool[getTimerIndex(nProfileToken)];
    if (!pGpuTimer)
        return -1.0f;

    return (float)(pGpuTimer->mGpuMinTime / pGpuProfiler->mGpuTimeStampFrequency) * 1000.0f;
}

float getGpuProfileMaxTime(ProfileToken nProfileToken)
{
    GpuProfiler* pGpuProfiler = getGpuProfiler(nProfileToken);
    if (!pGpuProfiler)
        return -1.0f;

    GpuTimer* pGpuTimer = &pGpuProfiler->pGpuTimerPool[getTimerIndex(nProfileToken)];
    if (!pGpuTimer)
        return -1.0f;

    return (float)(pGpuTimer->mGpuMaxTime / pGpuProfiler->mGpuTimeStampFrequency) * 1000.0f;
}

uint64_t getGpuProfileTicksPerSecond(ProfileToken nProfileToken)
{
    GpuProfiler* pGpuProfiler = getGpuProfiler(nProfileToken);
    if (!pGpuProfiler)
        return 0;
    return (uint64_t)pGpuProfiler->mGpuTimeStampFrequency;
}
#endif
