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

#pragma once

#include "../OS/Math/MathTypes.h"
#include "../ThirdParty/OpenSource/EASTL/string.h"
#include "../ThirdParty/OpenSource/EASTL/string_hash_map.h"
#include "../ThirdParty/OpenSource/EASTL/vector.h"

#include <stdint.h>

struct Cmd;
struct Renderer;
struct Buffer;
struct Queue;
struct QueryPool;
struct ProfileThreadLog;

typedef struct GpuTimer
{
	static const uint32_t LENGTH_OF_HISTORY = 60;

	eastl::string      mName;
	uint32_t             mIndex;
	uint32_t             mHistoryIndex;

	int64_t mStartGpuTime;
	int64_t mEndGpuTime;
	int64_t mGpuTime;
	int64_t mGpuHistory[LENGTH_OF_HISTORY];

	int64_t mStartCpuTime;
	int64_t mEndCpuTime;
	int64_t mCpuTime;
	int64_t mCpuHistory[LENGTH_OF_HISTORY];

} GpuTimer;

typedef struct GpuTimerTree
{
	GpuTimerTree*                  pParent;
	GpuTimer                       mGpuTimer;
	eastl::vector<GpuTimerTree*>   mChildren;
	uint64_t                       mMicroProfileToken;
	bool                           mDebugMarker;
} GpuTimerTree;

typedef struct GpuProfiler
{
	// double buffered
	static const uint32_t NUM_OF_FRAMES = 2;
	static const uint32_t MAX_TIMERS = 4096;

	Buffer*               pReadbackBuffer[NUM_OF_FRAMES];
	QueryPool*            pQueryPool[NUM_OF_FRAMES];
	uint64_t*             pTimeStamp;
	uint64_t*             pTimeStampBuffer;
	double                mGpuTimeStampFrequency;
	double                mCpuTimeStampFrequency;

	uint32_t mBufferIndex;
	uint32_t mMaxTimerCount;
	uint32_t mCurrentTimerCount;
	uint32_t mCurrentPoolIndex;

	eastl::string_hash_map<uint32_t> mGpuPoolHash;
	GpuTimerTree*                    pGpuTimerPool;
	GpuTimerTree                     mRoot;
	GpuTimerTree*                    pCurrentNode;

	double mCumulativeTimeInternal;
	double mCumulativeTime;

	double mCumulativeCpuTimeInternal;
	double mCumulativeCpuTime;

	// MicroProfile
	char mGroupName[256] = "GPU";
	ProfileThreadLog * pLog = nullptr;

	bool mReset = true;
	bool mUpdate;
} GpuProfiler;

double getAverageGpuTime(struct GpuProfiler* pGpuProfiler, struct GpuTimer* pGpuTimer);
double getAverageCpuTime(struct GpuProfiler* pGpuProfiler, struct GpuTimer* pGpuTimer);

void addGpuProfiler(Renderer* pRenderer, Queue* pQueue, struct GpuProfiler** ppGpuProfiler, const char * pName, uint32_t maxTimers = GpuProfiler::MAX_TIMERS);
void removeGpuProfiler(Renderer* pRenderer, struct GpuProfiler* pGpuProfiler);

void cmdBeginGpuTimestampQuery(Cmd* pCmd, struct GpuProfiler* pGpuProfiler, const char* pName, bool addMarker = true, const float3& color = { 1,1,0 }, bool isRoot = false);

void cmdEndGpuTimestampQuery(Cmd* pCmd, struct GpuProfiler* pGpuProfiler, GpuTimer** ppGpuTimer = NULL, bool isRoot = false);

// Must be called before any call to cmdBeginGpuTimestampQuery
// Preferred time to call this function is right after calling beginCmd
void cmdBeginGpuFrameProfile(Cmd* pCmd, GpuProfiler* pGpuProfiler, bool bUseMarker = true);
// Must be called after all gpu profiles are finished.
// This function cannot be called inside a render pass (cmdBeginRender-cmdEndRender)
// Preferred time to call this function is right before calling endCmd
void cmdEndGpuFrameProfile(Cmd* pCmd, GpuProfiler* pGpuProfiler);
