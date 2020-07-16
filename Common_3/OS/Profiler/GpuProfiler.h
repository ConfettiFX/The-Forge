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

#pragma once
#include "../Math/MathTypes.h"

struct Cmd;
struct Renderer;
struct Buffer;
struct Queue;
struct QueryPool;
struct ProfileThreadLog;
typedef uint64_t ProfileToken;

typedef struct GpuTimer
{
	static const uint32_t LENGTH_OF_HISTORY = 60;

    char mName[64] =    "Timer";
	uint32_t             mIndex;
	uint32_t             mHistoryIndex;
    uint32_t             mDepth;

	uint64_t mStartGpuTime;
	uint64_t mEndGpuTime;
	uint64_t mGpuTime;
    uint64_t mGpuMinTime;
    uint64_t mGpuMaxTime;
	uint64_t mGpuHistory[LENGTH_OF_HISTORY];
    ProfileToken mToken;
    ProfileToken mMicroProfileToken;
    GpuTimer* pParent;
	bool mDebugMarker;
    bool mStarted;


} GpuTimer;

typedef struct GpuProfiler
{
	// double buffered
	static const uint32_t NUM_OF_FRAMES = 3;
	static const uint32_t MAX_TIMERS = 512;

    Renderer*             pRenderer;
	Buffer*               pReadbackBuffer[NUM_OF_FRAMES];
	QueryPool*            pQueryPool[NUM_OF_FRAMES];
	uint64_t*             pTimeStamp;
	double                mGpuTimeStampFrequency;

	uint32_t mProfilerIndex;
	uint32_t mBufferIndex;
	uint32_t mCurrentTimerCount;
	uint32_t mCurrentPoolIndex;

    GpuTimer*                    pGpuTimerPool;
    GpuTimer*                    pCurrentNode;

	// MicroProfile
	char mGroupName[256] = "GPU";
	ProfileThreadLog * pLog = nullptr;

	bool mReset = true;
	bool mUpdate;
} GpuProfiler;

struct GpuProfilerContainer
{
    static const uint32_t MAX_GPU_PROFILERS = 8;
    GpuProfiler* mProfilers[MAX_GPU_PROFILERS] = { NULL };
    uint32_t mSize = 0;
};