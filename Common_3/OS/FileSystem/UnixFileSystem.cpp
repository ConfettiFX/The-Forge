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

#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

#include "UnixFileSystem.h"
#include "SystemFileStream.h"

#include "../Interfaces/ILog.h"
#include "../Interfaces/IMemory.h"

bool UnixFileSystem::IsReadOnly() const { return false; }

char UnixFileSystem::GetPathDirectorySeparator() const { return '/'; }

size_t UnixFileSystem::GetRootPathLength() const
{
	return 1;    // A single /
}

bool UnixFileSystem::FormRootPath(const char* absolutePathString, Path* path, size_t* pathComponentOffset) const
{
	if (absolutePathString[0] != GetPathDirectorySeparator())
	{
		// Unix root paths must start with a /
		return false;
	}

	(&path->mPathBufferOffset)[0] = '/';
	path->mPathLength = 1;
	*pathComponentOffset = 1;

	return true;
}

FileStream* UnixFileSystem::OpenFile(const Path* filePath, FileMode mode) const
{
	FILE* file = fopen(fsGetPathAsNativeString(filePath), fsFileModeToString(mode));
	if (!file)
	{
		return NULL;
	}

	return conf_new(SystemFileStream, file, mode, filePath);
}

time_t UnixFileSystem::GetCreationTime(const Path* filePath) const
{
	struct stat fileInfo = {};

	stat(fsGetPathAsNativeString(filePath), &fileInfo);
	return fileInfo.st_ctime;
}

time_t UnixFileSystem::GetLastAccessedTime(const Path* filePath) const
{
	struct stat fileInfo = {};

	stat(fsGetPathAsNativeString(filePath), &fileInfo);
	return fileInfo.st_atime;
}

time_t UnixFileSystem::GetLastModifiedTime(const Path* filePath) const
{
	struct stat fileInfo = {};

	stat(fsGetPathAsNativeString(filePath), &fileInfo);
	return fileInfo.st_mtime;
}

bool UnixFileSystem::CreateDirectory(const Path* directoryPath) const
{
	if (IsDirectory(directoryPath))
	{
		return false;
	}

	// Recursively create all parent directories.
	if (Path* parentPath = fsCopyParentPath(directoryPath))
	{
		CreateDirectory(parentPath);
        fsFreePath(parentPath);
	}

	if (mkdir(fsGetPathAsNativeString(directoryPath), 0777) != 0)
	{
		LOGF(LogLevel::eINFO, "Unable to create directory at %s: %s", fsGetPathAsNativeString(directoryPath), strerror(errno));
		return false;
	}

	return true;
}

bool UnixFileSystem::FileExists(const Path* path) const
{
#if defined(ORBIS)
	struct stat orbis_file_stats;
	int32_t ret = stat(fsGetPathAsNativeString(path), &orbis_file_stats);
	return ret == 0;
#else
	return access(fsGetPathAsNativeString(path), F_OK) != -1;
#endif
}

bool UnixFileSystem::IsDirectory(const Path* path) const
{
	struct stat s;
	if (stat(fsGetPathAsNativeString(path), &s))
		return false;
	return (s.st_mode & S_IFDIR) != 0;
}

bool UnixFileSystem::DeleteFile(const Path* path) const
{
	if (remove(fsGetPathAsNativeString(path)) != 0)
	{
		LOGF(LogLevel::eINFO, "Unable to delete file at %s: %s", fsGetPathAsNativeString(path), strerror(errno));
		return false;
	}

	return true;
}
