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

#include "Clip.h"

void Clip::Initialize(const char* animationFile, Rig* rig) { LoadClip(animationFile); }

void Clip::Destroy() { mAnimation.Deallocate(); }

bool Clip::Sample(ozz::animation::SamplingCache* cacheInput, ozz::Range<SoaTransform>& localTransOutput, float timeRatio)
{
	// Setup sampling job.
	ozz::animation::SamplingJob samplingJob;
	samplingJob.animation = &mAnimation;
	samplingJob.cache = cacheInput;
	samplingJob.ratio = timeRatio;
	samplingJob.output = localTransOutput;

	// Samples animation.
	if (!samplingJob.Run())
		return false;

	return true;
}

bool Clip::LoadClip(const char* fileName)
{
	ozz::io::File file(fileName, "rb");
	if (!file.opened())
	{
		ErrorMsg("Cannot open file ");
		return false;
	}

	ozz::io::IArchive archive(&file);
	if (!archive.TestTag<ozz::animation::Animation>())
	{
		ErrorMsg("Archive doesn't contain the expected object type.");
		return false;
	}

	archive >> mAnimation;
	return true;
}