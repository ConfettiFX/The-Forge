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

#ifdef __ANDROID__
#include "../FileSystem/FileSystemInternal.h"
#include "../FileSystem/UnixfileSystem.h"
#include "../Interfaces/ILog.h"
#include "../Interfaces/IOperatingSystem.h"
#include <unistd.h>
#include <sys/stat.h>
#include <sys/inotify.h>
#include <dirent.h>
#include <android/asset_manager.h>
#include "../Interfaces/IMemory.h"

#define MAX_PATH PATH_MAX

class AAssetFileStream : public FileStream
{
	AAsset* pAsset;
public:

	AAssetFileStream(AAsset* asset, const Path* path) : 
		FileStream(FileStreamType_BundleAsset, path),
		pAsset(asset) {}

	size_t  Read(void* outputBuffer, size_t bufferSizeInBytes) override
	{
		return AAsset_read(pAsset, outputBuffer, bufferSizeInBytes);
	}

	size_t  Scan(const char* format, va_list args, int* bytesRead) override
	{
		LOGF(LogLevel::eWARNING, "fsScanFromStream is not implemented for bundled Android assets.");
		return 0;
	}

	size_t  Write(const void* sourceBuffer, size_t byteCount) override
	{
		LOGF(LogLevel::eERROR, "Bundled Android assets are not writable.");
		return 0;
	}

	size_t  Print(const char* format, va_list args) override
	{
		LOGF(LogLevel::eERROR, "Bundled Android assets are not writable.");
		return 0;
	}

	bool    Seek(SeekBaseOffset baseOffset, ssize_t seekOffset) override
	{
		int origin = SEEK_SET;
		switch (baseOffset)
		{
			case SBO_START_OF_FILE: origin = SEEK_SET; break;
			case SBO_CURRENT_POSITION: origin = SEEK_CUR; break;
			case SBO_END_OF_FILE: origin = SEEK_END; break;
		}
		return AAsset_seek64(pAsset, seekOffset, origin) != -1;
	}

	ssize_t GetSeekPosition() const override {
		return (ssize_t)AAsset_seek64(pAsset, 0, SEEK_CUR);
	}

	ssize_t GetFileSize() const override
	{
		return (ssize_t)AAsset_getLength64(pAsset);
	}

	void* GetUnderlyingBuffer() const override
	{
		return NULL;
	}

	void    Flush() override {}

	bool    IsAtEnd() const override
	{
		return AAsset_getRemainingLength64(pAsset) == 0;
	}

	bool    Close() override
	{
		AAsset_close(pAsset);

		conf_delete(this);
		return true;
	}
};

class AndroidBundleFileSystem: public FileSystem
{
	AAssetManager* pAssetManager;

public:
	AndroidBundleFileSystem() :
		FileSystem(FSK_RESOURCE_BUNDLE),
		pAssetManager(NULL) {}

	AndroidBundleFileSystem(AAssetManager* assetManager): 
		FileSystem(FSK_RESOURCE_BUNDLE), 
		pAssetManager(assetManager) 
	{
		Path* rootPath = fsCreatePath(this, ""); 
		fsSetResourceDirectoryRootPath(rootPath);
		fsFreePath(rootPath);
	}

	bool IsReadOnly() const override { return true; }

	bool IsCaseSensitive() const override { return true; }

	char GetPathDirectorySeparator() const override { return '/'; }

	size_t GetRootPathLength() const override
	{
		return 0;
	}

	/// Fills path's buffer with the canonical root path corresponding to the root of absolutePathString,
	/// and returns an offset into absolutePathString containing the path component after the root by pathComponentOffset.
	/// path is assumed to have storage for up to 16 characters.
	bool FormRootPath(const char* absolutePathString, Path* path, size_t* pathComponentOffset) const override
	{
		if (absolutePathString[0] == '/')
		{
			return false;
		}

		(&path->mPathBufferOffset)[7] = 0;
		path->mPathBufferOffset = 0;
		path->mPathLength = 0;

		*pathComponentOffset = 0;

		return true;
	}

	time_t GetCreationTime(const Path* filePath) const override
	{
		LOGF(LogLevel::eWARNING, "GetCreationTime is unsupported on Android bundle paths.");
		return 0;
	}

	time_t GetLastAccessedTime(const Path* filePath) const override
	{
		LOGF(LogLevel::eWARNING, "GetLastAccessedTime is unsupported on Android bundle paths.");
		return 0;
	}

	time_t GetLastModifiedTime(const Path* filePath) const override
	{
		LOGF(LogLevel::eWARNING, "GetLastModifiedTime is unsupported on Android bundle paths.");
		return 0;
	}

	bool CreateDirectory(const Path* directoryPath) const override
	{
		LOGF(LogLevel::eWARNING, "The Android bundle is read-only");
		return false;
	}

	bool DeleteFile(const Path* path) const override
	{
		LOGF(LogLevel::eWARNING, "The Android bundle is read-only");
		return false;
	}

	bool FileExists(const Path* path) const override
	{
		if (IsDirectory(path))
			return true;

		if (AAsset* asset = AAssetManager_open(pAssetManager, fsGetPathAsNativeString(path), AASSET_MODE_UNKNOWN))
		{
			AAsset_close(asset);
			return true;
		}

		return false;
	}

	bool IsDirectory(const Path* path) const override
	{
		// https://stackoverflow.com/questions/26101371/checking-if-directory-folder-exists-in-apk-via-native-code-only
		// AAsetManager_openDir will always return a pointer to initialized object,
		// even if the specified directory doesn't exist. In other words, checking if assetDir==NULL is pointless.
		// Trick is to check if AAssetDir_getNextFileName will return a non-null const char *.
		// If it's NULL - there is no folder, else - there is one.
		AAssetDir* subDir = AAssetManager_openDir(pAssetManager, fsGetPathAsNativeString(path));
		bool exists = AAssetDir_getNextFileName(subDir) != NULL;
		AAssetDir_close(subDir);
		return exists;
	}

	FileStream* OpenFile(const Path* filePath, FileMode mode) const override
	{
		if ((mode & (FM_WRITE | FM_APPEND)) != 0)
		{
			LOGF(LogLevel::eERROR, "Cannot open %s with mode %i: the Android bundle is read-only.",
				fsGetPathAsNativeString(filePath), mode);
			return NULL;
		}

		AAsset* file = AAssetManager_open(pAssetManager,
			fsGetPathAsNativeString(filePath), AASSET_MODE_BUFFER);

		if (file)
		{
			return conf_new(AAssetFileStream, file, filePath);
		}
		return NULL;
	}

	bool CopyFile(const Path* sourcePath, const Path* destinationPath, bool overwriteIfExists) const override
	{
		LOGF(LogLevel::eWARNING, "The Android bundle is read-only");
		return false;
	}

	void EnumerateFilesWithExtension(
		const Path* directory, const char* extension, bool (*processFile)(const Path*, void* userData), void* userData) const override
	{
		if (extension[0] == '.')
		{
			extension += 1;
		}

		size_t extensionLen = strlen(extension);

		AAssetDir* assetDir = AAssetManager_openDir(pAssetManager, fsGetPathAsNativeString(directory));
		while (const char* fileName = AAssetDir_getNextFileName(assetDir)) {
			const char* p = strcasestr(fileName, extension);
			if (p)
			{
				Path* path = fsAppendPathComponent(directory, fileName);
				processFile(path, userData);
				fsFreePath(path);
			}
		}
		AAssetDir_close(assetDir);
	}

	void
		EnumerateSubDirectories(const Path* directory, bool (*processDirectory)(const Path*, void* userData), void* userData) const override
	{
		AAssetDir* assetDir = AAssetManager_openDir(pAssetManager, fsGetPathAsNativeString(directory));
		while (const char* fileName = AAssetDir_getNextFileName(assetDir)) {
			if (AAssetDir* subDir = AAssetManager_openDir(pAssetManager, fileName))
			{
				Path* path = fsCreatePath(this, fileName);
				processDirectory(path, userData);
				fsFreePath(path);
				AAssetDir_close(subDir);
			}
		}
		AAssetDir_close(assetDir);
	}
};


struct AndroidDiskFileSystem : public UnixFileSystem
{
	AndroidDiskFileSystem() : UnixFileSystem() {}

	bool IsCaseSensitive() const override
	{
		return true;
	}

	bool CopyFile(const Path* sourcePath, const Path* destinationPath, bool overwriteIfExists) const override
	{
		FileStream* sourceFile = fsOpenFile(sourcePath, FM_READ_BINARY);
		if (!sourceFile) { return false; }

		if (!overwriteIfExists && fsFileExists(destinationPath)) { return false; }

		FileStream* destinationFile = fsOpenFile(destinationPath, FM_WRITE_BINARY);
		if (!destinationFile) { return false; }

		while (!fsStreamAtEnd(sourceFile))
		{
			uint8_t byte;
			if (fsReadFromStream(sourceFile, &byte, sizeof(uint8_t)) != sizeof(uint8_t))
			{
				fsCloseStream(sourceFile);
				fsCloseStream(destinationFile);
				return false;
			}

			if (fsWriteToStream(destinationFile, &byte, sizeof(uint8_t)) != sizeof(uint8_t))
			{
				fsCloseStream(sourceFile);
				fsCloseStream(destinationFile);
				return false;
			}
		}

		fsCloseStream(sourceFile);
		fsCloseStream(destinationFile);

		return true;
	}

	void EnumerateFilesWithExtension(
		const Path* directoryPath, const char* extension, bool (*processFile)(const Path*, void* userData), void* userData) const override
	{
		DIR* directory = opendir(fsGetPathAsNativeString(directoryPath));
		if (!directory)
			return;

		struct dirent* entry;
		do
		{
			entry = readdir(directory);
			if (!entry)
				break;

			Path* path = fsAppendPathComponent(directoryPath, entry->d_name);
			PathComponent fileExt = fsGetPathExtension(path);

			if (!extension)
				processFile(path, userData);

			else if (extension[0] == 0 && fileExt.length == 0)
				processFile(path, userData);

			else if (fileExt.length > 0 && strncasecmp(fileExt.buffer, extension, fileExt.length) == 0)
			{
				processFile(path, userData);
			}

			fsFreePath(path);

		} while (entry != NULL);

		closedir(directory);
	}

	void EnumerateSubDirectories(
		const Path* directoryPath, bool (*processDirectory)(const Path*, void* userData), void* userData) const override
	{
		DIR* directory = opendir(fsGetPathAsNativeString(directoryPath));
		if (!directory)
			return;

		struct dirent* entry;
		do
		{
			entry = readdir(directory);
			if (!entry)
				break;

			if ((entry->d_type & DT_DIR) && (entry->d_name[0] != '.'))
			{
				Path* path = fsAppendPathComponent(directoryPath, entry->d_name);
				processDirectory(path, userData);
				fsFreePath(path);
			}
		} while (entry != NULL);

		closedir(directory);
	}
};

static ANativeActivity* pNativeActivity = NULL;

AndroidBundleFileSystem gBundleFileSystem;
AndroidDiskFileSystem gSystemFileSystem;

FileSystem* fsGetSystemFileSystem() { return &gSystemFileSystem; }

void AndroidFS_SetNativeActivity(ANativeActivity* nativeActivity)
{
	ASSERT(nativeActivity);
	pNativeActivity = nativeActivity;
	gBundleFileSystem = AndroidBundleFileSystem(nativeActivity->assetManager);
}

Path* fsCopyWorkingDirectoryPath()
{
	char cwd[MAX_PATH];
	getcwd(cwd, MAX_PATH);
	Path* path = fsCreatePath(fsGetSystemFileSystem(), cwd);
	return path;
}

Path* fsCopyExecutablePath()
{
	char exeName[MAX_PATH];
	exeName[0] = 0;
	ssize_t count = readlink("/proc/self/exe", exeName, MAX_PATH);
	exeName[count] = '\0';
	return fsCreatePath(fsGetSystemFileSystem(), exeName);
}

Path* fsCopyProgramDirectoryPath()
{
	return fsCreatePath(&gBundleFileSystem, "");
}

Path* fsCopyPreferencesDirectoryPath(const char* organisation, const char* application)
{
	ASSERT(pNativeActivity);
	return fsCreatePath(fsGetSystemFileSystem(), pNativeActivity->internalDataPath);
}

Path* fsCopyUserDocumentsDirectoryPath()
{
	ASSERT(pNativeActivity);
	return fsCreatePath(fsGetSystemFileSystem(), pNativeActivity->externalDataPath);
}

Path* fsCopyLogFileDirectoryPath() 
{
	return fsCreatePath(fsGetSystemFileSystem(), pNativeActivity->externalDataPath);
}

void fsShowOpenFileDialog(
	const char* title, const Path* directory, FileDialogCallbackFn callback, void* userData, const char* fileDesc,
	const char** fileExtensions, size_t fileExtensionCount)
{
	LOGF(LogLevel::eERROR, "fsShowOpenFileDialog is not implemented on Android");
}

void fsShowSaveFileDialog(
	const char* title, const Path* directory, FileDialogCallbackFn callback, void* userData, const char* fileDesc,
	const char** fileExtensions, size_t fileExtensionCount)
{
	LOGF(LogLevel::eERROR, "fsShowSaveFileDialog is not implemented on Android");
}

FileWatcher* fsCreateFileWatcher(const Path* path, FileWatcherEventMask eventMask, FileWatcherCallback callback)
{
	LOGF(LogLevel::eERROR, "FileWatcher is unsupported on Android.");
	return NULL;
}

void fsFreeFileWatcher(FileWatcher* fileWatcher) { LOGF(LogLevel::eERROR, "FileWatcher is unsupported on Android."); }

#endif
