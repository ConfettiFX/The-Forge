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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <unistd.h>
#include <pwd.h>
#include <fcntl.h>           //for open and O_* enums
#include <dirent.h>

#include <sys/inotify.h>

#include "IToolFileSystem.h"

#include "../../OS/Interfaces/IOperatingSystem.h"
#include "../../OS/Interfaces/IThread.h"
#include "../../OS/Interfaces/ILog.h"
#include "../../OS/Interfaces/IMemory.h"

struct FileWatcher
{
	char                mPath[FS_MAX_PATH];
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

	char filePath[FS_MAX_PATH] = {};
	fsAppendPathComponent("", fs->mPath, filePath);
	wd = inotify_add_watch(fd, filePath, fs->mNotifyFilter);

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
				if (event->mask & IN_MODIFY)
				{
					fs->mCallback(event->name, FWE_MODIFIED);
				}
				if (event->mask & (IN_ACCESS | IN_OPEN))
				{
					fs->mCallback(event->name, FWE_ACCESSED);
				}
				if (event->mask & (IN_MOVED_TO | IN_CREATE))
				{
					fs->mCallback(event->name, FWE_CREATED);
				}
				if (event->mask & (IN_MOVED_FROM | IN_DELETE))
				{
					fs->mCallback( event->name, FWE_DELETED);
				}
			}
			offset += sizeof(struct inotify_event) + event->len;
		}
	}
	inotify_rm_watch(fd, wd);

	close(fd);
};

FileWatcher* fsCreateFileWatcher(const char* path, FileWatcherEventMask eventMask, FileWatcherCallback callback)
{
	FileWatcher* watcher = (FileWatcher*)tf_calloc(1, sizeof(FileWatcher));
	strcpy(watcher->mPath, path);

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

    return watcher;
}

void fsFreeFileWatcher(FileWatcher* fileWatcher)
{
	fileWatcher->mRun = 0;
	destroy_thread(fileWatcher->mThread);
	tf_free(fileWatcher);
}

void fsGetFilesWithExtension(ResourceDirectory resourceDir, const char* subDirectory, const char* extension, eastl::vector<eastl::string>& out)
{
	char directoryPath[FS_MAX_PATH] = {};
	fsAppendPathComponent(fsGetResourceDirectory(resourceDir), subDirectory, directoryPath);

	DIR* directory = opendir(directoryPath);
	if (!directory)
		return;

	if(extension[0] == '*')
	{
		++extension;
	}
	if(extension[0] == '.')
	{
		++extension;
	}

	struct dirent* entry;
	do
	{
		entry = readdir(directory);
		if (!entry)
			break;

		char fileExt[FS_MAX_PATH] = {};
		fsGetPathExtension(entry->d_name, fileExt);
		size_t fileExtLen = strlen(fileExt);

		if ((!extension) ||
			(extension[0] == 0 && fileExtLen == 0) ||
			(fileExtLen > 0 && strncasecmp(fileExt, extension, fileExtLen) == 0))
		{
			char result[FS_MAX_PATH] = {};
			fsAppendPathComponent(subDirectory, entry->d_name, result);
			out.push_back(result);
		}
	} while (entry != NULL);

	closedir(directory);
}

void fsGetSubDirectories(ResourceDirectory resourceDir, const char* subDirectory, eastl::vector<eastl::string>& out)
{
	char directoryPath[FS_MAX_PATH] = {};
	fsAppendPathComponent(fsGetResourceDirectory(resourceDir), subDirectory, directoryPath);

	DIR* directory = opendir(directoryPath);
	if (!directory)
	{
		return;
	}

	struct dirent* entry;
	do
	{
		entry = readdir(directory);
		if (!entry)
			break;

		if ((entry->d_type & DT_DIR) && (entry->d_name[0] != '.'))
		{
			char result[FS_MAX_PATH] = {};
			fsAppendPathComponent(subDirectory, entry->d_name, result);
			out.push_back(result);
		}
	} while (entry != NULL);

	closedir(directory);
}
