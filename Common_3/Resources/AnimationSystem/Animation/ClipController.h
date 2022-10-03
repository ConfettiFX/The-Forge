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
#include "../ThirdParty/OpenSource/ozz-animation/include/ozz/animation/runtime/animation.h"
#include "../../../Utilities/Math/MathTypes.h"

// Utility class that helps with controlling animation playback time. Time is
// computed every update according to the dt given by the caller, playback speed
// and "play" state.
// Internally time is stored as a ratio in unit interval [0,1], as expected by
// ozz runtime animation jobs.
class FORGE_API ClipController
{
	public:
	// Initialization
	void Initialize(float duration, float* externallyUsedTime = NULL);

	// Sets animation current time.
	void SetTimeRatio(float time);

	// Hard sets the time ratio and pauses the animation
	void SetTimeRatioHard(float time);

	// Gets animation time ratio of last update. Useful when the range between
	// previous and current frame needs to pe processed.
	inline float GetPreviousTimeRatio() const { return mPreviousTimeRatio; };

	// Updates animation time if in "play" state, according to playback speed and
	// given frame time dt.
	// Returns true if animation has looped during update
	void Update(float dt);

	// Resets all parameters to their default value.
	void Reset();

	// Gets updated each time we set the time, will get used externally (UI)
	float* gExternallyUsedTime = NULL;

	// Blending weight [0-1] for the clip this is managing.
	// Initialize to max of 1.0f so clip will have full influence
	float mWeight = 1.0f;

	// Current animation time ratio, in the unit interval [0,1], where 0 is the
	// beginning of the animation, 1 is the end.
	float mTimeRatio = 0.0f;

	// Time ratio of the previous update.
	float mPreviousTimeRatio = 0.0f;

	// Length of the clip this is managing
	float mDuration = 0.0f;

	// Playback speed, can be negative in order to play the animation backward.
	float mPlaybackSpeed = 0.0f;

	// Indicates if the clip is additive or not.
	bool mAdditive = false;

	// Animation play mode state: play/pause.
	bool mPlay = false;

	// Animation loop mode.
	bool mLoop = false;
};