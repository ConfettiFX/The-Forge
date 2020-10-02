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

#include "../Interfaces/ILog.h"
#include "../Interfaces/IMemory.h"

bool PlatformOpenFile(ResourceDirectory resourceDir, const char* fileName, FileMode mode, FileStream* pOut);

typedef struct ResourceDirectoryInfo
{
	IFileSystem* pIO;
	char         mPath[FS_MAX_PATH] = {};
	bool         mBundled;
} ResourceDirectoryInfo;

static ResourceDirectoryInfo gResourceDirectories[RD_COUNT] = {};

/************************************************************************/
// Memory Stream Functions
/************************************************************************/
static inline FORGE_CONSTEXPR size_t AvailableCapacity(FileStream* pStream, size_t requestedCapacity)
{
	return min((ssize_t)requestedCapacity, max((ssize_t)pStream->mSize - (ssize_t)pStream->mMemory.mCursor, (ssize_t)0));
}

static bool MemoryStreamClose(FileStream* pStream)
{
	MemoryStream* mem = &pStream->mMemory;
	if (mem->mOwner)
	{
		tf_free(mem->pBuffer);
	}

	return true;
}

static size_t MemoryStreamRead(FileStream* pStream, void* outputBuffer, size_t bufferSizeInBytes)
{
	if (!(pStream->mMode & FM_READ))
	{
		LOGF(LogLevel::eWARNING, "Attempting to write to read-only buffer");
		return 0;
	}

	size_t bytesToRead = AvailableCapacity(pStream, bufferSizeInBytes);
	memcpy(outputBuffer, pStream->mMemory.pBuffer + pStream->mMemory.mCursor, bytesToRead);
	pStream->mMemory.mCursor += bytesToRead;
	return bytesToRead;
}

static size_t MemoryStreamWrite(FileStream* pStream, const void* sourceBuffer, size_t byteCount)
{
	if (!(pStream->mMode & FM_WRITE))
	{
		LOGF(LogLevel::eWARNING, "Attempting to write to read-only buffer");
		return 0;
	}

	size_t bytesToWrite = AvailableCapacity(pStream, byteCount);
	memcpy(pStream->mMemory.pBuffer + pStream->mMemory.mCursor, sourceBuffer, bytesToWrite);
	pStream->mMemory.mCursor += bytesToWrite;
	return bytesToWrite;
}


static bool MemoryStreamSeek(FileStream* pStream, SeekBaseOffset baseOffset, ssize_t seekOffset)
{
	switch (baseOffset)
	{
	case SBO_START_OF_FILE:
	{
		if (seekOffset < 0 || seekOffset >= pStream->mSize)
		{
			return false;
		}
		pStream->mMemory.mCursor = seekOffset;
	}
	break;
	case SBO_CURRENT_POSITION:
	{
		ssize_t newPosition = (ssize_t)pStream->mMemory.mCursor + seekOffset;
		if (newPosition < 0 || newPosition >= pStream->mSize)
		{
			return false;
		}
		pStream->mMemory.mCursor = (size_t)newPosition;
	}
	break;

	case SBO_END_OF_FILE:
	{
		ssize_t newPosition = (ssize_t)pStream->mSize + seekOffset;
		if (newPosition < 0 || newPosition >= pStream->mSize)
		{
			return false;
		}
		pStream->mMemory.mCursor = (size_t)newPosition;
	}
	break;
	}
	return true;
}

static ssize_t MemoryStreamGetSeekPosition(const FileStream* pStream)
{
	return pStream->mMemory.mCursor;
}

static ssize_t MemoryStreamGetSize(const FileStream* pStream)
{
	return pStream->mSize;
}

static bool MemoryStreamFlush(FileStream*)
{
	// No-op.
	return true;
}

static bool MemoryStreamIsAtEnd(const FileStream* pStream)
{
	return pStream->mMemory.mCursor == pStream->mSize;
}
/************************************************************************/
// File Stream Functions
/************************************************************************/
static bool FileStreamOpen(IFileSystem*, const ResourceDirectory resourceDir, const char* fileName, FileMode mode, FileStream* pOut)
{
	return PlatformOpenFile(resourceDir, fileName, mode, pOut);
}

static bool FileStreamClose(FileStream* pFile)
{
	if (fclose(pFile->pFile) == EOF)
	{
		LOGF(LogLevel::eERROR, "Error closing system FileStream", errno);
		return false;
	}

	return true;
}

static size_t FileStreamRead(FileStream* pFile, void* outputBuffer, size_t bufferSizeInBytes)
{
	size_t bytesRead = fread(outputBuffer, 1, bufferSizeInBytes, pFile->pFile);
	if (bytesRead != bufferSizeInBytes)
	{
		if (ferror(pFile->pFile) != 0)
		{
			LOGF(LogLevel::eWARNING, "Error reading from system FileStream: %s", strerror(errno));
		}
	}
	return bytesRead;
}

static size_t FileStreamWrite(FileStream* pFile, const void* sourceBuffer, size_t byteCount)
{
	if ((pFile->mMode & (FM_WRITE | FM_APPEND)) == 0)
	{
		LOGF(LogLevel::eWARNING, "Writing to FileStream with mode %u", pFile->mMode);
		return 0;
	}

	size_t bytesWritten = fwrite(sourceBuffer, 1, byteCount, pFile->pFile);

	if (bytesWritten != byteCount)
	{
		if (ferror(pFile->pFile) != 0)
		{
			LOGF(LogLevel::eWARNING, "Error writing to system FileStream: %s", strerror(errno));
		}
	}

	return bytesWritten;
}

static bool FileStreamSeek(FileStream* pFile, SeekBaseOffset baseOffset, ssize_t seekOffset)
{
	if ((pFile->mMode & FM_BINARY) == 0 && baseOffset != SBO_START_OF_FILE)
	{
		LOGF(LogLevel::eWARNING, "Text-mode FileStreams only support SBO_START_OF_FILE");
		return false;
	}

	int origin = SEEK_SET;
	switch (baseOffset)
	{
	case SBO_START_OF_FILE: origin = SEEK_SET; break;
	case SBO_CURRENT_POSITION: origin = SEEK_CUR; break;
	case SBO_END_OF_FILE: origin = SEEK_END; break;
	}

	return fseek(pFile->pFile, (long)seekOffset, origin) == 0;
}

static ssize_t FileStreamGetSeekPosition(const FileStream* pFile)
{
	long int result = ftell(pFile->pFile);
	if (result == -1L)
	{
		LOGF(LogLevel::eWARNING, "Error getting seek position in FileStream: %i", errno);
	}
	return result;
}

static ssize_t FileStreamGetSize(const FileStream* pFile)
{
	return pFile->mSize;
}

bool FileStreamFlush(FileStream* pFile)
{
	if (fflush(pFile->pFile) == EOF)
	{
		LOGF(LogLevel::eWARNING, "Error flushing system FileStream: %s", strerror(errno));
		return false;
	}

	return true;
}

bool FileStreamIsAtEnd(const FileStream* pFile)
{
	return feof(pFile->pFile) != 0;
}

/************************************************************************/
// File IO
/************************************************************************/
static IFileSystem gMemoryFileIO =
{
	NULL,
	MemoryStreamClose,
	MemoryStreamRead,
	MemoryStreamWrite,
	MemoryStreamSeek,
	MemoryStreamGetSeekPosition,
	MemoryStreamGetSize,
	MemoryStreamFlush,
	MemoryStreamIsAtEnd
};

static IFileSystem gSystemFileIO =
{
	FileStreamOpen,
	FileStreamClose,
	FileStreamRead,
	FileStreamWrite,
	FileStreamSeek,
	FileStreamGetSeekPosition,
	FileStreamGetSize,
	FileStreamFlush,
	FileStreamIsAtEnd
};

IFileSystem* pSystemFileIO = &gSystemFileIO;

bool fsOpenStreamFromMemory(const void* buffer, size_t bufferSize, FileMode mode, bool owner, FileStream* pOut)
{
	FileStream stream = {};
	stream.mMemory.mCursor = 0;
	stream.mMemory.pBuffer = (uint8_t*)buffer;
	stream.mMemory.mOwner = owner;
	stream.mSize = bufferSize;
	stream.mMode = mode;
	stream.pIO = &gMemoryFileIO;
	*pOut = stream;
	return true;
}

/// Opens the file at `filePath` using the mode `mode`, returning a new FileStream that can be used
/// to read from or modify the file. May return NULL if the file could not be opened.
bool fsOpenStreamFromPath(const ResourceDirectory resourceDir, const char* fileName, FileMode mode, FileStream* pOut)
{
	IFileSystem* io = gResourceDirectories[resourceDir].pIO;
	if (!io)
	{
		LOGF(LogLevel::eERROR, "Trying to get an unset resource directory '%d', make sure the resourceDirectory is set on start of the application", resourceDir);
		return false;
	}

	return io->Open(io, resourceDir, fileName, mode, pOut);
}

/// Closes and invalidates the file stream.
bool fsCloseStream(FileStream* pStream)
{
	return pStream->pIO->Close(pStream);
}

/// Returns the number of bytes read.
size_t fsReadFromStream(FileStream* pStream, void* pOutputBuffer, size_t bufferSizeInBytes)
{
	return pStream->pIO->Read(pStream, pOutputBuffer, bufferSizeInBytes);
}

/// Reads at most `bufferSizeInBytes` bytes from sourceBuffer and writes them into the file.
/// Returns the number of bytes written.
size_t fsWriteToStream(FileStream* pStream, const void* pSourceBuffer, size_t byteCount)
{
	return pStream->pIO->Write(pStream, pSourceBuffer, byteCount);
}

/// Seeks to the specified position in the file, using `baseOffset` as the reference offset.
bool fsSeekStream(FileStream* pStream, SeekBaseOffset baseOffset, ssize_t seekOffset)
{
	return pStream->pIO->Seek(pStream, baseOffset, seekOffset);
}

/// Gets the current seek position in the file.
ssize_t fsGetStreamSeekPosition(const FileStream* pStream)
{
	return pStream->pIO->GetSeekPosition(pStream);
}

/// Gets the current size of the file. Returns -1 if the size is unknown or unavailable.
ssize_t fsGetStreamFileSize(const FileStream* pStream)
{
	return pStream->pIO->GetFileSize(pStream);
}

/// Flushes all writes to the file stream to the underlying subsystem.
bool fsFlushStream(FileStream* pStream)
{
	return pStream->pIO->Flush(pStream);
}

/// Returns whether the current seek position is at the end of the file stream.
bool fsStreamAtEnd(const FileStream* pStream)
{
	return pStream->pIO->IsAtEnd(pStream);
}
/************************************************************************/
// Platform independent filename, extension functions
/************************************************************************/
static inline FORGE_CONSTEXPR const char fsGetDirectorySeparator()
{
#if defined(_WIN32) || defined(_WINDOWS) ||  defined(XBOX)
	return '\\';
#else
	return '/';
#endif
}

void fsAppendPathComponent(const char* basePath, const char* pathComponent, char* output)
{
	const size_t componentLength = strlen(pathComponent);
	const size_t baseLength = strlen(basePath);
	const size_t maxPathLength = baseLength + componentLength + 1;    // + 1 due to a possible added directory slash.

	LOGF_IF(LogLevel::eERROR, maxPathLength >= FS_MAX_PATH, "Component path length '%d' greater than FS_MAX_PATH", maxPathLength);
	ASSERT(maxPathLength < FS_MAX_PATH);

	strncpy(output, basePath, baseLength);
	size_t newPathLength = baseLength;
	output[baseLength] = '\0';

	if (componentLength == 0)
	{
		return;
	}

	const char directorySeparator = fsGetDirectorySeparator();
	const char forwardSlash = '/';    // Forward slash is accepted on all platforms as a path component.


	if (newPathLength != 0 && output[newPathLength - 1] != directorySeparator)
	{
		// Append a trailing slash to the directory.
		strncat(output, &directorySeparator, 1);
		newPathLength += 1;
		output[newPathLength] = '\0';
	}

	// ./ or .\ means current directory
	// ../ or ..\ means parent directory.

	for (size_t i = 0; i < componentLength; i += 1)
	{
		if ((pathComponent[i] == directorySeparator || pathComponent[i] == forwardSlash) &&
			newPathLength != 0 && output[newPathLength - 1] != directorySeparator)
		{
			// We've encountered a new directory.
			strncat(output, &directorySeparator, 1);
			newPathLength += 1;
			output[newPathLength] = '\0';
			continue;
		}
		else if (pathComponent[i] == '.')
		{
			size_t j = i + 1;
			if (j < componentLength)
			{
				if (pathComponent[j] == directorySeparator || pathComponent[j] == forwardSlash)
				{
					// ./, so it's referencing the current directory.
					// We can just skip it.
					i = j;
					continue;
				}
				else if (
					pathComponent[j] == '.' && ++j < componentLength &&
					(pathComponent[j] == directorySeparator || pathComponent[j] == forwardSlash))
				{
					// ../, so referencing the parent directory.

					if (newPathLength > 1 && output[newPathLength - 1] == directorySeparator)
					{
						// Delete any trailing directory separator.
						newPathLength -= 1;
					}

					// Backtrack until we come to the next directory separator
					for (; newPathLength > 0; newPathLength -= 1)
					{
						if (output[newPathLength - 1] == directorySeparator)
						{
							break;
						}
					}

					i = j;    // Skip past the ../
					continue;
				}
			}
		}

		output[newPathLength] = pathComponent[i];
		newPathLength += 1;
		output[newPathLength] = '\0';
	}

	if (output[newPathLength - 1] == directorySeparator)
	{
		// Delete any trailing directory separator.
		newPathLength -= 1;
	}

	output[newPathLength] = '\0';
}

void fsAppendPathExtension(const char* basePath, const char* extension, char* output)
{
	size_t extensionLength = strlen(extension);
	const size_t baseLength = strlen(basePath);
	const size_t maxPathLength = baseLength + extensionLength + 1;    // + 1 due to a possible added directory slash.

	LOGF_IF(LogLevel::eERROR, maxPathLength >= FS_MAX_PATH, "Extension path length '%d' greater than FS_MAX_PATH", maxPathLength);
	ASSERT(maxPathLength < FS_MAX_PATH);

	strncpy(output, basePath, baseLength);

	if (extensionLength == 0)
	{
		return;
	}

	const char directorySeparator = fsGetDirectorySeparator();
	const char forwardSlash = '/';    // Forward slash is accepted on all platforms as a path component.

	// Extension validation
	for (size_t i = 0; i < extensionLength; i += 1)
	{
		LOGF_IF(LogLevel::eERROR, extension[i] == directorySeparator || extension[i] == forwardSlash, "Extension '%s' contains directory specifiers", extension);
		ASSERT(extension[i] != directorySeparator && extension[i] != forwardSlash);
	}
	LOGF_IF(LogLevel::eERROR, extension[extensionLength - 1] == '.', "Extension '%s' ends with a '.' character", extension);


	if (extension[0] == '.')
	{
		extension += 1;
		extensionLength -= 1;
	}

	strncat(output, ".", 1);
	strncat(output, extension, extensionLength);
	output[strlen(output)] = '\0';
}

void fsReplacePathExtension(const char* path, const char* newExtension, char* output)
{
	size_t newExtensionLength = strlen(newExtension);
	const size_t baseLength = strlen(path);
	const size_t maxPathLength = baseLength + newExtensionLength + 1;    // + 1 due to a possible added directory slash.

	ASSERT(baseLength != 0);
	LOGF_IF(LogLevel::eERROR, maxPathLength >= FS_MAX_PATH, "New extension path length '%d' greater than FS_MAX_PATH", maxPathLength);
	ASSERT(maxPathLength < FS_MAX_PATH);

	strncpy(output, path, baseLength);
	size_t newPathLength = baseLength;

	if (newExtensionLength == 0)
	{
		return;
	}

	const char directorySeparator = fsGetDirectorySeparator();
	const char forwardSlash = '/';    // Forward slash is accepted on all platforms as a path component.

	// Extension validation
	for (size_t i = 0; i < newExtensionLength; i += 1)
	{
		LOGF_IF(LogLevel::eERROR, newExtension[i] == directorySeparator || newExtension[i] == forwardSlash, "Extension '%s' contains directory specifiers", newExtension);
		ASSERT(newExtension[i] != directorySeparator && newExtension[i] != forwardSlash);
	}
	LOGF_IF(LogLevel::eERROR, newExtension[newExtensionLength - 1] == '.', "Extension '%s' ends with a '.' character", newExtension);

	if (newExtension[0] == '.')
	{
		newExtension += 1;    // Skip over the first '.'.
		newExtensionLength -= 1;
	}

	char currentExtension[FS_MAX_PATH] = {};
	fsGetPathExtension(path, currentExtension);
	newPathLength -= strlen(currentExtension);
	if (output[newPathLength - 1] != '.')
	{
		output[newPathLength] = '.';
		newPathLength += 1;
	}

	strncpy(output + newPathLength, newExtension, newExtensionLength);
	output[newPathLength + newExtensionLength] = '\0';
}

void fsGetParentPath(const char* path, char* output)
{
	size_t pathLength = strlen(path);
	ASSERT(pathLength != 0);

	const char directorySeparator = fsGetDirectorySeparator();
	const char forwardSlash = '/';    // Forward slash is accepted on all platforms as a path component.

	//Find last seperator
	const char* dirSeperatorLoc = strrchr(path, directorySeparator);
	if (dirSeperatorLoc == NULL)
	{
		dirSeperatorLoc = strrchr(path, forwardSlash);
		if (dirSeperatorLoc == NULL)
		{
			return;
		}
	}

	const size_t outputLength = pathLength - strlen(dirSeperatorLoc);
	strncpy(output, path, outputLength);
	output[outputLength] = '\0';
	return;
}

void fsGetPathFileName(const char* path, char* output)
{
	const size_t pathLength = strlen(path);
	ASSERT(pathLength != 0);

	char parentPath[FS_MAX_PATH] = { 0 };
	fsGetParentPath(path, parentPath);
	size_t parentPathLength = strlen(parentPath);

	const char directorySeparator = fsGetDirectorySeparator();
	const char forwardSlash = '/';    // Forward slash is accepted on all platforms as a path component.
	if (parentPathLength < pathLength && (path[parentPathLength] == directorySeparator || path[parentPathLength] == forwardSlash))
	{
		parentPathLength += 1;
	}

	char extension[FS_MAX_PATH] = { 0 };
	fsGetPathExtension(path, extension);
	const size_t extensionLength = extension[0] != 0 ? strlen(extension) + 1 : 0; // Include dot in the length
	const size_t outputLength = pathLength - parentPathLength - extensionLength;
	strncpy(output, path + parentPathLength, outputLength);
	output[outputLength] = '\0';
}

void fsGetPathExtension(const char* path, char* output)
{
	size_t pathLength = strlen(path);
	ASSERT(pathLength != 0);
	const char* dotLocation = strrchr(path, '.');
	if (dotLocation == NULL)
	{
		return;
	}
	dotLocation += 1;
	const size_t extensionLength = strlen(dotLocation);
	const char directorySeparator = fsGetDirectorySeparator();
	const char forwardSlash = '/';    // Forward slash is accepted on all platforms as a path component.
	if (extensionLength == 0 || dotLocation[0] == forwardSlash || dotLocation[0] == directorySeparator) // Make sure it is not "../"
	{
		return;
	}
	strncpy(output, dotLocation, extensionLength);
	output[extensionLength] = '\0';
}
/************************************************************************/
// Platform independent directory queries
/************************************************************************/
bool fsCreateDirectory(ResourceDirectory resourceDir);

bool fsIsBundledResourceDir(ResourceDirectory resourceDir)
{
	return gResourceDirectories[resourceDir].mBundled;
}

const char* fsGetResourceDirectory(ResourceDirectory resourceDir)
{
	const ResourceDirectoryInfo* dir = &gResourceDirectories[resourceDir];

	if (!dir->mBundled || !dir->pIO)
	{
		LOGF_IF(LogLevel::eERROR, !strlen(dir->mPath), "Trying to get an unset resource directory '%d', make sure the resourceDirectory is set on start of the application", resourceDir);
		ASSERT(strlen(dir->mPath) != 0);
	}
	return dir->mPath;
}

void fsSetPathForResourceDir(IFileSystem* pIO, ResourceMount mount, ResourceDirectory resourceDir, const char* bundledFolder)
{
	ASSERT(pIO);
	ResourceDirectoryInfo* dir = &gResourceDirectories[resourceDir];

	if (strlen(dir->mPath) != 0)
	{
		LOGF(LogLevel::eWARNING, "Resource directory {%d} already set on:'%s'", resourceDir, dir->mPath);
		return;
	}

	if (RM_CONTENT == mount)
	{
		dir->mBundled = true;
	}

	char resourcePath[FS_MAX_PATH] = { 0 };
	fsAppendPathComponent(pIO->GetResourceMount ? pIO->GetResourceMount(mount) : "", bundledFolder, resourcePath);
	strncpy(dir->mPath, resourcePath, FS_MAX_PATH);
	dir->pIO = pIO;

	if (!dir->mBundled)
	{
		if (!fsCreateDirectory(resourceDir))
		{
			LOGF(LogLevel::eERROR, "Could not create direcotry '%s' in filesystem", resourcePath);
		}
	}
}
/************************************************************************/
/************************************************************************/
