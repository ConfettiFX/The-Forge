/*
 * Copyright (c) 2017-2024 The Forge Interactive Inc.
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

#include <dirent.h>
#include <errno.h>
#include <fcntl.h> //for open and O_* enums
#include <pwd.h>
#include <sys/inotify.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "../../OS/Interfaces/IOperatingSystem.h"
#include "../../Utilities/Interfaces/ILog.h"
#include "../../Utilities/Interfaces/IThread.h"
#include "../../Utilities/Interfaces/IToolFileSystem.h"

#include "../../Utilities/Interfaces/IMemory.h"

struct FileWatcher
{
    char                mPath[FS_MAX_PATH];
    uint32_t            mNotifyFilter;
    FileWatcherCallback mCallback;
    void*               mCallbackUserData;
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

    char filePath[FS_MAX_PATH] = { 0 };
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
                    fs->mCallback(event->name, FWE_MODIFIED, fs->mCallbackUserData);
                }
                if (event->mask & (IN_ACCESS | IN_OPEN))
                {
                    fs->mCallback(event->name, FWE_ACCESSED, fs->mCallbackUserData);
                }
                if (event->mask & (IN_MOVED_TO | IN_CREATE))
                {
                    fs->mCallback(event->name, FWE_CREATED, fs->mCallbackUserData);
                }
                if (event->mask & (IN_MOVED_FROM | IN_DELETE))
                {
                    fs->mCallback(event->name, FWE_DELETED, fs->mCallbackUserData);
                }
            }
            offset += sizeof(struct inotify_event) + event->len;
        }
    }
    inotify_rm_watch(fd, wd);

    close(fd);
}

FileWatcher* fsCreateFileWatcher(const char* path, FileWatcherEventMask eventMask, FileWatcherCallback callback, void* callbackUserData)
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
    watcher->mCallbackUserData = callbackUserData;
    watcher->mRun = 1;

    ThreadDesc threadDesc = { 0 };

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

bool fsRemoveFile(const ResourceDirectory resourceDir, const char* fileName)
{
    const char* resourcePath = fsGetResourceDirectory(resourceDir);
    char        filePath[FS_MAX_PATH] = { 0 };
    fsAppendPathComponent(resourcePath, fileName, filePath);

    return !remove(filePath);
}

bool fsRenameFile(const ResourceDirectory resourceDir, const char* fileName, const char* newFileName)
{
    const char* resourcePath = fsGetResourceDirectory(resourceDir);

    char filePath[FS_MAX_PATH] = { 0 };
    fsAppendPathComponent(resourcePath, fileName, filePath);

    char newfilePath[FS_MAX_PATH] = { 0 };
    fsAppendPathComponent(resourcePath, newFileName, newfilePath);

    return !rename(filePath, newfilePath);
}

bool fsCopyFile(const ResourceDirectory sourceResourceDir, const char* sourceFileName, const ResourceDirectory destResourceDir,
                const char* destFileName)
{
    const char* sourceResourcePath = fsGetResourceDirectory(sourceResourceDir);
    const char* destResourcePath = fsGetResourceDirectory(destResourceDir);

    char sourceFilePath[FS_MAX_PATH] = { 0 };
    fsAppendPathComponent(sourceResourcePath, sourceFileName, sourceFilePath);

    char destFilePath[FS_MAX_PATH] = { 0 };
    fsAppendPathComponent(destResourcePath, destFileName, destFilePath);

    int input, output;
    if ((input = open(sourceFilePath, O_RDONLY)) == -1)
    {
        return false;
    }
    if ((output = creat(destFilePath, 0660)) == -1)
    {
        close(input);
        return false;
    }

    struct stat fileinfo = { 0 };
    if (fstat(input, &fileinfo) == -1)
    {
        close(input);
        close(output);
        return false;
    }

    bool sendfileUnavailable = false;

    off_t bytesCopied = 0;
    while (bytesCopied < fileinfo.st_size)
    {
        // syscall has a 2GB limit per call in both 32+64 bit environments
        const size_t remaining = fileinfo.st_size - bytesCopied;
        const size_t count = remaining < 0x7ffff000 ? remaining : 0x7ffff000;

        const ssize_t result = sendfile(output, input, &bytesCopied, count);

        // -1 with these arguments may suggest sendfile doesn't supporting copying between the fds
        // sendfile manual suggests defaulting to read/write functions
        if (result == -1 && (errno == EINVAL || errno == ENOSYS))
        {
            sendfileUnavailable = true;
            break;
        }

        if (result == -1)
        {
            LOGF(eERROR, "%d Error while sending file between descriptors: %d -> %d", errno, input, output);
            break;
        }
    }

    // manually copy source file to destination in fixed-size chunks
    // continues until source EOF is reached or any data fails to copy
    if (sendfileUnavailable)
    {
        const size_t count = 4096;
        void*        buf = tf_malloc(count);

        while (bytesCopied < fileinfo.st_size)
        {
            const ssize_t readBytes = read(input, buf, count);
            if (readBytes <= 0)
                break;

            const ssize_t written = write(output, buf, readBytes);
            if (written != readBytes)
                break;

            bytesCopied += written;
        }

        tf_free(buf);
    }

    close(input);
    close(output);

    return bytesCopied == fileinfo.st_size;
}

static bool file_exist(const char* filePath)
{
    struct stat s = { 0 };
    int         res = stat(filePath, &s);
    if (res != 0)
        return false;
    return true;
}

bool fsFileExist(const ResourceDirectory resourceDir, const char* fileName)
{
    const char* resourcePath = fsGetResourceDirectory(resourceDir);
    char        filePath[FS_MAX_PATH] = { 0 };
    fsAppendPathComponent(resourcePath, fileName, filePath);

    return file_exist(filePath);
}

static bool create_directories(const char* path, bool recursive)
{
    if (!path || !*path)
        return true;
    if (file_exist(path))
        return true;

    char directoryPath[FS_MAX_PATH] = { 0 };
    strcpy(directoryPath, path);

    if (recursive)
    {
        const char directorySeparator = '/';
        char*      loc = strrchr(directoryPath, directorySeparator);
        if (loc)
        {
            char parentPath[FS_MAX_PATH] = { 0 };
            strncpy(parentPath, directoryPath, loc - directoryPath);
            // Recurse parent directories
            if (!create_directories(parentPath, recursive))
                return false;
        }
    }

    if (mkdir(directoryPath, 0777) != 0)
    {
        LOGF(eERROR, "Unable to create directory {%s}!", directoryPath);
        return false;
    }

    return true;
}

bool fsCreateDirectory(const ResourceDirectory resourceDir, const char* path, bool recursive)
{
    const char* resourcePath = fsGetResourceDirectory(resourceDir);
    char        directoryPath[FS_MAX_PATH] = { 0 };
    fsAppendPathComponent(resourcePath, path, directoryPath);

    return create_directories(directoryPath, recursive);
}
