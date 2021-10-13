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

#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "../Core/Config.h"

#ifdef __cplusplus
extern "C" {
#endif
// High res timer functions
int64_t getUSec(void);
int64_t getTimerFrequency(void);

// Time related functions
uint32_t getSystemTime(void);
uint32_t getTimeSinceStart(void);


/// Low res OS timer
typedef struct Timer
{
	uint32_t mStartTime;
}Timer;

void		initTimer(Timer* pTimer);
void		resetTimer(Timer* pTimer);
uint32_t	getTimerMSec(Timer* pTimer, bool reset);


/// High-resolution OS timer
#define HIRES_TIMER_LENGTH_OF_HISTORY 60

typedef struct HiresTimer
{
	int64_t		mStartTime;

	int64_t		mHistory[HIRES_TIMER_LENGTH_OF_HISTORY];
	uint32_t	mHistoryIndex;
}HiresTimer;

void		initHiresTimer(HiresTimer* pTimer);
int64_t		getHiresTimerUSec(HiresTimer* pTimer, bool reset);
int64_t		getHiresTimerUSecAverage(HiresTimer* pTimer);
float		getHiresTimerSeconds(HiresTimer* pTimer, bool reset);
float		getHiresTimerSecondsAverage(HiresTimer* pTimer);
void		resetHiresTimer(HiresTimer* pTimer);


#ifdef __cplusplus
}
#endif
