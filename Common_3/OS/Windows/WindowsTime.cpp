/*
 * Copyright (c) 2018-2021 The Forge Interactive Inc.
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

#include <time.h>
#include <stdint.h>
#include <windows.h>

/************************************************************************/
// Time Related Functions
/************************************************************************/
uint32_t getSystemTime() { return (uint32_t)timeGetTime(); }

uint32_t getTimeSinceStart() { return (uint32_t)time(NULL); }

static int64_t highResTimerFrequency = 0;

void initTime()
{
	LARGE_INTEGER frequency;
	if (QueryPerformanceFrequency(&frequency))
	{
		highResTimerFrequency = frequency.QuadPart;
	}
	else
	{
		highResTimerFrequency = 1000LL;
	}
}


int64_t getTimerFrequency()
{
	if (highResTimerFrequency == 0)
		initTime();

	return highResTimerFrequency;
}

// Make sure timer frequency is initialized before anyone tries to use it
struct StaticTime
{
	StaticTime() { initTime(); }
} staticTimeInst;

int64_t getUSec()
{
	LARGE_INTEGER counter;
	QueryPerformanceCounter(&counter);
	return counter.QuadPart * (int64_t)1e6 / getTimerFrequency();
}
