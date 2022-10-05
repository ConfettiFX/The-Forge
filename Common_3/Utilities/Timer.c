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

#include "Interfaces/ITime.h"

void initTimer(Timer* pTimer) { resetTimer(pTimer); }

unsigned getTimerMSec(Timer* pTimer, bool reset)
{
	unsigned currentTime = getSystemTime();
	unsigned elapsedTime = currentTime - pTimer->mStartTime;
	if (reset)
		pTimer->mStartTime = currentTime;

	return elapsedTime;
}

void resetTimer(Timer* pTimer) { pTimer->mStartTime = getSystemTime(); }

void initHiresTimer(HiresTimer* pTimer)
{
	*pTimer = (HiresTimer){ 0 };
	resetHiresTimer(pTimer);
}

int64_t getHiresTimerUSec(HiresTimer* pTimer, bool reset)
{
	int64_t currentTime = getUSec(false);
	int64_t elapsedTime = currentTime - pTimer->mStartTime;

	// Correct for possible weirdness with changing internal frequency
	if (elapsedTime < 0)
		elapsedTime = 0;

	if (reset)
		pTimer->mStartTime = currentTime;

	pTimer->mHistory[pTimer->mHistoryIndex] = elapsedTime;
	pTimer->mHistoryIndex = (pTimer->mHistoryIndex + 1) % HIRES_TIMER_LENGTH_OF_HISTORY;

	return elapsedTime;
}

int64_t getHiresTimerUSecAverage(HiresTimer* pTimer)
{
	int64_t elapsedTime = 0;
	for (uint32_t i = 0; i < HIRES_TIMER_LENGTH_OF_HISTORY; ++i)
		elapsedTime += pTimer->mHistory[i];
	elapsedTime /= HIRES_TIMER_LENGTH_OF_HISTORY;

	// Correct for overflow
	if (elapsedTime < 0)
		elapsedTime = 0;

	return elapsedTime;
}

float getHiresTimerSeconds(HiresTimer* pTimer, bool reset) { return (float)(getHiresTimerUSec(pTimer, reset) / 1e6); }

float getHiresTimerSecondsAverage(HiresTimer* pTimer) { return (float)(getHiresTimerUSecAverage(pTimer) / 1e6); }

void resetHiresTimer(HiresTimer* pTimer) { pTimer->mStartTime = getUSec(false); }
