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

#include "../Interfaces/ITime.h"
#include "../Interfaces/IOperatingSystem.h"

#ifndef _WIN32
#include <unistd.h>    // for sleep()
#include <time.h>      // for CLOCK_REALTIME
#include <cstring>     // for memset
#endif
#include "../Interfaces/IMemory.h"

Timer::Timer() { Reset(); }

unsigned Timer::GetMSec(bool reset)
{
	unsigned currentTime = getSystemTime();
	unsigned elapsedTime = currentTime - mStartTime;
	if (reset)
		mStartTime = currentTime;

	return elapsedTime;
}

void Timer::Reset() { mStartTime = getSystemTime(); }

HiresTimer::HiresTimer()
{
	memset(mHistory, 0, sizeof(mHistory));
	mHistoryIndex = 0;
	Reset();
}

int64_t HiresTimer::GetUSec(bool reset)
{
	int64_t currentTime = getUSec();
	int64_t elapsedTime = currentTime - mStartTime;

	// Correct for possible weirdness with changing internal frequency
	if (elapsedTime < 0)
		elapsedTime = 0;

	if (reset)
		mStartTime = currentTime;

	mHistory[mHistoryIndex] = elapsedTime;
	mHistoryIndex = (mHistoryIndex + 1) % LENGTH_OF_HISTORY;

	return elapsedTime;
}

int64_t HiresTimer::GetUSecAverage()
{
	int64_t elapsedTime = 0;
	for (uint32_t i = 0; i < LENGTH_OF_HISTORY; ++i)
		elapsedTime += mHistory[i];
	elapsedTime /= LENGTH_OF_HISTORY;

	// Correct for overflow
	if (elapsedTime < 0)
		elapsedTime = 0;

	return elapsedTime;
}

float HiresTimer::GetSeconds(bool reset) { return (float)(GetUSec(reset) / 1e6); }

float HiresTimer::GetSecondsAverage() { return (float)(GetUSecAverage() / 1e6); }

void HiresTimer::Reset() { mStartTime = getUSec(); }
