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

#include "../../Common_3/OS/Math/MathTypes.h"
#include "../../Common_3/OS/Interfaces/IFileSystem.h"

#include "../../Common_3/ThirdParty/OpenSource/ozz-animation/include/ozz/animation/runtime/animation.h"
#include "../../Common_3/ThirdParty/OpenSource/ozz-animation/include/ozz/animation/runtime/sampling_job.h"
#include "../../Common_3/ThirdParty/OpenSource/ozz-animation/include/ozz/base/memory/allocator.h"
#include "../../Common_3/ThirdParty/OpenSource/ozz-animation/include/ozz/base/io/archive.h"

#include "Rig.h"

//Responsible for loading and storing a clip. Only need one per clip file
//all rigs can sample the same clip object
class Clip
{
	public:
	// Set up a clip associated with a rig and read from an ozz animation file path
	void Initialize(const ResourceDirectory resourceDir, const char* fileName, Rig* rig);

	// Must be called to clean up if the clip was initialized
	void Destroy();

	// Will sample the clip at timeRatio [0,1], using cacheInput as input and saving results to localTransOutput
	bool Sample(ozz::animation::SamplingCache* cacheInput, ozz::Range<SoaTransform>& localTransOutput, float timeRatio);

	// Get the length of the clip
	inline float GetDuration() { return mAnimation.duration(); };

	private:
	// Load a clip from an ozz animation file
	bool LoadClip(const ResourceDirectory resourceDir, const char* fileName);

	// Runtime animation.
	ozz::animation::Animation mAnimation;
};
