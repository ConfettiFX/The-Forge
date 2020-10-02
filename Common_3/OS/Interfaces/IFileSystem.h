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

#pragma once

#include "../Interfaces/IOperatingSystem.h"

#define FS_MAX_PATH 256

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum ResourceMount
{
	/// Installed game directory / bundle resource directory
	RM_CONTENT = 0,
	/// For storing debug data such as log files. To be used only during development
	RM_DEBUG,
	/// Save game data mount 0
	RM_SAVE_0,
	RM_COUNT,
} ResourceMount;

typedef enum ResourceDirectory
{
	/// The main application's shader binaries directory
	RD_SHADER_BINARIES = 0,
	/// The main application's shader source directory
	RD_SHADER_SOURCES,

	RD_PIPELINE_CACHE,
	/// The main application's texture source directory (TODO processed texture folder)
	RD_TEXTURES,
	RD_MESHES,
	RD_FONTS,
	RD_ANIMATIONS,
	RD_AUDIO,
	RD_GPU_CONFIG,
	RD_LOG,
	RD_SCRIPTS,
	RD_OTHER_FILES,

	// Libraries can have their own directories.
	// Up to 100 libraries are supported.
	____rd_lib_counter_begin = RD_OTHER_FILES + 1,

	// Add libraries here
	RD_MIDDLEWARE_0 = ____rd_lib_counter_begin,
	RD_MIDDLEWARE_1,
	RD_MIDDLEWARE_2,
	RD_MIDDLEWARE_3,
	RD_MIDDLEWARE_4,
	RD_MIDDLEWARE_5,
	RD_MIDDLEWARE_6,
	RD_MIDDLEWARE_7,
	RD_MIDDLEWARE_8,
	RD_MIDDLEWARE_9,
	RD_MIDDLEWARE_10,
	RD_MIDDLEWARE_11,
	RD_MIDDLEWARE_12,
	RD_MIDDLEWARE_13,
	RD_MIDDLEWARE_14,
	RD_MIDDLEWARE_15,

	____rd_lib_counter_end = ____rd_lib_counter_begin + 99 * 2,
	RD_COUNT
} ResourceDirectory;

typedef enum SeekBaseOffset
{
	SBO_START_OF_FILE = 0,
	SBO_CURRENT_POSITION,
	SBO_END_OF_FILE,
} SeekBaseOffset;

typedef enum FileMode
{
	FM_READ = 1 << 0,
	FM_WRITE = 1 << 1,
	FM_APPEND = 1 << 2,
	FM_BINARY = 1 << 3,
	FM_ALLOW_READ = 1 << 4, // Read Access to Other Processes, Usefull for Log System
	FM_READ_WRITE = FM_READ | FM_WRITE,
	FM_READ_APPEND = FM_READ | FM_APPEND,
	FM_WRITE_BINARY = FM_WRITE | FM_BINARY,
	FM_READ_BINARY = FM_READ | FM_BINARY,
	FM_APPEND_BINARY = FM_APPEND | FM_BINARY,
	FM_READ_WRITE_BINARY = FM_READ | FM_WRITE | FM_BINARY,
	FM_READ_APPEND_BINARY = FM_READ | FM_APPEND | FM_BINARY,
	FM_WRITE_ALLOW_READ = FM_WRITE | FM_ALLOW_READ,
	FM_APPEND_ALLOW_READ = FM_READ | FM_ALLOW_READ,
	FM_READ_WRITE_ALLOW_READ = FM_READ | FM_WRITE | FM_ALLOW_READ,
	FM_READ_APPEND_ALLOW_READ = FM_READ | FM_APPEND | FM_ALLOW_READ,
	FM_WRITE_BINARY_ALLOW_READ = FM_WRITE | FM_BINARY | FM_ALLOW_READ,
	FM_APPEND_BINARY_ALLOW_READ = FM_APPEND | FM_BINARY | FM_ALLOW_READ,
	FM_READ_WRITE_BINARY_ALLOW_READ = FM_READ | FM_WRITE | FM_BINARY | FM_ALLOW_READ,
	FM_READ_APPEND_BINARY_ALLOW_READ = FM_READ | FM_APPEND | FM_BINARY | FM_ALLOW_READ
} FileMode;

typedef struct IFileSystem IFileSystem;

typedef struct MemoryStream
{
	uint8_t* pBuffer;
	size_t   mCursor;
	bool     mOwner;
} MemoryStream;

typedef struct FileStream
{
	IFileSystem*      pIO;
	union
	{
		FILE*         pFile;
#if defined(__ANDROID__)
		AAsset*       pAsset;
#elif defined(NX64)
		FileNX        mStruct;
#endif
		MemoryStream  mMemory;
		void*         pUser;
	};
	ssize_t           mSize;
	FileMode          mMode;
} FileStream;

typedef struct FileSystemInitDesc
{
	const char* pAppName;
	void*       pPlatformData;
	const char* pResourceMounts[RM_COUNT] = {};
} FileSystemInitDesc;

typedef struct IFileSystem
{
	bool        (*Open)(IFileSystem* pIO, const ResourceDirectory resourceDir, const char* fileName, FileMode mode, FileStream* pOut);
	bool        (*Close)(FileStream* pFile);
	size_t      (*Read)(FileStream* pFile, void* outputBuffer, size_t bufferSizeInBytes);
	size_t      (*Write)(FileStream* pFile, const void* sourceBuffer, size_t byteCount);
	bool        (*Seek)(FileStream* pFile, SeekBaseOffset baseOffset, ssize_t seekOffset);
	ssize_t     (*GetSeekPosition)(const FileStream* pFile);
	ssize_t     (*GetFileSize)(const FileStream* pFile);
	bool        (*Flush)(FileStream* pFile);
	bool        (*IsAtEnd)(const FileStream* pFile);
	const char* (*GetResourceMount)(ResourceMount mount);

	void*       pUser;
} IFileSystem;

/// Default file system using C File IO or Bundled File IO (Android) based on the ResourceDirectory
extern IFileSystem* pSystemFileIO;
/************************************************************************/
// MARK: - Initialization
/************************************************************************/
/// Initializes the FileSystem API
bool initFileSystem(FileSystemInitDesc* pDesc);

/// Frees resources associated with the FileSystem API
void exitFileSystem();
/************************************************************************/
// MARK: - File IO
/************************************************************************/
/// Opens the file at `filePath` using the mode `mode`, returning a new FileStream that can be used
/// to read from or modify the file. May return NULL if the file could not be opened.
bool fsOpenStreamFromPath(const ResourceDirectory resourceDir, const char* fileName, FileMode mode, FileStream* pOut);

/// Opens a memory buffer as a FileStream, returning a stream that must be closed with `fsCloseStream`.
bool fsOpenStreamFromMemory(const void* buffer, size_t bufferSize, FileMode mode, bool owner, FileStream* pOut);

/// Closes and invalidates the file stream.
bool fsCloseStream(FileStream* stream);

/// Returns the number of bytes read.
size_t fsReadFromStream(FileStream* stream, void* outputBuffer, size_t bufferSizeInBytes);

/// Reads at most `bufferSizeInBytes` bytes from sourceBuffer and writes them into the file.
/// Returns the number of bytes written.
size_t fsWriteToStream(FileStream* stream, const void* sourceBuffer, size_t byteCount);

/// Seeks to the specified position in the file, using `baseOffset` as the reference offset.
bool fsSeekStream(FileStream* stream, SeekBaseOffset baseOffset, ssize_t seekOffset);

/// Gets the current seek position in the file.
ssize_t fsGetStreamSeekPosition(const FileStream* stream);

/// Gets the current size of the file. Returns -1 if the size is unknown or unavailable.
ssize_t fsGetStreamFileSize(const FileStream* stream);

/// Flushes all writes to the file stream to the underlying subsystem.
bool fsFlushStream(FileStream* stream);

/// Returns whether the current seek position is at the end of the file stream.
bool fsStreamAtEnd(const FileStream* stream);
/************************************************************************/
// MARK: - Minor filename manipulation
/************************************************************************/
/// Appends `pathComponent` to `basePath`, returning a new Path for which the caller has ownership.
/// `basePath` is assumed to be a directory.
void fsAppendPathComponent(const char* basePath, const char* pathComponent, char* output);

/// Appends `newExtension` to `basePath`, returning a new Path for which the caller has ownership.
/// If `basePath` already has an extension, `newExtension` will be appended to the end.
void fsAppendPathExtension(const char* basePath, const char* newExtension, char* output);

/// Appends `newExtension` to `basePath`, returning a new Path for which the caller has ownership.
/// If `basePath` already has an extension, its previous extension will be replaced by `newExtension`.
void fsReplacePathExtension(const char* path, const char* newExtension, char* output);

/// Copies `path`'s parent path, returning a new Path for which the caller has ownership. May return NULL if `path` has no parent.
void fsGetParentPath(const char* path, char* output);

/// Returns `path`'s file name as a PathComponent. The return value is guaranteed to live for as long as `path` lives.
void fsGetPathFileName(const char* path, char* output);

/// Returns `path`'s extension, excluding the '.'. The return value is guaranteed to live for as long as `path` lives.
/// The returned PathComponent's buffer is guaranteed to either be NULL or a NULL-terminated string.
void fsGetPathExtension(const char* path, char* output);
/************************************************************************/
// MARK: - Directory queries
/************************************************************************/
/// Returns location set for resource directory in fsSetPathForResourceDir
const char* fsGetResourceDirectory(ResourceDirectory resourceDir);

/// Sets the relative path for `resourceDir` on `fileSystem` to `relativePath`, where the base path is the root resource directory path.
/// If `fsSetResourceDirRootPath` is called after this function, this function must be called again to ensure `resourceDir`'s path
/// is relative to the new root path.
///
/// NOTE: This call is not thread-safe. It is the application's responsibility to ensure that
/// no modifications to the file system are occurring at the time of this call.
void fsSetPathForResourceDir(IFileSystem* pIO, ResourceMount mount, ResourceDirectory resourceDir, const char* bundledFolder);
/************************************************************************/
// MARK: - File Queries
/************************************************************************/
/// Gets the time of last modification for the file at `filePath`. Undefined if no file exists at `filePath`.
time_t fsGetLastModifiedTime(ResourceDirectory resourceDir, const char* fileName);
/************************************************************************/
// MARK: - FileMode
/************************************************************************/
static inline FileMode fsFileModeFromString(const char* modeStr)
{
	if (strcmp(modeStr, "r") == 0)
	{
		return FM_READ;
	}
	if (strcmp(modeStr, "w") == 0)
	{
		return FM_WRITE;
	}
	if (strcmp(modeStr, "a") == 0)
	{
		return FM_APPEND;
	}
	if (strcmp(modeStr, "rb") == 0)
	{
		return FM_READ_BINARY;
	}
	if (strcmp(modeStr, "wb") == 0)
	{
		return FM_WRITE_BINARY;
	}
	if (strcmp(modeStr, "ab") == 0)
	{
		return FM_APPEND_BINARY;
	}
	if (strcmp(modeStr, "r+") == 0)
	{
		return FM_READ_WRITE;
	}
	if (strcmp(modeStr, "a+") == 0)
	{
		return FM_READ_APPEND;
	}
	if (strcmp(modeStr, "rb+") == 0)
	{
		return FM_READ_WRITE_BINARY;
	}
	if (strcmp(modeStr, "ab+") == 0)
	{
		return FM_READ_APPEND_BINARY;
	}

	return (FileMode)0;
}

/// Converts `mode` to a string which is compatible with the C standard library conventions for `fopen`
/// parameter strings.
static inline FORGE_CONSTEXPR const char* fsFileModeToString(FileMode mode)
{
	mode = (FileMode)(mode & ~FM_ALLOW_READ);
	switch (mode)
	{
	case FM_READ: return "r";
	case FM_WRITE: return "w";
	case FM_APPEND: return "a";
	case FM_READ_BINARY: return "rb";
	case FM_WRITE_BINARY: return "wb";
	case FM_APPEND_BINARY: return "ab";
	case FM_READ_WRITE: return "r+";
	case FM_READ_APPEND: return "a+";
	case FM_READ_WRITE_BINARY: return "rb+";
	case FM_READ_APPEND_BINARY: return "ab+";
	default: return "r";
	}
}
#ifdef __cplusplus
} // extern "C"
#endif