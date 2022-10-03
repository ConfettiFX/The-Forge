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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <unistd.h>
#include <pwd.h>
#include <fcntl.h>           //for open and O_* enums
#include <dirent.h>

#include <sys/inotify.h>

#include "../../Utilities/Interfaces/IToolFileSystem.h"

#include "../../OS/Interfaces/IOperatingSystem.h"
#include "../../Utilities/Interfaces/IThread.h"
#include "../../Utilities/Interfaces/ILog.h"
#include "../../Utilities/Interfaces/IMemory.h"

struct FileWatcher
{
	char                mPath[FS_MAX_PATH];
	uint32_t            mNotifyFilter;
	FileWatcherCallback mCallback;
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

		ssize_t offset = 0;
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

	ThreadDesc threadDesc = {};
	threadDesc.pFunc = fswThreadFunc;
	threadDesc.pData = watcher;
	strncpy(threadDesc.mThreadName, "FileWatcher", sizeof(threadDesc.mThreadName));
	initThread(&threadDesc, &watcher->mThread);

    return watcher;
}

void fsFreeFileWatcher(FileWatcher* fileWatcher)
{
	fileWatcher->mRun = 0;
	joinThread(fileWatcher->mThread);
	tf_free(fileWatcher);
}

void fsGetFilesWithExtension(ResourceDirectory resourceDir, const char* subDirectory, const char* extension, char*** out, int* count)
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

    int filesFound = 0;
	struct dirent* entry;
	do
	{
		entry = readdir(directory);
		if (!entry)
			break;

		char entryPath[FS_MAX_PATH] = {};
		fsAppendPathComponent(directoryPath, entry->d_name, entryPath);
		
		struct stat st = {};
		stat(entryPath, &st);
		if(S_ISDIR(st.st_mode))
			continue;

		char fileExt[FS_MAX_PATH] = {};
		fsGetPathExtension(entry->d_name, fileExt);
		size_t fileExtLen = strlen(fileExt);

		if ((!extension) ||
			(extension[0] == 0 && fileExtLen == 0) ||
			(fileExtLen > 0 && strncasecmp(fileExt, extension, fileExtLen) == 0))
		{
            filesFound += 1;
		}
	} while (entry != NULL);
	closedir(directory);

    *out = NULL;
    *count = 0;

    directory = opendir(directoryPath);
    if (!directory)
        return;

    if (filesFound > 0)
    {
        char** stringList = (char**)tf_malloc(filesFound * sizeof(char*) + filesFound * sizeof(char) * FS_MAX_PATH);
        char* firstString = ((char*)stringList + filesFound * sizeof(char*));
        for (int i = 0; i < filesFound; ++i)
        {
            stringList[i] = firstString + (sizeof(char) * FS_MAX_PATH * i);
        }
        *out = stringList;
        *count = filesFound;
    }

    int strIndex = 0;
    do
    {
        entry = readdir(directory);
        if (!entry)
            break;

		char entryPath[FS_MAX_PATH] = {};
		fsAppendPathComponent(directoryPath, entry->d_name, entryPath);
		
		struct stat st = {};
		stat(entryPath, &st);
		if(S_ISDIR(st.st_mode))
			continue;

        char fileExt[FS_MAX_PATH] = {};
        fsGetPathExtension(entry->d_name, fileExt);
        size_t fileExtLen = strlen(fileExt);

        if ((!extension) ||
            (extension[0] == 0 && fileExtLen == 0) ||
            (fileExtLen > 0 && strncasecmp(fileExt, extension, fileExtLen) == 0))
        {
            char result[FS_MAX_PATH] = {};
            fsAppendPathComponent(subDirectory, entry->d_name, result);
            char * dest = (*out)[strIndex++];
            strcpy(dest, result);
        }
    } while (entry != NULL);
    closedir(directory);
}

void fsGetSubDirectories(ResourceDirectory resourceDir, const char* subDirectory, char*** out, int* count)
{
	char directoryPath[FS_MAX_PATH] = {};
	fsAppendPathComponent(fsGetResourceDirectory(resourceDir), subDirectory, directoryPath);

	DIR* directory = opendir(directoryPath);
	if (!directory)
	{
		return;
	}

    int filesFound = 0;
	struct dirent* entry;
	do
	{
		entry = readdir(directory);
		if (!entry)
			break;

		if ((entry->d_type & DT_DIR) && (entry->d_name[0] != '.'))
		{
            filesFound += 1;
		}
	} while (entry != NULL);
	closedir(directory);

    *out = NULL;
    *count = 0;

    directory = opendir(directoryPath);
    if (!directory)
        return;

    if (filesFound > 0)
    {
        char** stringList = (char**)tf_malloc(filesFound * sizeof(char*) + filesFound * sizeof(char) * FS_MAX_PATH);
        char* firstString = ((char*)stringList + filesFound * sizeof(char*));
        for (int i = 0; i < filesFound; ++i)
        {
            stringList[i] = firstString + (sizeof(char) * FS_MAX_PATH * i);
        }
        *out = stringList;
        *count = filesFound;
    }

    int strIndex = 0;
    do
    {
        entry = readdir(directory);
        if (!entry)
            break;

        if ((entry->d_type & DT_DIR) && (entry->d_name[0] != '.'))
        {
            char result[FS_MAX_PATH] = {};
            fsAppendPathComponent(subDirectory, entry->d_name, result);
            char * dest = (*out)[strIndex++];
            strcpy(dest, result);
        }
    } while (entry != NULL);
    closedir(directory);
}

bool fsRemoveFile(const ResourceDirectory resourceDir, const char* fileName)
{
	const char* resourcePath = fsGetResourceDirectory(resourceDir);
	char filePath[FS_MAX_PATH] = {};
	fsAppendPathComponent(resourcePath, fileName, filePath);

	return !remove(filePath);
}

bool fsRenameFile(const ResourceDirectory resourceDir, const char* fileName, const char* newFileName)
{
	const char* resourcePath = fsGetResourceDirectory(resourceDir);

	char filePath[FS_MAX_PATH] = {};
	fsAppendPathComponent(resourcePath, fileName, filePath);

	char newfilePath[FS_MAX_PATH] = {};
	fsAppendPathComponent(resourcePath, newFileName, newfilePath);

	return !rename(filePath, newfilePath);
}

bool fsCopyFile(const ResourceDirectory sourceResourceDir, const char* sourceFileName, const ResourceDirectory destResourceDir, const char* destFileName)
{
	const char* sourceResourcePath = fsGetResourceDirectory(sourceResourceDir);
	const char* destResourcePath = fsGetResourceDirectory(destResourceDir);

	char sourceFilePath[FS_MAX_PATH] = {};
	fsAppendPathComponent(sourceResourcePath, sourceFileName, sourceFilePath);

	char destFilePath[FS_MAX_PATH] = {};
	fsAppendPathComponent(destResourcePath, destFileName, destFilePath);

	int input, output;
	if ((input = open(sourceFilePath, O_RDONLY)) == -1)
	{
		return -1;
	}
	if ((output = creat(destFilePath, 0660)) == -1)
	{
		close(input);
		return -1;
	}

	off_t bytesCopied = 0;
	struct stat fileinfo = { 0 };
	fstat(input, &fileinfo);
	int result = sendfile(output, input, &bytesCopied, fileinfo.st_size);
	close(input);
	close(output);
	return result;
}

bool fsFileExist(const ResourceDirectory resourceDir, const char* fileName)
{
	const char* resourcePath = fsGetResourceDirectory(resourceDir);
	char filePath[FS_MAX_PATH] = {};
	fsAppendPathComponent(resourcePath, fileName, filePath);

	struct stat s = {};
	int res = stat(filePath, &s);
	if (res != 0)
		return false;
	return true;
}
