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

#ifdef __linux__

#include "../FileSystem/UnixFileSystem.h"
#include "../Interfaces/ILog.h"
#include "../Interfaces/IOperatingSystem.h"
#include "../Interfaces/IThread.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <unistd.h>
#include <pwd.h>
#include <fcntl.h>           //for open and O_* enums
#include <linux/limits.h>    //PATH_MAX declaration
#include <dirent.h>

#include "../Interfaces/IMemory.h"

#define MAX_PATH PATH_MAX

struct LinuxFileSystem: public UnixFileSystem
{
	LinuxFileSystem(): UnixFileSystem() {}
    
    bool IsCaseSensitive() const override
    {
        return true;
    }

	bool CopyFile(const Path* sourcePath, const Path* destinationPath, bool overwriteIfExists) const override
	{
		int         source = open(fsGetPathAsNativeString(sourcePath), O_RDONLY, 0);
		int         dest = open(fsGetPathAsNativeString(destinationPath), O_WRONLY);
		struct stat stat_source;
		fstat(source, &stat_source);
		bool ret = sendfile64(dest, source, 0, stat_source.st_size) != -1;
		close(source);
		close(dest);
		return ret;
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

			Path*         path = fsAppendPathComponent(directoryPath, entry->d_name);
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

LinuxFileSystem gDefaultFS;

FileSystem* fsGetSystemFileSystem() { return &gDefaultFS; }

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
	Path* exePath = fsCopyExecutablePath();
	Path* directory = fsCopyParentPath(exePath);
	fsFreePath(exePath);
	return directory;
}

Path* fsCopyPreferencesDirectoryPath(const char* organisation, const char* application)
{
	const char* homedir;
	if ((homedir = getenv("HOME")) == NULL)
	{
		homedir = getpwuid(getuid())->pw_dir;
	}

	Path* homePath = fsCreatePath(fsGetSystemFileSystem(), homedir);
	return homePath;
}

Path* fsCopyUserDocumentsDirectoryPath()
{
	const char* homedir;
	if ((homedir = getenv("HOME")) == NULL)
	{
		homedir = getpwuid(getuid())->pw_dir;
	}

	Path* homePath = fsCreatePath(fsGetSystemFileSystem(), homedir);
	Path* documents = fsAppendPathComponent(homePath, "Documents");
	fsFreePath(homePath);
	return documents;
}

Path* fsCopyLogFileDirectoryPath() { return fsCopyProgramDirectoryPath(); }

#pragma mark - FileManager Dialogs

void fsShowOpenFileDialog(
	const char* title, const Path* directory, FileDialogCallbackFn callback, void* userData, const char* fileDesc,
	const char** fileExtensions, size_t fileExtensionCount)
{
	LOGF(LogLevel::eERROR, "Not implemented");
}

void fsShowSaveFileDialog(
	const char* title, const Path* directory, FileDialogCallbackFn callback, void* userData, const char* fileDesc,
	const char** fileExtensions, size_t fileExtensionCount)
{
	LOGF(LogLevel::eERROR, "Not implemented");
}

#include <sys/inotify.h>

struct FileWatcher
{
	Path*               mWatchDir;
	uint32_t            mNotifyFilter;
	FileWatcherCallback mCallback;
	ThreadDesc          mThreadDesc;
	ThreadHandle        mThread;
	volatile int        mRun;
};

static void fswThreadFunc(void* data)
{
	FileWatcher* fs = (FileWatcher*)data;

	int  fd, wd;
	char buffer[4096];

	fd = inotify_init();
	if (fd < 0)
	{
		return;
	}

	wd = inotify_add_watch(fd, fsGetPathAsNativeString(fs->mWatchDir), fs->mNotifyFilter);

	if (wd < 0)
	{
		close(fd);
		return;
	}

	fd_set         rfds;
	struct timeval tv = { 0, 128 << 10 };

	while (fs->mRun)
	{
		FD_ZERO(&rfds);
		FD_SET(fd, &rfds);
		int retval = select(FD_SETSIZE, &rfds, 0, 0, &tv);
		if (retval < 0)
		{
			break;
		}
		if (retval == 0)
		{
			continue;
		}

		int length = read(fd, buffer, sizeof(buffer));
		if (length < 0)
		{
			break;
		}

		size_t offset = 0;
		while (offset < length)
		{
			struct inotify_event* event = (struct inotify_event*)(buffer + offset);
			if (event->len)
			{
				Path* path = fsAppendPathComponent(fs->mWatchDir, event->name);
				if (event->mask & IN_MODIFY)
				{
					fs->mCallback(path, FWE_MODIFIED);
				}
				if (event->mask & (IN_ACCESS | IN_OPEN))
				{
					fs->mCallback(path, FWE_ACCESSED);
				}
				if (event->mask & (IN_MOVED_TO | IN_CREATE))
				{
					fs->mCallback(path, FWE_CREATED);
				}
				if (event->mask & (IN_MOVED_FROM | IN_DELETE))
				{
					fs->mCallback(path, FWE_DELETED);
				}
				fsFreePath(path);
			}
			offset += sizeof(struct inotify_event) + event->len;
		}
	}
	inotify_rm_watch(fd, wd);

	close(fd);
};

FileWatcher* fsCreateFileWatcher(const Path* path, FileWatcherEventMask eventMask, FileWatcherCallback callback)
{
	FileWatcher* watcher = conf_new(FileWatcher);
	watcher->mWatchDir = fsCopyPath(path);

	uint32_t notifyFilter = 0;

	if (eventMask & FWE_MODIFIED)
	{
		notifyFilter |= IN_MODIFY;
	}
	if (eventMask & FWE_ACCESSED)
	{
		notifyFilter |= IN_ACCESS | IN_OPEN;
	}
	if (eventMask & FWE_CREATED)
	{
		notifyFilter |= IN_CREATE | IN_MOVED_TO;
	}
	if (eventMask & FWE_DELETED)
	{
		notifyFilter |= IN_DELETE | IN_MOVED_FROM;
	}

	watcher->mNotifyFilter = notifyFilter;
	watcher->mCallback = callback;
	watcher->mRun = 1;

	watcher->mThreadDesc.pFunc = fswThreadFunc;
	watcher->mThreadDesc.pData = watcher;

	watcher->mThread = create_thread(&watcher->mThreadDesc);
}

void fsFreeFileWatcher(FileWatcher* fileWatcher)
{
	fileWatcher->mRun = 0;
	fsFreePath(fileWatcher->mWatchDir);
	destroy_thread(fileWatcher->mThread);
	conf_delete(fileWatcher);
}

#endif
