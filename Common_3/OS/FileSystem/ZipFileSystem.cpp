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

#include "../../ThirdParty/OpenSource/zip/zip.h"

#include "ZipFileSystem.h"
#include "ZipFileStream.h"

#include "../Interfaces/ILog.h"
#include "../Interfaces/IMemory.h"

ZipFileSystem* ZipFileSystem::CreateWithRootAtPath(const Path* rootPath, FileSystemFlags flags)
{
	char mode = 0;

	if (flags & FSF_OVERWRITE)
	{
		mode = 'w';
	}
	else if (flags & FSF_READ_ONLY)
	{
		mode = 'r';
	}
	else
	{
		mode = 'a';
	}

	zip_t* zipFile = zip_open(fsGetPathAsNativeString(rootPath), ZIP_DEFAULT_COMPRESSION_LEVEL, mode);

	if (!zipFile)
	{
		LOGF(LogLevel::eERROR, "Error creating file system from zip file at %s", fsGetPathAsNativeString(rootPath));
		return NULL;
	}

	time_t creationTime = fsGetCreationTime(rootPath);
	time_t accessedTime = fsGetLastAccessedTime(rootPath);
	ZipFileSystem* system = conf_new(ZipFileSystem, rootPath, zipFile, flags, creationTime, accessedTime);
	uint32_t totalEntries = zip_total_entries(zipFile);
	for (uint32_t i = 0; i < totalEntries; ++i)
	{
		zip_entry_openbyindex(zipFile, i);
		uint32_t dir = zip_entry_isdir(zipFile);
		ssize_t size = zip_entry_size(zipFile);
		time_t time = zip_entry_time(zipFile);
		system->mEntries[zip_entry_name(zipFile)] = { time, size, i, dir };
		zip_entry_close(zipFile);
	}

	return system;
}

ZipFileSystem::ZipFileSystem(const Path* pathInParent, zip_t* zipFile, FileSystemFlags flags, time_t creationTime, time_t lastAccessedTime):
	FileSystem(FSK_ZIP),
    pPathInParent(fsCopyPath(pathInParent)),
	pZipFile(zipFile),
	mFlags(flags),
	mCreationTime(creationTime),
	mLastAccessedTime(lastAccessedTime)
{
}

ZipFileSystem::~ZipFileSystem()
{
	zip_close(pZipFile);
	fsFreePath(pPathInParent);
}


Path* ZipFileSystem::CopyPathInParent() const { return fsCopyPath(pPathInParent); }

bool ZipFileSystem::IsReadOnly() const {
	return mFlags & FSF_READ_ONLY;
}

bool ZipFileSystem::IsCaseSensitive() const {
	return true;
}

char ZipFileSystem::GetPathDirectorySeparator() const { return '/'; }

size_t ZipFileSystem::GetRootPathLength() const { return 0; }

bool ZipFileSystem::FormRootPath(const char* absolutePathString, Path* path, size_t* pathComponentOffset) const
{
	(&path->mPathBufferOffset)[0] = 0;
	path->mPathLength = 0;

	if (absolutePathString[0] == GetPathDirectorySeparator())
	{
		*pathComponentOffset = 1;
	}
	else
	{
		*pathComponentOffset = 0;
	}

	return true;
}

FileStream* ZipFileSystem::OpenFile(const Path* filePath, FileMode mode) const
{
	if (mode & FM_WRITE)
	{
		if ((mode & FM_READ) != 0)
		{
			LOGF(LogLevel::eERROR, "Zip file %s cannot be open for both reading and writing", fsGetPathAsNativeString(filePath));
			return NULL;
		}

		int error = zip_entry_open(pZipFile, fsGetPathAsNativeString(filePath));
		if (error)
		{
			LOGF(LogLevel::eINFO, "Error %i finding file %s for opening in zip: %s", error, fsGetPathAsNativeString(filePath),
				fsGetPathAsNativeString(pPathInParent));
			return NULL;
		}

		return conf_new(ZipFileStream, pZipFile, mode, filePath);
	}

	decltype(mEntries)::const_iterator it = mEntries.find(fsGetPathAsNativeString(filePath));
	if (it == mEntries.end())
	{
		LOGF(LogLevel::eINFO, "Error finding file %s for opening in zip: %s", fsGetPathAsNativeString(filePath),
			fsGetPathAsNativeString(pPathInParent));
		return NULL;
	}

	if (!it->second.mSize || UINT_MAX == it->second.mSize)
	{
		LOGF(LogLevel::eERROR, "Error getting uncompressed size for %s: %s", fsGetPathAsNativeString(filePath),
			fsGetPathAsNativeString(pPathInParent));
	}

	int error = zip_entry_open(pZipFile, fsGetPathAsNativeString(filePath));
	if (error)
	{
		LOGF(LogLevel::eINFO, "Error %i finding file %s for opening in zip: %s", error, fsGetPathAsNativeString(filePath),
			fsGetPathAsNativeString(pPathInParent));
		return NULL;
	}

	return conf_new(ZipFileStream, pZipFile, mode, filePath);
}

time_t ZipFileSystem::GetCreationTime(const Path* filePath) const { return mCreationTime; }

time_t ZipFileSystem::GetLastAccessedTime(const Path* filePath) const { return mLastAccessedTime; }

time_t ZipFileSystem::GetLastModifiedTime(const Path* path) const
{
	decltype(mEntries)::const_iterator it = mEntries.find(fsGetPathAsNativeString(path));
	if (it != mEntries.end())
		return it->second.mTime;
	return 0;
}

bool ZipFileSystem::CreateDirectory(const Path* directoryPath) const
{
	LOGF(LogLevel::eWARNING, "ZipFileSystem::CopyFile is unimplemented.");
	return false;
}

bool ZipFileSystem::FileExists(const Path* path) const
{
	decltype(mEntries)::const_iterator it = mEntries.find(fsGetPathAsNativeString(path));
	return it != mEntries.end();
}

bool ZipFileSystem::IsDirectory(const Path* path) const
{
	decltype(mEntries)::const_iterator it = mEntries.find(fsGetPathAsNativeString(path));
	if (it != mEntries.end())
		return it->second.mDirectory == 1;
	return false;
}

bool ZipFileSystem::DeleteFile(const Path* path) const
{
	LOGF(LogLevel::eWARNING, "ZipFileSystem::CopyFile is unimplemented.");
	return false;
}

bool ZipFileSystem::CopyFile(const Path* sourcePath, const Path* destinationPath, bool overwriteIfExists) const
{
	LOGF(LogLevel::eWARNING, "ZipFileSystem::CopyFile is unimplemented.");
	return false;
}

void ZipFileSystem::EnumerateFilesWithExtension(
	const Path* directory, const char* extension, bool (*processFile)(const Path*, void* userData), void* userData) const
{
	LOGF(LogLevel::eERROR, "ZipFileSystem::EnumerateFilesWithExtension is unimplemented.");
}

void ZipFileSystem::EnumerateSubDirectories(
	const Path* directory, bool (*processDirectory)(const Path*, void* userData), void* userData) const
{
	LOGF(LogLevel::eERROR, "ZipFileSystem::EnumerateSubDirectories is unimplemented.");
}
