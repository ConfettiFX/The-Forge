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

typedef struct GpuTimer
{
	const static int32_t LENGTH_OF_HISTORY = 60;
	tinystl::string      mName;
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
	tinystl::vector<GpuTimerTree*> mChildren;
	bool                           mDebugMarker;
} GpuTimerTree;

typedef struct GpuProfiler
{
	// double buffered
	const static uint32_t NUM_OF_FRAMES = 2;
	Buffer*               pReadbackBuffer[NUM_OF_FRAMES];
	QueryHeap*            pQueryHeap[NUM_OF_FRAMES];
	uint64_t*             pTimeStamp;
	double                mGpuTimeStampFrequency;
	double                mCpuTimeStampFrequency;

	uint32_t mBufferIndex;
	uint32_t mMaxTimerCount;
	uint32_t mCurrentTimerCount;
	uint32_t mCurrentPoolIndex;

	tinystl::unordered_map<uint32_t, uint32_t> mGpuPoolHash;
	GpuTimerTree*                              pGpuTimerPool;
	GpuTimerTree                               mRoot;
	GpuTimerTree*                              pCurrentNode;

	double mCumulativeTimeInternal;
	double mCumulativeTime;

	double mCumulativeCpuTimeInternal;
	double mCumulativeCpuTime;

	bool mUpdate;
} GpuProfiler;

double getAverageGpuTime(struct GpuProfiler* pGpuProfiler, struct GpuTimer* pGpuTimer);
double getAverageCpuTime(struct GpuProfiler* pGpuProfiler, struct GpuTimer* pGpuTimer);

void addGpuProfiler(Renderer* pRenderer, Queue* pQueue, struct GpuProfiler** ppGpuProfiler, uint32_t maxTimers = 4096);
void removeGpuProfiler(Renderer* pRenderer, struct GpuProfiler* pGpuProfiler);

void cmdBeginGpuTimestampQuery(
	Cmd* pCmd, struct GpuProfiler* pGpuProfiler, const char* pName, bool addMarker = false, const float3& color = { 1, 1, 0 });
void cmdEndGpuTimestampQuery(Cmd* pCmd, struct GpuProfiler* pGpuProfiler, GpuTimer** ppGpuTimer = NULL);

/// Must be called before any call to cmdBeginGpuTimestampQuery
/// Preferred time to call this function is right after calling beginCmd
void cmdBeginGpuFrameProfile(Cmd* pCmd, GpuProfiler* pGpuProfiler, bool bUseMarker = false);
/// Must be called after all gpu profiles are finished.
/// This function cannot be called inside a render pass (cmdBeginRender-cmdEndRender)
/// Preferred time to call this function is right before calling endCmd
void cmdEndGpuFrameProfile(Cmd* pCmd, GpuProfiler* pGpuProfiler);
