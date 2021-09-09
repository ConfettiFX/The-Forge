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

#include "Clip.h"

void Clip::Initialize(const ResourceDirectory resourceDir, const char* fileName, const char* password, Rig* rig)
{
	LoadClip(resourceDir, fileName, password);
}

void Clip::Exit()
{
	mAnimation.Deallocate();
}

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

bool Clip::LoadClip(const ResourceDirectory resourceDir, const char* fileName, const char* password)
{
	FileStream file = {};
	if (!fsOpenStreamFromPath(resourceDir, fileName, FM_READ_BINARY, password, &file))
	{
		LOGF(eERROR, "Cannot open skeleton file");
		return false;
	}

	ssize_t size = fsGetStreamFileSize(&file);
	void* data = tf_malloc(size);
	fsReadFromStream(&file, data, (size_t)size);
	fsCloseStream(&file);

	// Archive is doing a lot of freads from disk which is slow on some platforms and also generally not good
	// So we just read the entire file once into a mem stream so the freads from IArchive are actually
	// only reading from system memory instead of disk or network
	FileStream memStream = {};
	fsOpenStreamFromMemory(data, size, FM_READ, true, &memStream);

	ozz::io::IArchive archive(&memStream);
	if (!archive.TestTag<ozz::animation::Animation>())
	{
		LOGF(eERROR, "Archive doesn't contain the expected object type.");
		return false;
	}

	archive >> mAnimation;

	fsCloseStream(&memStream);

	return true;
}
