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

#include "IOperatingSystem.h"

/// Low res OS timer
class Timer
{
	public:
	Timer();
	unsigned GetMSec(bool reset);
	void     Reset();

	private:
	unsigned mStartTime;
};

/// High-resolution OS timer
class HiresTimer
{
	friend class TimeManager;

	public:
	HiresTimer();

	int64_t GetUSec(bool reset);
	int64_t GetUSecAverage();
	float   GetSeconds(bool reset);
	float   GetSecondsAverage();
	void    Reset();

	private:
	int64_t mStartTime;

	static const uint32_t LENGTH_OF_HISTORY = 60;
	int64_t               mHistory[LENGTH_OF_HISTORY];
	uint32_t              mHistoryIndex;
};

class TimeManager
{
	public:
	static unsigned GetSystemTime();
	static unsigned GetTimeSinceStart();
};
