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

#include "../Interfaces/IFileSystem.h"
#include "../Interfaces/ILog.h"

static bool fsDirectoryExists(const char* path)
{
	struct stat s;
	if (stat(path, &s))
	{
		return false;
	}

	return (s.st_mode & S_IFDIR) != 0;
}

static bool fsCreateDirectory(const char* path)
{
	if (fsDirectoryExists(path)) // Check if directory already exists
	{
		return true;
	}

	char parentPath[FS_MAX_PATH] = { 0 };
	fsGetParentPath(path, parentPath);

	// Recursively create all parent directories.
	if (parentPath[0] != 0)
	{
		fsCreateDirectory(parentPath);
	}

	if (mkdir(path, 0777) != 0)
	{
		LOGF(LogLevel::eINFO, "Unable to create directory at %s: %s", path, strerror(errno));
		return false;
	}

	return true;
}

bool fsCreateDirectory(ResourceDirectory resourceDir)
{
	return fsCreateDirectory(fsGetResourceDirectory(resourceDir));
}

time_t fsGetLastModifiedTime(ResourceDirectory resourceDir, const char* fileName)
{
	const char* resourcePath = fsGetResourceDirectory(resourceDir);
	char filePath[FS_MAX_PATH] = { 0 };
	fsAppendPathComponent(resourcePath, fileName, filePath);

	struct stat fileInfo = {};
	stat(filePath, &fileInfo);
	return fileInfo.st_mtime;
}

bool UnixOpenFile(ResourceDirectory resourceDir, const char* fileName, FileMode mode, FileStream* pOut)
{
	const char* resourcePath = fsGetResourceDirectory(resourceDir);
	char filePath[FS_MAX_PATH] = {};
	fsAppendPathComponent(resourcePath, fileName, filePath);
	const char* modeStr = fsFileModeToString(mode);

	FILE* file = fopen(filePath, modeStr);
	if (!file)
	{
		LOGF(LogLevel::eERROR, "Error opening file: %s -- %s (error: %s)", filePath, modeStr, strerror(errno));
		return false;
	}

	*pOut = {};
	pOut->pFile = file;
	pOut->mMode = mode;
	pOut->pIO = pSystemFileIO;

	pOut->mSize = -1;
	if (fseek(pOut->pFile, 0, SEEK_END) == 0)
	{
		pOut->mSize = ftell(pOut->pFile);
		rewind(pOut->pFile);
	}

	return true;
}

#if !defined(__ANDROID__)
bool PlatformOpenFile(ResourceDirectory resourceDir, const char* fileName, FileMode mode, FileStream* pOut)
{
	return UnixOpenFile(resourceDir, fileName, mode, pOut);
}
#endif
