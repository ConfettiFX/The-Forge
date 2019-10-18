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

#include "FileSystemInternal.h"
#ifndef FORGE_DISABLE_ZIP
#include "ZipFileSystem.h"
#endif

#include "../Interfaces/ILog.h"
#include "../Interfaces/IMemory.h"

// MARK: - Initialization

bool fsInitAPI(void)
{
	Path* resourceDirPath = fsCopyProgramDirectoryPath();
	if (!resourceDirPath)
		return false;
	
	fsSetResourceDirectoryRootPath(resourceDirPath);
	fsFreePath(resourceDirPath);

#ifdef USE_MEMORY_TRACKING
	Path* logFilePath = fsCopyLogFileDirectoryPath();
	if (!logFilePath)
		return false;
	mmgr_setLogFileDirectory(fsGetPathAsNativeString(logFilePath));
	fsFreePath(logFilePath);

	Path* executablePath = fsCopyExecutablePath();
	if (!executablePath)
		return false;
	
	PathComponent executableName = fsGetPathFileName(executablePath);
	mmgr_setExecutableName(executableName.buffer, executableName.length);
	fsFreePath(executablePath);
#endif

	return true;
}

void fsDeinitAPI(void)
{
	fsResetResourceDirectories();
}

// MARK: - FileMode

FileMode fsFileModeFromString(const char* modeStr)
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

	LOGF(LogLevel::eERROR, "Unhandled FileMode string %s", modeStr);

	return (FileMode)0;
}

const char* fsFileModeToString(FileMode mode)
{
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
		default: LOGF(LogLevel::eERROR, "Unhandled FileMode %i", mode); return "";
	}
}

// MARK: - Path

static Path* fsAllocatePath(size_t pathLength)
{
	Path* path = (Path*)conf_malloc(sizeof(Path) + pathLength);
	memset(path, 0, sizeof(Path) + pathLength);
	return path;
}

Path* fsCreatePath(const FileSystem* fileSystem, const char* absolutePathString)
{
	if (!fileSystem) { return NULL; }

	// Allocate a stack buffer for the base path.
	struct
	{
		Path path;
		char pathBuffer[16];
	} stackPath = {};

	stackPath.path.pFileSystem = (FileSystem*)fileSystem;

	size_t pathComponentOffset;
	if (!fileSystem->FormRootPath(absolutePathString, &stackPath.path, &pathComponentOffset))
	{
		LOGF(LogLevel::eERROR, "Path string '%s' is not an absolute path", absolutePathString);
		return NULL;
	}

	return fsAppendPathComponent(&stackPath.path, absolutePathString + pathComponentOffset);
}

Path* fsCopyPath(const Path* path)
{
	if (!path) { return NULL; }

	size_t pathSize = Path_SizeOf(path);
	Path*  newPath = (Path*)conf_malloc(pathSize);
	memcpy(newPath, path, pathSize);

	return newPath;
}

void fsFreePath(Path* path)
{
	if (!path) { return; }
	conf_free(path);
}

FileSystem* fsGetPathFileSystem(const Path* path) 
{ 
	if (!path) { return NULL; }
	return path->pFileSystem; 
}

size_t fsGetPathLength(const Path* path) 
{ 
	if (!path) { return 0; }
	return path->mPathLength; 
}

Path* fsAppendPathComponent(const Path* basePath, const char* pathComponent)
{
	if (!basePath) { return NULL; }

	if (!pathComponent) { return fsCopyPath(basePath); }

	const size_t rootPathLength = basePath->pFileSystem->GetRootPathLength();
	const char   directorySeparator = basePath->pFileSystem->GetPathDirectorySeparator();
	const char   forwardSlash = '/';    // Forward slash is accepted on all platforms as a path component.

	size_t componentLength = strlen(pathComponent);
	size_t maxPathLength = basePath->mPathLength + componentLength + 1;    // + 1 due to a possible added directory slash.
	Path*  newPath = fsAllocatePath(maxPathLength);
	newPath->pFileSystem = basePath->pFileSystem;

	char* newPathBuffer = &newPath->mPathBufferOffset;
	strncpy(newPathBuffer, &basePath->mPathBufferOffset, basePath->mPathLength);

	size_t newPathLength = basePath->mPathLength;

	if (newPathLength > 0 && newPathBuffer[newPathLength - 1] != directorySeparator)
	{
		// Append a trailing slash to the directory.
		newPathBuffer[newPathLength] = directorySeparator;
		newPathLength += 1;
	}

	// ./ or .\ means current directory
	// ../ or ..\ means parent directory.

	for (size_t i = 0; i < componentLength; i += 1)
	{
		if ((pathComponent[i] == directorySeparator || pathComponent[i] == forwardSlash) &&
			newPathBuffer[newPathLength - 1] != directorySeparator)
		{
			// We've encountered a new directory.
			newPathBuffer[newPathLength] = directorySeparator;
			newPathLength += 1;
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

					if (newPathLength > 1 && newPathBuffer[newPathLength - 1] == directorySeparator)
					{
						// Delete any trailing directory separator.
						newPathLength -= 1;
					}

					// Backtrack until we come to the next directory separator
					for (; newPathLength > rootPathLength; newPathLength -= 1)
					{
						if (newPathBuffer[newPathLength - 1] == directorySeparator)
						{
							break;
						}
					}

					if (newPathLength <= rootPathLength)
					{
						// We couldn't find a parent directory.
						LOGF(
							LogLevel::eERROR, "Path component '%s' escapes the root of path '%s'", pathComponent,
							&basePath->mPathBufferOffset);
						fsFreePath(newPath);
						return NULL;
					}

					i = j;    // Skip past the ../
					continue;
				}
			}
		}

		newPathBuffer[newPathLength] = pathComponent[i];
		newPathLength += 1;
	}

	if (newPathLength > rootPathLength && newPathBuffer[newPathLength - 1] == directorySeparator)
	{
		// Delete any trailing directory separator.
		newPathLength -= 1;
	}

	newPathBuffer[newPathLength] = 0;
	newPath->mPathLength = newPathLength;
	return newPath;
}

Path* fsAppendPathExtension(const Path* basePath, const char* extension)
{
	if (!basePath) { return NULL; }

	const char directorySeparator = basePath->pFileSystem->GetPathDirectorySeparator();
	const char forwardSlash = '/';    // Forward slash is accepted on all platforms as a path component.

	size_t extensionLength = strlen(extension);
	size_t maxPathLength = basePath->mPathLength + extensionLength + 1;    // + 1 due to a possible added directory slash.
	Path*  newPath = fsAllocatePath(maxPathLength);
	newPath->pFileSystem = basePath->pFileSystem;

	char* newPathBuffer = &newPath->mPathBufferOffset;
	strncpy(newPathBuffer, &basePath->mPathBufferOffset, basePath->mPathLength);

	size_t newPathLength = basePath->mPathLength;

	if (extensionLength > 0 && extension[0] == '.')
	{
		extensionLength -= 1;
		extension += 1;
	}

	if (extensionLength == 0)
	{
		newPath->mPathLength = basePath->mPathLength;
		return newPath;
	}

	newPathBuffer[newPathLength] = '.';
	newPathLength += 1;

	for (size_t i = 0; i < extensionLength; i += 1)
	{
		if (extension[i] == directorySeparator || extension[i] == forwardSlash)
		{
			LOGF(LogLevel::eERROR, "Extension '%s' contains directory specifiers", extension);
			fsFreePath(newPath);
			return NULL;
		}

		newPathBuffer[newPathLength] = extension[i];
		newPathLength += 1;
	}

	if (newPathLength > 1 && newPathBuffer[newPathLength - 1] == '.')
	{
		LOGF(LogLevel::eERROR, "Extension '%s' ends with a '.' character", extension);
		fsFreePath(newPath);
		return NULL;
	}

	newPathBuffer[newPathLength] = 0;
	newPath->mPathLength = newPathLength;
	return newPath;
}

// MARK: Path Components

/// Splits this path into its components and returns those components by reference.
/// The out values is guaranteed to live for as long as the Path object lives.
void fsGetPathComponents(const Path* path, PathComponent* directoryName, PathComponent* fileName, PathComponent* extension)
{
	if (directoryName)
	{
		directoryName->buffer = NULL;
		directoryName->length = 0;
	}

	if (fileName)
	{
		fileName->buffer = NULL;
		fileName->length = 0;
	}

	if (extension)
	{
		extension->buffer = NULL;
		extension->length = 0;
	}
	
	if (!path) { return; }

	const char  directorySeparator = path->pFileSystem->GetPathDirectorySeparator();
	const char* pathString = fsGetPathAsNativeString(path);

	const char* extensionEnd = &pathString[path->mPathLength];
	const char* extensionStart = NULL;
	const char* fileNameEnd = extensionEnd;
	const char* fileNameStart = pathString;
	const char* directoryNameEnd = NULL;
	const char* directoryNameStart = NULL;

	for (size_t i = path->mPathLength - 1; i > 0; i--)
	{
		if (pathString[i] == '.' && extensionStart == NULL)
		{
			extensionStart = &pathString[i + 1];
			fileNameEnd = &pathString[i];

			if (directoryName == NULL && fileName == NULL)
			{
				break;    // We don't need to keep searching since only the extension was requested.
			}
		}
		else if (pathString[i] == directorySeparator)
		{
			if (directoryNameEnd != NULL)
			{
				directoryNameStart = &pathString[i + 1];
				break;
			}
			else
			{
				fileNameStart = &pathString[i + 1];
				directoryNameEnd = &pathString[i];

				if (directoryName == NULL)
				{
					break;    // We don't need to keep searching since the directory was not requested.
				}
			}
		}
	}

	if (directoryName && directoryNameStart != NULL)
	{
		directoryName->buffer = directoryNameStart;
		directoryName->length = directoryNameEnd - directoryNameStart;
	}

	if (fileName)
	{
		fileName->buffer = fileNameStart;
		fileName->length = fileNameEnd - fileNameStart;
	}

	if (extension && extensionStart != NULL)
	{
		extension->buffer = extensionStart;
		extension->length = extensionEnd - extensionStart;
	}
}

Path* fsReplacePathExtension(const Path* path, const char* newExtension)
{
	if (!path) { return NULL; }

	if (!newExtension) { newExtension = ""; }

	const char directorySeparator = path->pFileSystem->GetPathDirectorySeparator();

	if (newExtension[0] == '.')
	{
		newExtension += 1;    // Skip over the first '.'.
	}

	// Validate the extension and calculate its length.
	size_t newExtensionLength = 0;
	while (newExtension[newExtensionLength] != 0)
	{
		ASSERT(newExtension[newExtensionLength] != directorySeparator && "Directory separator present in file extension.");
		newExtensionLength += 1;
	}

	PathComponent currentExtension = fsGetPathExtension(path);
	size_t        newPathLength = path->mPathLength + newExtensionLength - currentExtension.length;

	if (currentExtension.buffer == NULL)
	{
		newPathLength += 1;    // for '.'
	}

	Path* newPath = fsAllocatePath(newPathLength);
	newPath->pFileSystem = path->pFileSystem;
	newPath->mPathLength = newPathLength;

	size_t basePathLength = path->mPathLength - currentExtension.length;
	strncpy(&newPath->mPathBufferOffset, &path->mPathBufferOffset, basePathLength);

	if (currentExtension.buffer == NULL)
	{
		(&newPath->mPathBufferOffset)[basePathLength] = '.';
		basePathLength += 1;
	}

	strncpy((&newPath->mPathBufferOffset) + basePathLength, newExtension, newExtensionLength);

	return newPath;
}

Path* fsCopyParentPath(const Path* path)
{
	if (!path) { return NULL; }

	// The parent path ends at the end of the directory name.
	PathComponent directoryComponent = fsGetPathDirectoryName(path);
	if (directoryComponent.buffer == NULL)
	{
		return NULL;
	}

	const char* directoryComponentEnd = directoryComponent.buffer + directoryComponent.length;
	const char* pathStart = fsGetPathAsNativeString(path);

	size_t newPathLength = (directoryComponent.buffer != NULL) ? (directoryComponentEnd - pathStart) : 0;

	Path* newPath = fsAllocatePath(newPathLength);
	newPath->pFileSystem = path->pFileSystem;
	newPath->mPathLength = newPathLength;
	strncpy(&newPath->mPathBufferOffset, pathStart, newPathLength);

	return newPath;
}

/// Returns this path's directory name. Undefined if this path does not belong to a disk file system.
/// The return value is guaranteed to live for as long as the Path object lives.
PathComponent fsGetPathDirectoryName(const Path* path)
{
	if (!path) { return { NULL, 0 }; }

	PathComponent component;
	fsGetPathComponents(path, &component, NULL, NULL);
	return component;
}

PathComponent fsGetPathFileName(const Path* path)
{
	if (!path) { return { NULL, 0 }; }

	PathComponent component;
	fsGetPathComponents(path, NULL, &component, NULL);
	return component;
}

PathComponent fsGetPathExtension(const Path* path)
{
	if (!path) { return { NULL, 0 }; }

	PathComponent component;
	fsGetPathComponents(path, NULL, NULL, &component);
	return component;
}

size_t fsGetLowercasedPathExtension(const Path* path, char* buffer, size_t maxLength)
{
	if (!path) 
	{ 
		if (maxLength > 0) { *buffer = 0; }
		return 0;
	}

	PathComponent extension = fsGetPathExtension(path);

	size_t charsToWrite = min(maxLength, extension.length);
	for (size_t i = 0; i < charsToWrite; i += 1)
	{
		buffer[i] = tolower(extension.buffer[i]);
	}

	if (charsToWrite < maxLength)
	{
		buffer[charsToWrite] = 0;
	}
	return extension.length;
}

const char* fsGetPathAsNativeString(const Path* path)
{
	if (!path || path->mPathLength == 0)
	{
		return NULL;
	}
	return &path->mPathBufferOffset;
}

#ifdef _WIN32
#define strncasecmp _strnicmp
#endif

bool fsPathsEqual(const Path* pathA, const Path* pathB)
{
	if (pathA == pathB)
	{
		return true;    // Pointer equality means they must be equal.
	}

	if (!pathA || !pathB)
	{
		return false;
	}

	return pathA->pFileSystem == pathB->pFileSystem && pathA->mPathLength == pathB->mPathLength &&
		   (pathA->pFileSystem->IsCaseSensitive()
				? (strncmp(fsGetPathAsNativeString(pathA), fsGetPathAsNativeString(pathB), pathA->mPathLength) == 0)
				: (strncasecmp(fsGetPathAsNativeString(pathA), fsGetPathAsNativeString(pathB), pathA->mPathLength) == 0));
}

// MARK: - FileSystem

FileSystem::FileSystem(FileSystemKind kind): mKind(kind) { };

FileSystemKind fsGetFileSystemKind(const FileSystem* fileSystem) { return fileSystem->mKind; }

/// Creates a new file-system with its root at rootPath.
/// If rootPath is a compressed zip file, the file system will be the contents of the zip file.
FileSystem* fsCreateFileSystemFromFileAtPath(const Path* rootPath, FileSystemFlags flags)
{
	if (!rootPath) { return NULL; }

	PathComponent extension = fsGetPathExtension(rootPath);
#ifndef FORGE_DISABLE_ZIP
	if (extension.length == 3 && stricmp("zip", extension.buffer) == 0)
	{
		return ZipFileSystem::CreateWithRootAtPath(rootPath, flags);
	}
#endif
	return NULL;
}

void fsFreeFileSystem(FileSystem* fileSystem)
{
	if (!fileSystem) { return; }

	if (fileSystem->mKind == FSK_SYSTEM)
	{
		return;    // The system FileSystem is a singleton that can never be deleted.
	}

	conf_delete(fileSystem);
}

FileStream* fsOpenFile(const Path* filePath, FileMode mode) 
{ 
	if (!filePath) { return NULL; }
	return filePath->pFileSystem->OpenFile(filePath, mode); 
}

bool fsFileSystemIsReadOnly(const FileSystem* fileSystem) 
{ 
	if (!fileSystem) { return NULL; }
	return fileSystem->IsReadOnly(); 
}

// MARK: - Resource Directories

#if defined(DIRECT3D12) && !defined(_DURANGO)
#define SHADER_DIR "Shaders/D3D12"
#elif defined(DIRECT3D11)
#define SHADER_DIR "Shaders/D3D11"
#elif defined(VULKAN)
#define SHADER_DIR "Shaders/Vulkan"
#elif defined(__APPLE__)
#define SHADER_DIR "Shaders/Metal"
#else
#define SHADER_DIR "Shaders"
#endif

const char* gResourceDirectoryDefaults[RD_COUNT] = {
	SHADER_DIR "/Binary/",    // RD_BinShaders
	SHADER_DIR "/",           // RD_SHADER_SOURCES
	"Textures/",              // RD_TEXTURES
	"Meshes/",                // RD_MESHES
	"Fonts/",                 // RD_BUILTIN_FONTS
	"GPUCfg/",                // RD_GpuConfig
	"Animation/",             // RD_ANIMATIONS
	"Audio/",                 // RD_AUDIO
};

Path* gResourceDirectoryOverrides[RD_COUNT];

Path* fsCopyResourceDirectoryRootPath() { 
	return fsCopyPath(gResourceDirectoryOverrides[RD_ROOT]); 
}

void fsSetResourceDirectoryRootPath(const Path* path) { 
	fsSetPathForResourceDirectory(RD_ROOT, path);
}

Path* fsCopyPathForResourceDirectory(ResourceDirectory resourceDir)
{
	if (gResourceDirectoryOverrides[resourceDir])
	{
		return fsCopyPath(gResourceDirectoryOverrides[resourceDir]);
	}

	Path* rootPath = gResourceDirectoryOverrides[RD_ROOT];
	ASSERT(rootPath);
	return fsAppendPathComponent(rootPath, gResourceDirectoryDefaults[resourceDir]);
}

void fsSetRelativePathForResourceDirectory(ResourceDirectory resourceDir, const char* relativePath)
{
	Path* rootPath = fsCopyResourceDirectoryRootPath();
	Path* fullPath = fsAppendPathComponent(rootPath, relativePath);

	fsSetPathForResourceDirectory(resourceDir, fullPath);

	fsFreePath(rootPath);
	fsFreePath(fullPath);
}

void fsSetPathForResourceDirectory(ResourceDirectory resourceDir, const Path* path)
{
	ASSERT(resourceDir != RD_ROOT || path != NULL);

	fsFreePath(gResourceDirectoryOverrides[resourceDir]);    // fsFreePath checks for NULL
	gResourceDirectoryOverrides[resourceDir] = fsCopyPath(path);
}

// NOTE: not thread-safe. It is the application's responsibility to ensure that no modifications to the file system
// are occurring at the time of this call.
void fsResetResourceDirectories() 
{ 
	for (size_t i = 0; i < RD_COUNT; i += 1)
	{
		fsFreePath(gResourceDirectoryOverrides[i]);    // fsFreePath checks for NULL
	}
	memset(gResourceDirectoryOverrides, 0, RD_COUNT * sizeof(Path*));
}

const char* fsGetDefaultRelativePathForResourceDirectory(ResourceDirectory resourceDir) { return gResourceDirectoryDefaults[resourceDir]; }

size_t fsGetExecutableName(char* buffer, size_t maxLength)
{
	Path*         executablePath = fsCopyExecutablePath();
	PathComponent fileName = fsGetPathFileName(executablePath);

	strncpy(buffer, fileName.buffer, min(fileName.length, maxLength));
	if (maxLength > fileName.length)
	{
		buffer[fileName.length] = 0;
	}

	fsFreePath(executablePath);
	return fileName.length;
}

// MARK: - File and Directory Queries

time_t fsGetCreationTime(const Path* filePath) 
{ 
	if (!filePath) { return {}; }
	return filePath->pFileSystem->GetCreationTime(filePath); 
}

time_t fsGetLastAccessedTime(const Path* filePath) 
{ 
	if (!filePath) { return {}; }
	return filePath->pFileSystem->GetLastAccessedTime(filePath); 
}

time_t fsGetLastModifiedTime(const Path* filePath) 
{ 
	if (!filePath) { return {}; }
	return filePath->pFileSystem->GetLastModifiedTime(filePath);
}

bool fsCopyFile(const Path* sourcePath, const Path* destinationPath, bool overwriteIfExists)
{
	if (!sourcePath || !destinationPath) { return false; }

	// If the files are on different file systems, copy using FileStreams.
	if (sourcePath->pFileSystem != destinationPath->pFileSystem)
	{
		if (!overwriteIfExists && fsFileExists(destinationPath)) { return false; }

		FileStream* toRead = fsOpenFile(sourcePath, FM_READ_BINARY);
		if (!toRead) { return false; }
		
		FileStream* toWrite = fsOpenFile(destinationPath, FM_WRITE_BINARY);
		if (!toWrite)
		{
			fsCloseStream(toRead);
			return false;
		}

		size_t fileSize = fsGetStreamFileSize(toRead);
		bool success = true;
	
		if (fileSize <= 0)
		{
			// Unknown size; copy byte by byte.
			while (success && !fsStreamAtEnd(toRead))
			{
				uint8_t byte = 0;
				if (!success || fsReadFromStream(toRead, &byte, sizeof(uint8_t)) != sizeof(uint8_t)) { success = false; }
				
				if (!success || fsWriteToStream(toWrite, &byte, sizeof(uint8_t)) != sizeof(uint8_t)) { success = false; }
			}
		}
		else 
		{
			void* tempBuffer = conf_malloc(fileSize);
			if (fsReadFromStream(toRead, tempBuffer, fileSize) != fileSize) { success = false; }
			if (!success || fsWriteToStream(toWrite, tempBuffer, fileSize) != fileSize) { success = false; }
			
			conf_free(tempBuffer);
		}

		fsCloseStream(toRead);
		fsCloseStream(toWrite);
		return success;
	}
	// Otherwise, use the specialised copy implementation of their FileSystem
	else
	{
		return sourcePath->pFileSystem->CopyFile(sourcePath, destinationPath, overwriteIfExists);
	}
}

bool fsCreateDirectory(const Path* directoryPath) 
{ 
	if (!directoryPath) { return false; }
	return directoryPath->pFileSystem->CreateDirectory(directoryPath);
}

bool fsDeleteFile(const Path* path) 
{ 
	if (!path) { return false; }
	return path->pFileSystem->DeleteFile(path); 
}

bool fsFileExists(const Path* path) 
{ 
	if (!path) { return false; }
	return path->pFileSystem->FileExists(path); 
}

bool fsDirectoryExists(const Path* path) 
{ 
	if (!path) { return false; }
	return path->pFileSystem->IsDirectory(path); 
}

void fsEnumerateFilesWithExtension(
	const Path* directory, const char* extension, bool (*processFile)(const Path*, void* userData), void* userData)
{
	if (!directory) { return; }
	return directory->pFileSystem->EnumerateFilesWithExtension(directory, extension, processFile, userData);
}

void fsEnumerateSubDirectories(const Path* directory, bool (*processDirectory)(const Path*, void* userData), void* userData)
{
	if (!directory) { return; }
	return directory->pFileSystem->EnumerateSubDirectories(directory, processDirectory, userData);
}

static bool EnumeratePathsFunc(const Path* path, void* userData)
{
	eastl::vector<PathHandle>* paths = (eastl::vector<PathHandle>*)userData;
	paths->push_back(fsCopyPath(path));
	return true;
}

eastl::vector<PathHandle> fsGetFilesWithExtension(const Path* directory, const char* extension)
{
	eastl::vector<PathHandle> files;
	fsEnumerateFilesWithExtension(directory, extension, EnumeratePathsFunc, &files);
	return files;
}

eastl::vector<PathHandle> fsGetSubDirectories(const Path* directory)
{
	eastl::vector<PathHandle> files;
	fsEnumerateSubDirectories(directory, EnumeratePathsFunc, &files);
	return files;
}

// MARK: - Resource Directory Utilities

bool fsPlatformUsesBundledResources()
{
#if defined(__ANDROID__) || defined(_DURANGO) || defined(TARGET_IOS) || defined(FORGE_IGNORE_PSZBASE)
	return true;
#else
	return false;
#endif
}

FileStream*
	fsOpenFileInResourceDirectory(ResourceDirectory resourceDir, const char* relativePath, FileMode mode)
{
	Path* path = fsCopyPathInResourceDirectory(resourceDir, relativePath);
	if (!path) { return NULL; }

	FileStream* fh = fsOpenFile(path, mode);
	fsFreePath(path);
	return fh;
}

Path* fsCopyPathInResourceDirectory(ResourceDirectory resourceDir, const char* relativePath)
{
	Path* resourceDirPath = fsCopyPathForResourceDirectory(resourceDir);
	Path* result = fsAppendPathComponent(resourceDirPath, relativePath);
	fsFreePath(resourceDirPath);

	return result;
}

bool fsFileExistsInResourceDirectory(ResourceDirectory resourceDir, const char* relativePath)
{
	Path* path = fsCopyPathInResourceDirectory(resourceDir, relativePath);
	bool  fileExists = fsFileExists(path);
	fsFreePath(path);

	return fileExists;
}

// MARK: - FileStream Functions

size_t fsReadFromStream(FileStream* stream, void* outputBuffer, size_t bufferSizeInBytes)
{
	if (!stream) { return 0; }
	return stream->Read(outputBuffer, bufferSizeInBytes);
}

size_t __fsScanFromStream(FileStream* stream, int* bytesRead, const char* format, ...)
{
	if (!stream) { return 0; }

    va_list args;
    va_start(args, format);
    size_t itemsRead = stream->Scan(format, args, bytesRead);
    va_end(args);
    return itemsRead;
}

size_t fsWriteToStream(FileStream* stream, const void* sourceBuffer, size_t byteCount) 
{ 
	if (!stream) { return 0; }
	return stream->Write(sourceBuffer, byteCount); 
}

size_t fsPrintToStream(FileStream* stream, const char* format, ...)
{
	if (!stream) { return 0; }

    va_list args;
    va_start(args, format);
    size_t charsWritten = stream->Print(format, args);
    va_end(args);
    return charsWritten;
}

bool fsSeekStream(FileStream* stream, SeekBaseOffset baseOffset, ssize_t seekOffset) 
{ 
	if (!stream) { return false; }
	return stream->Seek(baseOffset, seekOffset); 
}

ssize_t fsGetStreamSeekPosition(const FileStream* stream) 
{ 
	if (!stream) { return -1; }
	return stream->GetSeekPosition(); 
}

ssize_t fsGetStreamFileSize(const FileStream* stream)
{
	if (!stream) { return 0; }
	return stream->GetFileSize();
}

void fsFlushStream(FileStream* stream) 
{ 
	if (!stream) { return; }
	return stream->Flush(); 
}

bool fsStreamAtEnd(const FileStream* stream)
{
	if (!stream) { return true; }
	return stream->IsAtEnd();
}

bool fsCloseStream(FileStream* stream)
{
	if (!stream) { return false; }
	return stream->Close();
}

// MARK: FileStream Utilities for Reading

int64_t fsReadFromStreamInt64(FileStream* stream) { return fsReadFromStreamType<int64_t>(stream); }

int32_t fsReadFromStreamInt32(FileStream* stream) { return fsReadFromStreamType<int32_t>(stream); }

int16_t fsReadFromStreamInt16(FileStream* stream) { return fsReadFromStreamType<int16_t>(stream); }

int8_t fsReadFromStreamInt8(FileStream* stream) { return fsReadFromStreamType<int8_t>(stream); }

uint64_t fsReadFromStreamUInt64(FileStream* stream) { return fsReadFromStreamType<uint64_t>(stream); }

uint32_t fsReadFromStreamUInt32(FileStream* stream) { return fsReadFromStreamType<uint32_t>(stream); }

uint16_t fsReadFromStreamUInt16(FileStream* stream) { return fsReadFromStreamType<uint16_t>(stream); }

uint8_t fsReadFromStreamUInt8(FileStream* stream) { return fsReadFromStreamType<uint8_t>(stream); }

bool fsReadFromStreamBool(FileStream* stream) { return fsReadFromStreamType<uint8_t>(stream) != 0; }

float fsReadFromStreamFloat(FileStream* stream) { return fsReadFromStreamType<float>(stream); }

double fsReadFromStreamDouble(FileStream* stream) { return fsReadFromStreamType<double>(stream); }

size_t fsReadFromStreamString(FileStream* stream, char* buffer, size_t maxLength)
{
	size_t i = 0;
	for (; i < maxLength && !fsStreamAtEnd(stream); i += 1)
	{
		char nextChar = fsReadFromStreamType<char>(stream);
		buffer[i] = nextChar;
		if (nextChar == 0)
		{
			break;
		}
	}
	return i;
}

size_t fsReadFromStreamLine(FileStream* stream, char* buffer, size_t maxLength)
{
	size_t i = 0;
	for (; i < maxLength && !fsStreamAtEnd(stream); i += 1)
	{
		char nextChar = fsReadFromStreamType<char>(stream);
		if (nextChar == 0 || nextChar == '\n')
		{
			break;
		}
		if (nextChar == '\r')
		{
			if (fsReadFromStreamType<char>(stream) == '\n')
			{
				break;
			}
			else
			{
				// We're not looking at a "\r\n" sequence, so add the '\r' to the buffer.
				fsSeekStream(stream, SBO_CURRENT_POSITION, -1);
			}
		}
		buffer[i] = nextChar;
	}

	if (i < maxLength)
	{
		// Add the null terminator
		buffer[i] = 0;
	}

	return i;
}

float2 fsReadFromStreamFloat2(FileStream* stream) { return fsReadFromStreamType<float2>(stream); }

float3 fsReadFromStreamFloat3(FileStream* stream) { return fsReadFromStreamType<float3>(stream); }

float3 fsReadFromStreamPackedFloat3(FileStream* stream, float maxAbsCoord)
{
	float   invV = maxAbsCoord / 32767.0f;
	int16_t coords[3];

	fsReadFromStream(stream, coords, 3 * sizeof(int16_t));

	float3 ret(coords[0] * invV, coords[1] * invV, coords[2] * invV);
	return ret;
}

float4 fsReadFromStreamFloat4(FileStream* stream) { return fsReadFromStreamType<float4>(stream); }

eastl::string fsReadFromStreamSTLString(FileStream* stream)
{
	eastl::string result;

	size_t i = 0;
	for (; !fsStreamAtEnd(stream); i += 1)
	{
		char nextChar = fsReadFromStreamType<char>(stream);
		if (nextChar == 0)
		{
			break;
		}
		result.push_back(nextChar);
	}
	return result;
}

eastl::string fsReadFromStreamSTLLine(FileStream* stream)
{
	eastl::string result;

	while (!fsStreamAtEnd(stream))
	{
		char nextChar = fsReadFromStreamType<char>(stream);
		if (nextChar == 0 || nextChar == '\n')
		{
			break;
		}
		if (nextChar == '\r')
		{
			if (fsReadFromStreamType<char>(stream) == '\n')
			{
				break;
			}
			else
			{
				// We're not looking at a "\r\n" sequence, so add the '\r' to the buffer.
				fsSeekStream(stream, SBO_CURRENT_POSITION, -1);
			}
		}
		result.push_back(nextChar);
	}

	return result;
}

// MARK: FileStream Utilities for Writing

bool fsWriteToStreamInt64(FileStream* stream, int64_t value) { return fsWriteToStreamType(stream, value); }

bool fsWriteToStreamInt32(FileStream* stream, int32_t value) { return fsWriteToStreamType(stream, value); }

bool fsWriteToStreamInt16(FileStream* stream, int16_t value) { return fsWriteToStreamType(stream, value); }

bool fsWriteToStreamInt8(FileStream* stream, int8_t value) { return fsWriteToStreamType(stream, value); }

bool fsWriteToStreamUInt64(FileStream* stream, uint64_t value) { return fsWriteToStreamType(stream, value); }

bool fsWriteToStreamUInt32(FileStream* stream, uint32_t value) { return fsWriteToStreamType(stream, value); }

bool fsWriteToStreamUInt16(FileStream* stream, uint16_t value) { return fsWriteToStreamType(stream, value); }

bool fsWriteToStreamUInt8(FileStream* stream, uint8_t value) { return fsWriteToStreamType(stream, value); }

bool fsWriteToStreamBool(FileStream* stream, bool value) { return fsWriteToStreamType(stream, value); }

bool fsWriteToStreamFloat(FileStream* stream, float value) { return fsWriteToStreamType(stream, value); }

bool fsWriteToStreamDouble(FileStream* stream, double value) { return fsWriteToStreamType(stream, value); }

bool fsWriteToStreamFloat2(FileStream* stream, float2 value) { return fsWriteToStreamType(stream, value); }

bool fsWriteToStreamFloat3(FileStream* stream, float3 value) { return fsWriteToStreamType(stream, value); }

bool fsWriteToStreamPackedFloat3(FileStream* stream, float3 value, float maxAbsCoord)
{
	int16_t coords[3];
	float   v = 32767.0f / maxAbsCoord;

	coords[0] = (int16_t)(clamp(value.getX(), -maxAbsCoord, maxAbsCoord) * v + 0.5f);
	coords[1] = (int16_t)(clamp(value.getY(), -maxAbsCoord, maxAbsCoord) * v + 0.5f);
	coords[2] = (int16_t)(clamp(value.getZ(), -maxAbsCoord, maxAbsCoord) * v + 0.5f);

	if (!fsWriteToStreamType(stream, coords[0]) || !fsWriteToStreamType(stream, coords[1]) || !fsWriteToStreamType(stream, coords[2]))
	{
		return false;
	}

	return true;
}

bool fsWriteToStreamFloat4(FileStream* stream, float4 value) { return fsWriteToStreamType(stream, value); }

bool fsWriteToStreamString(FileStream* stream, const char* value)
{
	size_t i = 0;

	while (true)
	{
		if (!fsWriteToStreamType(stream, value[i]))
		{
			return false;
		}
		if (value[i] == 0)
		{
			break;
		}
		i += 1;
	}

	return true;
}

bool fsWriteToStreamLine(FileStream* stream, const char* value)
{
	size_t i = 0;

	while (true)
	{
		if (value[i] == 0)
		{
			break;
		}
		if (!fsWriteToStreamType(stream, value[i]))
		{
			return false;
		}
		
		i += 1;
	}

	if (!fsWriteToStreamType(stream, (char)'\n'))
	{
		return false;
	}

	return true;
}
