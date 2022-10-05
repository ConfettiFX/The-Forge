/*
 * Copyright (c) 2017-2022 The Forge Interactive Inc.
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

#include "../../Application/Config.h"
#include "../../OS/Interfaces/IOperatingSystem.h"

// IOS Simulator paths can get a bit longer then 256 bytes
#ifdef TARGET_IOS_SIMULATOR
#define FS_MAX_PATH 320
#else
#define FS_MAX_PATH 512
#endif

struct bstring;

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
	/// Documents directory
	RM_DOCUMENTS,
#if defined(ANDROID)
	// System level files (/proc/ or equivalent if available)
	RM_SYSTEM,
#endif
	/// Save game data mount 0
	RM_SAVE_0,
#ifdef ENABLE_FS_EMPTY_MOUNT
	/// Empty mount for absolute paths
	RM_EMPTY,
#endif
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
	RD_SCREENSHOTS,
#if defined(ANDROID)
	// #TODO: Add for others if necessary
	RD_SYSTEM,
#endif
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
	size_t   mCapacity;
	bool     mOwner;
} MemoryStream;

typedef struct FileStream
{
	IFileSystem*        pIO;
	struct FileStream*  pBase; // for chaining streams
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
	ResourceMount     mMount;
} FileStream;

typedef struct FileSystemInitDesc
{
	const char* pAppName;
	void*       pPlatformData;
	const char* pResourceMounts[RM_COUNT];
} FileSystemInitDesc;

struct IFileSystem
{
	bool        (*Open)(IFileSystem* pIO, const ResourceDirectory resourceDir, const char* fileName, 
	             FileMode mode, const char* password, FileStream* pOut);
	bool        (*Close)(FileStream* pFile);
	size_t      (*Read)(FileStream* pFile, void* outputBuffer, size_t bufferSizeInBytes);
	size_t      (*Write)(FileStream* pFile, const void* sourceBuffer, size_t byteCount);
	bool        (*Seek)(FileStream* pFile, SeekBaseOffset baseOffset, ssize_t seekOffset);
	ssize_t     (*GetSeekPosition)(const FileStream* pFile);
	ssize_t     (*GetFileSize)(const FileStream* pFile);
	bool        (*Flush)(FileStream* pFile);
	bool        (*IsAtEnd)(const FileStream* pFile);
	const char* (*GetResourceMount)(ResourceMount mount);

	bool        (*GetPropInt64)(FileStream* pFile, int32_t prop, int64_t *pValue);
	bool        (*SetPropInt64)(FileStream* pFile, int32_t prop, int64_t value);


	void*       pUser;
};

/// Default file system using C File IO or Bundled File IO (Android) based on the ResourceDirectory
FORGE_API extern IFileSystem* pSystemFileIO;
/************************************************************************/
// MARK: - Initialization
/************************************************************************/
/// Initializes the FileSystem API
FORGE_API bool initFileSystem(FileSystemInitDesc* pDesc);

/// Frees resources associated with the FileSystem API
FORGE_API void exitFileSystem(void);

/************************************************************************/
// MARK: - Zip file system IO
/************************************************************************/
/// Opens zip file and initializes IFileSystem for it.
/// The actual file handle is open only when zip file entry is open.
/// Internally it keeps track of entries opened and closes when the counter reaches 0.
/// The counter can be manually incremented/decremented by calling fsOpenZipFile or fsCloseZipFile.
/// Specified password is for the zip file itself, not for its content.
FORGE_API bool initZipFileSystem(const ResourceDirectory resourceDir, const char* fileName, FileMode mode, const char* password, IFileSystem* pOut);

/// Frees resources associated with the zip file
FORGE_API bool exitZipFileSystem(IFileSystem* pZip);

/// Fetches number of entries in zip file
FORGE_API bool fsEntryCountZipFile(IFileSystem* pIO, uint64_t* pOut);
/// Opens zip entry by it's index in the zip file
FORGE_API bool fsOpenZipEntryByIndex(IFileSystem* pIO, uint64_t index, FileMode mode, const char* filePassword, FileStream* pOut);

/// Reopens file handle if open entry counter was 0 and increments the counter
FORGE_API bool fsOpenZipFile(IFileSystem* pIO);
/// Decrements open entry counter and closes file handle if it reaches 0
FORGE_API bool fsCloseZipFile(IFileSystem* pIO);

/// Fetches zip file index from it's filename
FORGE_API bool fsFetchZipEntryIndex(IFileSystem* pIO, ResourceDirectory resourceDir, const char* pFileName, uint64_t* pOut);
/// Fills pSize with the size of the filename of the entry with the given index
/// If pBuffer is not NULL fills up to bufferSize - 1 bytes with filename
/// the last byte of pBuffer is filled with null terminator
FORGE_API bool fsFetchZipEntryName(IFileSystem* pIO, uint64_t index, char* pBuffer, size_t* pSize, size_t bufferSize);


/************************************************************************/
// MARK: - File IO
/************************************************************************/
/// Opens the file at `filePath` using the mode `mode`, returning a new FileStream that can be used
/// to read from or modify the file. May return NULL if the file could not be opened.
FORGE_API bool fsOpenStreamFromPath(const ResourceDirectory resourceDir, const char* fileName,
	                      FileMode mode, const char* password, FileStream* pOut);

/// Opens a memory buffer as a FileStream, returning a stream that must be closed with `fsCloseStream`.
FORGE_API bool fsOpenStreamFromMemory(const void* buffer, size_t bufferSize, FileMode mode, bool owner, FileStream* pOut);

/// Checks if stream is a standard system stream
FORGE_API bool fsIsSystemFileStream(FileStream* pStream);
/// Checks if stream is a memory stream
FORGE_API bool fsIsMemoryStream(FileStream* pStream);

/// Closes and invalidates the file stream.
FORGE_API bool fsCloseStream(FileStream* stream);

/// Returns the number of bytes read.
FORGE_API size_t fsReadFromStream(FileStream* stream, void* outputBuffer, size_t bufferSizeInBytes);
/// symbolsCount can be SIZE_MAX, then reads until the end of file
/// appends '\0' to the end of string
FORGE_API size_t fsReadBstringFromStream(FileStream* stream, struct bstring* pStr, size_t symbolsCount);

/// Reads at most `bufferSizeInBytes` bytes from sourceBuffer and writes them into the file.
/// Returns the number of bytes written.
FORGE_API size_t fsWriteToStream(FileStream* stream, const void* sourceBuffer, size_t byteCount);
/// Writes `byteCount` bytes from one stream to another
FORGE_API bool fsCopyStream(FileStream* pDst, FileStream* pSrc, size_t byteCount);

/// Seeks to the specified position in the file, using `baseOffset` as the reference offset.
FORGE_API bool fsSeekStream(FileStream* pStream, SeekBaseOffset baseOffset, ssize_t seekOffset);
FORGE_API bool fsFindStream(FileStream* pStream, const void* pFind, size_t findSize, ssize_t maxSeek, ssize_t *pPosition);
FORGE_API bool fsFindReverseStream(FileStream* pStream, const void* pFind, size_t findSize, ssize_t maxSeek, ssize_t *pPosition);

/// Gets the current seek position in the file.
FORGE_API ssize_t fsGetStreamSeekPosition(const FileStream* stream);

/// Gets the current size of the file. Returns -1 if the size is unknown or unavailable.
FORGE_API ssize_t fsGetStreamFileSize(const FileStream* stream);

/// Flushes all writes to the file stream to the underlying subsystem.
FORGE_API bool fsFlushStream(FileStream* stream);

/// Returns whether the current seek position is at the end of the file stream.
FORGE_API bool fsStreamAtEnd(const FileStream* stream);

/// Get property of a stream (minizip requires such function)
FORGE_API bool fsGetStreamPropInt64(FileStream* pStream, int32_t prop, int64_t *pValue);

/// Set property of a stream (minizip requires such function)
FORGE_API bool fsSetStreamPropInt64(FileStream* pStream, int32_t prop, int64_t value);

/************************************************************************/
// MARK: - Memory stream functions
/************************************************************************/

/// Gets buffer pointer from the begining of memory stream
FORGE_API bool fsGetMemoryStreamBuffer(FileStream* pStream, const void** pBuf);
/// Gets buffer pointer from the begining of memory stream with a given offset
FORGE_API bool fsGetMemoryStreamBufferAt(FileStream* pStream, ssize_t offset, const void** pBuf);


/************************************************************************/
// MARK: - Minor filename manipulation
/************************************************************************/
/// Appends `pathComponent` to `basePath`, where `basePath` is assumed to be a directory.
FORGE_API void fsAppendPathComponent(const char* basePath, const char* pathComponent, char* output);

/// Appends `newExtension` to `basePath`.
/// If `basePath` already has an extension, `newExtension` will be appended to the end.
FORGE_API void fsAppendPathExtension(const char* basePath, const char* newExtension, char* output);

/// Appends `newExtension` to `basePath`.
/// If `basePath` already has an extension, its previous extension will be replaced by `newExtension`.
FORGE_API void fsReplacePathExtension(const char* path, const char* newExtension, char* output);

/// Get `path`'s parent path, excluding the end seperator. 
FORGE_API void fsGetParentPath(const char* path, char* output);

/// Get `path`'s file name, without extension or parent path.
FORGE_API void fsGetPathFileName(const char* path, char* output);

/// Returns `path`'s extension, excluding the '.'.
FORGE_API void fsGetPathExtension(const char* path, char* output);
/************************************************************************/
// MARK: - Directory queries
/************************************************************************/
/// Returns location set for resource directory in fsSetPathForResourceDir.
FORGE_API const char* fsGetResourceDirectory(ResourceDirectory resourceDir);
/// Returns Resource Mount point for resource directory
FORGE_API ResourceMount fsGetResourceDirectoryMount(ResourceDirectory resourceDir);

/// Sets the relative path for `resourceDir` from `mount` to `bundledFolder`.
/// The `resourceDir` will making use of the given IFileSystem `pIO` file functions.
/// When `mount` is set to `RM_CONTENT` for a `resourceDir`, this directory is marked as a bundled resource folder.
/// Bundled resource folders should only be used for Read operations.
/// NOTE: A `resourceDir` can only be set once.
FORGE_API void fsSetPathForResourceDir(IFileSystem* pIO, ResourceMount mount, ResourceDirectory resourceDir, const char* bundledFolder);
/************************************************************************/
// MARK: - File Queries
/************************************************************************/
/// Gets the time of last modification for the file at `fileName`, within 'resourceDir'.
FORGE_API time_t fsGetLastModifiedTime(ResourceDirectory resourceDir, const char* fileName);
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
	if (strcmp(modeStr, "w+") == 0)
	{
		return FM_READ_WRITE;
	}
	if (strcmp(modeStr, "wb+") == 0)
	{
		return FM_READ_WRITE_BINARY;
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
static inline FORGE_CONSTEXPR const char* fsOverwriteFileModeToString(FileMode mode)
{

	switch (mode)
	{
	case FM_READ_WRITE: return "w+";
	case FM_READ_WRITE_BINARY: return "wb+";
	default: return fsFileModeToString(mode);
	}
}
#ifdef __cplusplus
} // extern "C"
#endif
