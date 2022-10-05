/*
 * Copyright (c) 2017-2022 The Forge Interactive Inc.
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
#include "../../Utilities/Math/MathTypes.h"

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
	uint32_t             mIndex = 0;
	uint32_t             mHistoryIndex = 0;
    uint32_t             mDepth = 0;

	uint64_t mStartGpuTime = 0;
	uint64_t mEndGpuTime = 0;
	uint64_t mGpuTime = 0;
    uint64_t mGpuMinTime = 0;
    uint64_t mGpuMaxTime = 0;
	uint64_t mGpuHistory[LENGTH_OF_HISTORY] = {};
	ProfileToken mToken = {};
	ProfileToken mMicroProfileToken = {};
    GpuTimer* pParent = NULL;
	bool mDebugMarker = false;
    bool mStarted = false;


} GpuTimer;

typedef struct GpuProfiler
{
	// double buffered
	static const uint32_t NUM_OF_FRAMES = 3;
	static const uint32_t MAX_TIMERS = 512;

	Renderer*             pRenderer = {};
	Buffer*               pReadbackBuffer[NUM_OF_FRAMES] = {};
	QueryPool*            pQueryPool[NUM_OF_FRAMES] = {};
	uint64_t*             pTimeStamp = NULL;
	double                mGpuTimeStampFrequency = 0.0;

	uint32_t mProfilerIndex = 0;
	uint32_t mBufferIndex = 0;
	uint32_t mCurrentTimerCount = 0;
	uint32_t mMaxTimerCount = 0;
	uint32_t mCurrentPoolIndex = 0;

    GpuTimer*                    pGpuTimerPool = NULL;
    GpuTimer*                    pCurrentNode = NULL;

	// MicroProfile
	char mGroupName[256] = "GPU";
	ProfileThreadLog * pLog = nullptr;

	bool mReset = true;
	bool mUpdate = false;
} GpuProfiler;

struct GpuProfilerContainer
{
    static const uint32_t MAX_GPU_PROFILERS = 8;
    GpuProfiler* mProfilers[MAX_GPU_PROFILERS] = { NULL };
    uint32_t mSize = 0;
};