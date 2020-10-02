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

#include <functional>

#if !defined(XBOX)
#include "shlobj.h"
#include "commdlg.h"
#include <WinBase.h>
#endif

#include "../Interfaces/ILog.h"
#include "../Interfaces/IOperatingSystem.h"
#include "../Interfaces/IThread.h"
#include "../Interfaces/IMemory.h"

template <typename T>
static inline T withUTF16Path(const char* path, T(*function)(const wchar_t*))
{
	size_t len = strlen(path);
	wchar_t* buffer = (wchar_t*)alloca((len + 1) * sizeof(wchar_t));

	size_t resultLength = MultiByteToWideChar(CP_UTF8, 0, path, (int)len, buffer, (int)len);
	buffer[resultLength] = 0;

	return function(buffer);
}



#ifndef XBOX
static bool gInitialized = false;
static const char* gResourceMounts[RM_COUNT];
const char* getResourceMount(ResourceMount mount) {
	return gResourceMounts[mount];
}

static char gApplicationPath[FS_MAX_PATH] = {};
static char gDocumentsPath[FS_MAX_PATH] = {};

bool initFileSystem(FileSystemInitDesc* pDesc)
{
	if (gInitialized)
	{
		LOGF(LogLevel::eWARNING, "FileSystem already initialized.");
		return true;
	}
	ASSERT(pDesc);
	pSystemFileIO->GetResourceMount = getResourceMount;

	// Get application directory
	wchar_t utf16Path[FS_MAX_PATH];
	GetModuleFileNameW(0, utf16Path, FS_MAX_PATH);
	char applicationFilePath[FS_MAX_PATH] = {};
	WideCharToMultiByte(CP_UTF8, 0, utf16Path, -1, applicationFilePath, MAX_PATH, NULL, NULL);
	fsGetParentPath(applicationFilePath, gApplicationPath);
	gResourceMounts[RM_CONTENT] = gApplicationPath;
	gResourceMounts[RM_DEBUG] = gApplicationPath;

	// Get user directory
	PWSTR userDocuments = NULL;
	SHGetKnownFolderPath(FOLDERID_Documents, 0, NULL, &userDocuments);
	WideCharToMultiByte(CP_UTF8, 0, userDocuments, -1, gDocumentsPath, MAX_PATH, NULL, NULL);
	CoTaskMemFree(userDocuments);
	gResourceMounts[RM_SAVE_0] = gDocumentsPath;

	// Override Resource mounts
	for (uint32_t i = 0; i < RM_COUNT; ++i)
	{
		if (pDesc->pResourceMounts[i])
			gResourceMounts[i] = pDesc->pResourceMounts[i];
	}

	//// Get app data directory
	//char appData[FS_MAX_PATH] = {};
	//PWSTR localAppdata = NULL;
	//SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, NULL, &localAppdata);
	//pathLength = wcslen(localAppdata);
	//utf8Length = WideCharToMultiByte(CP_UTF8, 0, localAppdata, (int)pathLength, NULL, 0, NULL, NULL);
	//WideCharToMultiByte(CP_UTF8, 0, localAppdata, (int)pathLength, appData, utf8Length, NULL, NULL);
	//CoTaskMemFree(localAppdata);

	gInitialized = true;
	return true;
}

void exitFileSystem(void)
{
	gInitialized = false;
}
#endif

static bool fsDirectoryExists(const char* path)
{
	return withUTF16Path<bool>(path, [](const wchar_t* pathStr)
	{
		DWORD attributes = GetFileAttributesW(pathStr);
		return (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY));
	});
}

static bool fsCreateDirectory(const char* path)
{
	if (fsDirectoryExists(path))
	{
		return true;
	}

	char parentPath[FS_MAX_PATH] = { 0 };
	fsGetParentPath(path, parentPath);
	if (parentPath[0] != 0)
	{
		if (!fsDirectoryExists(parentPath))
		{
			fsCreateDirectory(parentPath);
		}
	}
	return withUTF16Path<bool>(path, [](const wchar_t* pathStr)
	{
		return ::CreateDirectoryW(pathStr, NULL) ? true : false;
	});
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

	// Fix paths for Windows 7 - needs to be generalized and propagated
	//eastl::string path = eastl::string(filePath);
	//auto directoryPos = path.find(":");
	//eastl::string cleanPath = path.substr(directoryPos - 1);

	struct stat fileInfo = { 0 };
	stat(filePath, &fileInfo);
	return fileInfo.st_mtime;
}

bool PlatformOpenFile(ResourceDirectory resourceDir, const char* fileName, FileMode mode, FileStream* pOut)
{
	const char* resourcePath = fsGetResourceDirectory(resourceDir);
	char filePath[FS_MAX_PATH] = {};
	fsAppendPathComponent(resourcePath, fileName, filePath);

	// Path utf-16 conversion
	size_t filePathLen = strlen(filePath);
	wchar_t* pathStr = (wchar_t*)alloca((filePathLen + 1) * sizeof(wchar_t));
	size_t pathStrLength =
		MultiByteToWideChar(CP_UTF8, 0, filePath, (int)filePathLen, pathStr, (int)filePathLen);
	pathStr[pathStrLength] = 0;

	// Mode string utf-16 conversion
	const char* modeStr = fsFileModeToString(mode);
	wchar_t modeWStr[4] = {};
	mbstowcs(modeWStr, modeStr, 4);

	FILE* fp = NULL;
	if (mode & FM_ALLOW_READ)
	{
		fp = _wfsopen(pathStr, modeWStr, _SH_DENYWR);
	}
	else
	{
		_wfopen_s(&fp, pathStr, modeWStr);
	}

	if (fp)
	{
		*pOut = {};
		pOut->pFile = fp;
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
	else
	{
		LOGF(LogLevel::eERROR, "Error opening file: %s -- %s (error: %s)", filePath, modeStr, strerror(errno));
	}

	return false;
}
