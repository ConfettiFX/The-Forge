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

#include "../../ThirdParty/OpenSource/libzip/zip.h"

#include "ZipFileSystem.h"
#include "ZipFileStream.h"

#include "../Interfaces/ILog.h"
#include "../Interfaces/IMemory.h"

ZipFileSystem* ZipFileSystem::CreateWithRootAtPath(const Path* rootPath, FileSystemFlags flags)
{
	zip_flags_t zipFlags = 0;

	if (flags & FSF_CREATE_IF_NECESSARY)
	{
		zipFlags |= ZIP_CREATE;
	}
	if (flags & FSF_OVERWRITE)
	{
		zipFlags |= ZIP_TRUNCATE;
	}
	if (flags & FSF_READ_ONLY)
	{
		zipFlags |= ZIP_RDONLY;
	}
	int    error;
	zip_t* zipFile = zip_open(fsGetPathAsNativeString(rootPath), zipFlags, &error);

	if (!zipFile)
	{
		LOGF(LogLevel::eERROR, "Error %i creating file system from zip file at %s", error, fsGetPathAsNativeString(rootPath));
		return NULL;
	}

	time_t creationTime = fsGetCreationTime(rootPath);
	time_t accessedTime = fsGetLastAccessedTime(rootPath);
	return conf_new(ZipFileSystem, zipFile, flags, creationTime, accessedTime);
}

ZipFileSystem::ZipFileSystem(zip_t* zipFile, FileSystemFlags flags, time_t creationTime, time_t lastAccessedTime):
	FileSystem(FSK_ZIP),
	pZipFile(zipFile),
	mFlags(flags),
	mCreationTime(creationTime),
	mLastAccessedTime(lastAccessedTime)
{
}

ZipFileSystem::~ZipFileSystem()
{
	int result = zip_close(pZipFile);
	if (result != 0)
	{
		zip_error_t* error = zip_get_error(pZipFile);
		LOGF(LogLevel::eERROR, "Error %i closing zip file: %s", error->zip_err, error->str);
	}
}

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
        
        zip_error_t error = {};
        zip_source_t* source = zip_source_buffer_create(NULL, 0, 0, &error);
        if (error.zip_err)
        {
            LOGF(
            LogLevel::eERROR, "Error %i creating zip source at %s: %s", error.zip_err, fsGetPathAsNativeString(filePath), error.str);
            return NULL;
        }
        
        if (!source) { return NULL; }
        
        zip_source_keep(source);
        
        if (zip_file_add(pZipFile, fsGetPathAsNativeString(filePath), source, ZIP_FL_ENC_UTF_8 | ZIP_FL_OVERWRITE) == -1)
        {
            zip_error_t* error = zip_get_error(pZipFile);
            LOGF(
            LogLevel::eERROR, "Error %i adding file to zip at %s: %s", error->zip_err, fsGetPathAsNativeString(filePath), error->str);
            zip_source_free(source);
            return NULL;
        }
        
        zip_source_begin_write(source);
        return conf_new(ZipSourceStream, source, mode);
    }
    
	zip_int64_t index = zip_name_locate(pZipFile, fsGetPathAsNativeString(filePath), ZIP_FL_ENC_STRICT);
	if (index == -1)
	{
		zip_error_t* error = zip_get_error(pZipFile);
		LOGF(
			LogLevel::eINFO, "Error %i finding file %s for opening in zip: %s", error->zip_err, fsGetPathAsNativeString(filePath),
			error->str);
		return NULL;
	}
    
	zip_stat_t stat = {};
	if (zip_stat_index(pZipFile, index, 0, &stat) != 0)
	{
		zip_error_t* error = zip_get_error(pZipFile);
		LOGF(
			LogLevel::eERROR, "Error %i getting uncompressed size for %s: %s", error->zip_err, fsGetPathAsNativeString(filePath), error->str);
		stat.size = 0;
	}

	zip_file_t* file = zip_fopen_index(pZipFile, index, ZIP_FL_ENC_STRICT);
	if (!file) { return NULL; }

    
    if (mode & FM_APPEND)
    {
        zip_source_t* source = zip_source_zip(pZipFile, pZipFile, index, (zip_flags_t)0, 0, -1);
        zip_source_begin_write_cloning(source, stat.size);
        return conf_new(ZipSourceStream, source, mode);
    }
    
	return conf_new(ZipFileStream, file, mode, (size_t)stat.size);
}

time_t ZipFileSystem::GetCreationTime(const Path* filePath) const { return mCreationTime; }

time_t ZipFileSystem::GetLastAccessedTime(const Path* filePath) const { return mLastAccessedTime; }

time_t ZipFileSystem::GetLastModifiedTime(const Path* filePath) const
{
	zip_stat_t stat;
	if (zip_stat(pZipFile, fsGetPathAsNativeString(filePath), 0, &stat) != 0)
	{
		zip_error_t* error = zip_get_error(pZipFile);
		LOGF(LogLevel::eERROR, "Error %i getting modified time for %s: %s", error->zip_err, fsGetPathAsNativeString(filePath), error->str);
		return 0;
	}

	return stat.mtime;
}

bool ZipFileSystem::CreateDirectory(const Path* directoryPath) const
{
	zip_int64_t result = zip_dir_add(pZipFile, fsGetPathAsNativeString(directoryPath), ZIP_FL_ENC_UTF_8);
	if (result != 0)
	{
		zip_error_t* error = zip_get_error(pZipFile);
		LOGF(
			LogLevel::eINFO, "Error %i creating directory %s in zip: %s", error->zip_err, fsGetPathAsNativeString(directoryPath), error->str);
		return false;
	}
	return true;
}

bool ZipFileSystem::FileExists(const Path* path) const
{
	return zip_name_locate(pZipFile, fsGetPathAsNativeString(path), ZIP_FL_ENC_STRICT) != -1;
}

bool ZipFileSystem::IsDirectory(const Path* path) const
{
	zip_stat_t stat;
	if (zip_stat(pZipFile, fsGetPathAsNativeString(path), 0, &stat) == 0)
	{
		return stat.name[strlen(stat.name) - 1] == '/';
	}
	return false;
}

bool ZipFileSystem::DeleteFile(const Path* path) const
{
	zip_int64_t index = zip_name_locate(pZipFile, fsGetPathAsNativeString(path), ZIP_FL_ENC_STRICT);
	if (index == -1)
	{
		zip_error_t* error = zip_get_error(pZipFile);
		LOGF(LogLevel::eINFO, "Error %i finding file %s for deletion in zip: %s", error->zip_err, fsGetPathAsNativeString(path), error->str);
		return false;
	}

	zip_int64_t result = zip_delete(pZipFile, index);
	if (result != 0)
	{
		zip_error_t* error = zip_get_error(pZipFile);
		LOGF(LogLevel::eINFO, "Error %i deleting file %s in zip: %s", error->zip_err, fsGetPathAsNativeString(path), error->str);
		return false;
	}

	return true;
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
