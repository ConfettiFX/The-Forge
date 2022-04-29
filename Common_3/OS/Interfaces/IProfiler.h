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

#include "../../Renderer/RendererConfig.h"

#include "ILog.h"
#include "IApp.h"
#include "IOperatingSystem.h"
#include "IThread.h"
#include "../Math/MathTypes.h"

typedef uint64_t ProfileToken;
#define PROFILE_INVALID_TOKEN (uint64_t) - 1

struct Cmd;
struct Renderer;
struct Queue;
struct FontDrawDesc; 
struct UserInterface;

typedef struct ProfilerDesc
{

	Renderer*     pRenderer = NULL; 
	Queue**       ppQueues = NULL;

	const char**  ppProfilerNames = NULL; 
	ProfileToken* pProfileTokens = NULL; 

	uint32_t      mGpuProfilerCount = 0;
	uint32_t      mWidthUI = 0; 
	uint32_t      mHeightUI = 0; 

} ProfilerDesc;

// Must be called before adding any profiling
void initProfiler(ProfilerDesc* pDesc);

// Call on application exit
void exitProfiler();

// Call once per frame to update profiler
void flipProfiler();

// Set amount of frames before aggregation
void setAggregateFrames(uint32_t nFrames);

// Dump profile data to "profile-(date).html" of recorded frames, until a maximum amount of frames
void dumpProfileData(const char* appName = "" , uint32_t nMaxFrames = 64);

// Dump benchmark data to "benchmark-(data).txt" of recorded frames
void dumpBenchmarkData(IApp::Settings* pSettings, const char* outFilename = "", const char * appName = "");

//------ Profiler UI Widget --------//

// Call once per frame before AppUI.Draw, draw requested Gpu profiler timers
// Returns text dimensions so caller can align other UI elements
float2 cmdDrawGpuProfile(Cmd* pCmd, float2 screenCoordsInPx, ProfileToken nProfileToken, FontDrawDesc* pDrawDesc);

// Call once per frame before AppUI.Draw, draw requested Cpu profile time
// Returns text dimensions so caller can align other UI elements
float2 cmdDrawCpuProfile(Cmd* pCmd, float2 screenCoordsInPx, FontDrawDesc* pDrawDesc);

// Toggle profiler display on/off.
void toggleProfilerUI();

//------ Gpu profiler ------------//

// Call only after initProfiler(), for manually adding Gpu Profilers
ProfileToken addGpuProfiler(Renderer* pRenderer, Queue* pQueue, const char* pProfilerName);

// Call only before exitProfiler(), for manually removing Gpu Profilers
void removeGpuProfiler(ProfileToken nProfileToken);

// Must be called before any call to cmdBeginGpuTimestampQuery
// Preferred time to call this function is right after calling beginCmd
void cmdBeginGpuFrameProfile(Cmd* pCmd, ProfileToken nProfileToken, bool bUseMarker = true);

// Must be called after all gpu profiles are finished.
// This function cannot be called inside a render pass (cmdBeginRender-cmdEndRender)
// Preferred time to call this function is right before calling endCmd
void cmdEndGpuFrameProfile(Cmd* pCmd, ProfileToken nProfileToken);

ProfileToken cmdBeginGpuTimestampQuery(Cmd* pCmd, ProfileToken nProfileToken, const char* pName, bool bUseMarker = true);

void cmdEndGpuTimestampQuery(Cmd* pCmd, ProfileToken nProfileToken);

// Gpu times in milliseconds
float getGpuProfileTime(ProfileToken nProfileToken);
float getGpuProfileAvgTime(ProfileToken nProfileToken);
float getGpuProfileMinTime(ProfileToken nProfileToken);
float getGpuProfileMaxTime(ProfileToken nProfileToken);

uint64_t getGpuProfileTicksPerSecond(ProfileToken nProfileToken);

//------ Cpu profiler ------------//

uint64_t cpuProfileEnter(ProfileToken nToken);

void cpuProfileLeave(ProfileToken nToken, uint64_t nTick);

ProfileToken getCpuProfileToken(const char* pGroup, const char* pName, uint32_t nColor);

struct CpuProfileScopeMarker
{
	ProfileToken nToken;
	uint64_t     nTick;
	CpuProfileScopeMarker(const char* pGroup, const char* pName, uint32_t nColor)
	{
		nToken = getCpuProfileToken(pGroup, pName, nColor);
		nTick = cpuProfileEnter(nToken);
	}
	~CpuProfileScopeMarker() { cpuProfileLeave(nToken, nTick); }
};

#define PROFILER_CONCAT0(a, b) a##b
#define PROFILER_CONCAT(a, b) PROFILER_CONCAT0(a, b)
// Call at the start of a block to profile cpu time between '{' '}'
#define PROFILER_SET_CPU_SCOPE(group, name, color) CpuProfileScopeMarker PROFILER_CONCAT(marker, __LINE__)(group, name, color)

// Cpu times in milliseconds
float getCpuProfileTime(const char* pGroup, const char* pName, ThreadID* pThreadID = NULL);
float getCpuProfileAvgTime(const char* pGroup, const char* pName, ThreadID* pThreadID = NULL);
float getCpuProfileMinTime(const char* pGroup, const char* pName, ThreadID* pThreadID = NULL);
float getCpuProfileMaxTime(const char* pGroup, const char* pName, ThreadID* pThreadID = NULL);

float getCpuFrameTime();
float getCpuAvgFrameTime();
float getCpuMinFrameTime();
float getCpuMaxFrameTime();