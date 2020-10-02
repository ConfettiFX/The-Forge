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

#include "../Interfaces/IFileSystem.h"
#include "../Interfaces/ILog.h"
#include "../Interfaces/IOperatingSystem.h"
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/inotify.h>
#include <dirent.h>
#include <android/asset_manager.h>
#include "../Interfaces/IMemory.h"

bool UnixOpenFile(ResourceDirectory resourceDir, const char* fileName, FileMode mode, FileStream* pOut);

static size_t  AssetStreamRead(FileStream* pFile, void* outputBuffer, size_t bufferSizeInBytes)
{
	return AAsset_read(pFile->pAsset, outputBuffer, bufferSizeInBytes);
}

static size_t AssetStreamWrite(FileStream* pFile, const void* sourceBuffer, size_t byteCount)
{
	LOGF(LogLevel::eERROR, "Bundled Android assets are not writable.");
	return 0;
}

static bool AssetStreamSeek(FileStream* pFile, SeekBaseOffset baseOffset, ssize_t seekOffset)
{
	int origin = SEEK_SET;
	switch (baseOffset)
	{
		case SBO_START_OF_FILE: origin = SEEK_SET; break;
		case SBO_CURRENT_POSITION: origin = SEEK_CUR; break;
		case SBO_END_OF_FILE: origin = SEEK_END; break;
	}
	return AAsset_seek64(pFile->pAsset, seekOffset, origin) != -1;
}

static ssize_t AssetStreamGetSeekPosition(const FileStream* pFile)
{
	return (ssize_t)AAsset_seek64(pFile->pAsset, 0, SEEK_CUR);
}

static ssize_t AssetStreamGetSize(const FileStream* pFile)
{
	return pFile->mSize;
}

static bool AssetStreamFlush(FileStream* pFile) 
{ 
	return true; 
}

static bool AssetStreamIsAtEnd(const FileStream* pFile)
{
	return AAsset_getRemainingLength64(pFile->pAsset) == 0;
}

static bool AssetStreamClose(FileStream* pFile)
{
	AAsset_close(pFile->pAsset);
	return true;
}

static IFileSystem gBundledFileIO =
{
	NULL,
	AssetStreamClose,
	AssetStreamRead,
	AssetStreamWrite,
	AssetStreamSeek,
	AssetStreamGetSeekPosition,
	AssetStreamGetSize,
	AssetStreamFlush,
	AssetStreamIsAtEnd
};

static bool gInitialized = false;
static const char* gResourceMounts[RM_COUNT];
const char* GetResourceMount(ResourceMount mount) {
	return gResourceMounts[mount];
}
bool fsIsBundledResourceDir(ResourceDirectory resourceDir);

static ANativeActivity* pNativeActivity = NULL;
static AAssetManager* pAssetManager = NULL;

bool initFileSystem(FileSystemInitDesc* pDesc)
{
	if (gInitialized)
	{
		LOGF(LogLevel::eWARNING, "FileSystem already initialized.");
		return true;
	}
	ASSERT(pDesc);
	pSystemFileIO->GetResourceMount = GetResourceMount;

	pNativeActivity = (ANativeActivity*)pDesc->pPlatformData;
	ASSERT(pNativeActivity);

	pAssetManager = pNativeActivity->assetManager;
	gResourceMounts[RM_CONTENT] = "\0";
	gResourceMounts[RM_DEBUG] = pNativeActivity->externalDataPath;
	gResourceMounts[RM_SAVE_0] = pNativeActivity->internalDataPath;

	// Override Resource mounts
	for (uint32_t i = 0; i < RM_COUNT; ++i)
	{
		if (pDesc->pResourceMounts[i])
			gResourceMounts[i] = pDesc->pResourceMounts[i];
	}

	gInitialized = true;
	return true;
}

void exitFileSystem()
{
	gInitialized = false;
}

bool PlatformOpenFile(ResourceDirectory resourceDir, const char* fileName, FileMode mode, FileStream* pOut)
{
	const char* resourcePath = fsGetResourceDirectory(resourceDir);
	char filePath[FS_MAX_PATH] = {};
	fsAppendPathComponent(resourcePath, fileName, filePath);
	const char* modeStr = fsFileModeToString(mode);

	if (fsIsBundledResourceDir(resourceDir))
	{
		if ((mode & (FM_WRITE | FM_APPEND)) != 0)
		{
			LOGF(LogLevel::eERROR, "Cannot open %s with mode %i: the Android bundle is read-only.",
				filePath, mode);
			return false;
		}

		AAsset* file = AAssetManager_open(pAssetManager, filePath, AASSET_MODE_BUFFER);
		if (!file)
		{
			LOGF(LogLevel::eERROR, "Failed to open '%s' with mode %i.", filePath, mode);
			return false;
		}

		*pOut = {};
		pOut->pAsset = file;
		pOut->mMode = mode;
		pOut->pIO = &gBundledFileIO;
		pOut->mSize = (ssize_t)AAsset_getLength64(file);
		return true;
	}

	return UnixOpenFile(resourceDir, fileName, mode, pOut);
}
