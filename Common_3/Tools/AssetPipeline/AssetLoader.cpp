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

#include "AssetLoader.h"

// OZZ
#include "../../ThirdParty/OpenSource/ozz-animation/include/ozz/base/io/stream.h"
#include "../../ThirdParty/OpenSource/ozz-animation/include/ozz/base/io/archive.h"

#include "../../OS/Interfaces/IOperatingSystem.h"
#include "../../OS/Interfaces/ILogManager.h"
#include "../../OS/Interfaces/IMemoryManager.h"    //NOTE: this should be the last include in a .cpp

bool AssetLoader::LoadSkeleton(const char* skeletonFile, FSRoot root, ozz::animation::Skeleton* skeleton)
{
	// Fix path
	tinystl::string path = FileSystem::FixPath(skeletonFile, root);

	// Load skeleton from disk
	ozz::io::File file(path.c_str(), "rb");
	if (!file.opened())
		return false;
	ozz::io::IArchive archive(&file);
	archive >> *skeleton;
	file.Close();

	return true;
}

bool AssetLoader::LoadAnimation(const char* animationFile, FSRoot root, ozz::animation::Animation* animation)
{
	// Fix path
	tinystl::string path = FileSystem::FixPath(animationFile, root);

	// Load animation from disk
	ozz::io::File file(path.c_str(), "rb");
	if (!file.opened())
		return false;
	ozz::io::IArchive archive(&file);
	archive >> *animation;
	file.Close();

	return true;
}