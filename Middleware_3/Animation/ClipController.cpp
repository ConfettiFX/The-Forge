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

#include "ClipController.h"

void ClipController::Initialize(float duration, float* externallyUsedTime)
{
	mTimeRatio = 0.f;
	mPreviousTimeRatio = 0.f;
	mPlaybackSpeed = 1.f;
	mPlay = true;
	mLoop = true;

	mDuration = duration;
	gExternallyUsedTime = externallyUsedTime;
}

void ClipController::Update(float dt)
{
	float newTime = mTimeRatio;

	if (mPlay)
	{
		newTime = mTimeRatio + dt * (mPlaybackSpeed / mDuration);
	}

	// Must be called even if time doesn't change, in order to update previous
	// frame time ratio. Uses SetTimeRatio function in order to update
	// mPreviousTime an wrap time value in the unit interval (depending on loop
	// mode).
	SetTimeRatio(newTime);
}

void ClipController::SetTimeRatio(float time)
{
	mPreviousTimeRatio = mTimeRatio;
	if (mLoop)
	{
		// Wraps in the unit interval [0:1], even for negative values (the reason
		// for using floorf).
		mTimeRatio = time - floorf(time);    // essentially (time / mDuration) mod 1
	}
	else
	{
		// Clamps in the unit interval [0:1].
		mTimeRatio = clamp(time, 0.f, 1.f);
	}

	if (gExternallyUsedTime)
	{
		*gExternallyUsedTime = mTimeRatio * mDuration;
	}
}

void ClipController::SetTimeRatioHard(float time)
{
	mPlay = false;
	SetTimeRatio(time / mDuration);
}

void ClipController::Reset()
{
	mPreviousTimeRatio = 0.f;
	mTimeRatio = 0.f;
	mPlaybackSpeed = 1.f;
	mPlay = true;
}