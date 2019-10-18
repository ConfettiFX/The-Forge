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

#include "AssimpIOSystem.h"
#include "AssimpIOStream.h"

#include "../Interfaces/ILog.h"

// NOTE: we use Assimp's allocator here rather than conf_new and conf_delete
// since the Assimp types need to be allocated from Assimp's heap.
// (see Assimp::Intern::AllocateFromAssimpHeap)

AssimpIOSystem::AssimpIOSystem(const FileSystem* fileSystem): pFileSystem(fileSystem)
{
	pDefaultPath = fsCopyPathForResourceDirectory(RD_ROOT);
}

AssimpIOSystem::~AssimpIOSystem() { fsFreePath(pDefaultPath); }

const Path* AssimpIOSystem::CurrentRootPath() const
{
	if (!mPathStack.empty())
	{
		return mPathStack.back();
	}
	return pDefaultPath;
}

Path* AssimpIOSystem::ToPath(const char* pathStr) const
{
	// First, try to form an absolute path
	if (Path* absolutePath = fsCreatePath(pFileSystem, pathStr))
	{
		return absolutePath;
	}

	// If that fails, form a relative path to the current directory.
	if (const Path* rootPath = CurrentRootPath())
	{
		Path* relativePath = fsAppendPathComponent(rootPath, pathStr);
		return relativePath;
	}

	return NULL;
}

bool AssimpIOSystem::Exists(const char* pFile) const
{
	if (PathHandle path = ToPath(pFile))
	{
		bool exists = fsFileExists(path);
		return exists;
	}
	return false;
}

char AssimpIOSystem::getOsSeparator() const { return pFileSystem->GetPathDirectorySeparator(); }

Assimp::IOStream* AssimpIOSystem::Open(const char* pFile, const char* pMode)
{
	if (PathHandle path = ToPath(pFile))
	{
		FileStream* fh = fsOpenFile(path, fsFileModeFromString(pMode));
		return new AssimpIOStream(fh);
	}
	return NULL;
}

void AssimpIOSystem::Close(Assimp::IOStream* pFile) { delete pFile; }

bool AssimpIOSystem::ComparePaths(const char* pathAStr, const char* pathBStr) const
{
	PathHandle pathA = ToPath(pathAStr);
	if (!pathA)
	{
		return false;
	}

	PathHandle pathB = ToPath(pathBStr);
	if (!pathB)
	{
		return false;
	}

	return fsPathsEqual(pathA, pathB);
}

bool AssimpIOSystem::PushDirectory(const std::string& pathStr)
{
	if (!Assimp::IOSystem::PushDirectory(pathStr))
	{
		return false;
	}

	if (Path* path = ToPath(pathStr.c_str()))
	{
		mPathStack.push_back(path);
		return true;
	}

	Assimp::IOSystem::PopDirectory();
	return false;
}

bool AssimpIOSystem::PopDirectory()
{
	bool success = Assimp::IOSystem::PopDirectory();
	success = success && !mPathStack.empty();
	if (!mPathStack.empty())
	{
		fsFreePath(mPathStack.back());
		mPathStack.pop_back();
	}
	return success;
}

bool AssimpIOSystem::CreateDirectory(const std::string& pathStr)
{
	if (PathHandle path = ToPath(pathStr.c_str()))
	{
		return fsCreateDirectory(path);
	}
	return false;
}

bool AssimpIOSystem::DeleteFile(const std::string& pathStr)
{
	if (PathHandle path = ToPath(pathStr.c_str()))
	{
		return fsDeleteFile(path);
	}
	return false;
}
