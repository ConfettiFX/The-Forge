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

#ifndef FileSystemInternal_h
#define FileSystemInternal_h

#include <stdio.h>
#include <string.h>

#include "../Interfaces/IFileSystem.h"

// Include the functions to set the log file directory and executable name
// for the memory manager.
// We need to set these manually as the memory manager doesn't know when it is safe to call
// FileSystem functions.
void mmgr_setExecutableName(const char* name, size_t length);
void mmgr_setLogFileDirectory(const char* directory);

// MARK: - FileSystem

struct FileSystem
{
	public:
	FileSystemKind mKind;

	FileSystem(FileSystemKind kind);

	virtual ~FileSystem() {};

	virtual bool   IsReadOnly() const = 0;
	virtual bool   IsCaseSensitive() const = 0;
	virtual char   GetPathDirectorySeparator() const = 0;
	virtual size_t GetRootPathLength() const = 0;

	/// Fills path's buffer with the canonical root path corresponding to the root of absolutePathString,
	/// and returns an offset into absolutePathString containing the path component after the root by pathComponentOffset.
	/// path is assumed to have storage for up to 16 characters.
	virtual bool FormRootPath(const char* absolutePathString, Path* path, size_t* pathComponentOffset) const = 0;

	virtual time_t GetCreationTime(const Path* filePath) const = 0;
	virtual time_t GetLastAccessedTime(const Path* filePath) const = 0;
	virtual time_t GetLastModifiedTime(const Path* filePath) const = 0;

	virtual bool CreateDirectory(const Path* directoryPath) const = 0;

	virtual FileStream* OpenFile(const Path* filePath, FileMode mode) const = 0;
	virtual bool        CopyFile(const Path* sourcePath, const Path* destinationPath, bool overwriteIfExists) const = 0;
	virtual bool        DeleteFile(const Path* path) const = 0;

	virtual bool FileExists(const Path* path) const = 0;
	virtual bool IsDirectory(const Path* path) const = 0;

	virtual void EnumerateFilesWithExtension(
		const Path* directory, const char* extension, bool (*processFile)(const Path*, void* userData), void* userData) const = 0;
	virtual void
		EnumerateSubDirectories(const Path* directory, bool (*processDirectory)(const Path*, void* userData), void* userData) const = 0;
};

// MARK: - Path

// Paths are always formatted in the native format of their file system.
// They never contain a trailing slash unless they are a root path.
typedef struct Path
{
	FileSystem* pFileSystem;
	size_t      mPathLength;
	char        mPathBufferOffset;
	// ... plus a heap allocated UTF-8 buffer of length pathLength.
} Path;

static inline size_t Path_SizeOf(const Path* path) { return sizeof(Path) + path->mPathLength; }


// MARK: - FileStream

typedef enum FileStreamType
{
	FileStreamType_System,
	FileStreamType_MemoryStream,
	FileStreamType_Zip,
	FileStreamType_BundleAsset
} FileStreamType;

struct FileStream
{
	protected:
	FileStreamType mType;

	public:
	inline FileStream(FileStreamType type): mType(type){};
	virtual ~FileStream() {};

	virtual size_t  Read(void* outputBuffer, size_t bufferSizeInBytes) = 0;
	virtual size_t  Write(const void* sourceBuffer, size_t byteCount) = 0;
    virtual size_t  Scan(const char* format, va_list args, int* bytesRead) = 0;
    virtual size_t  Print(const char* format, va_list args) = 0;
	virtual bool    Seek(SeekBaseOffset baseOffset, ssize_t seekOffset) = 0;
	virtual ssize_t GetSeekPosition() const = 0;
	virtual ssize_t GetFileSize() const = 0;
	virtual void    Flush() = 0;
	virtual bool    IsAtEnd() const = 0;
	virtual bool    Close() = 0;
};

#endif /* FileSystemInternal_h */
