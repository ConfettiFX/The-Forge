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

#include <AppKit/AppKit.h>

#include "../../Utilities/Interfaces/ILog.h"
#include "../../Utilities/Interfaces/IToolFileSystem.h"

#include "../../Utilities/Interfaces/IMemory.h"

#pragma mark - FileWatcher

struct FileWatcher
{
    FileWatcherCallback mCallback;
    void*               mCallbackUserData;
    uint32_t            mEventMask;
    FSEventStreamRef    mStream;
};

static void fswCbFunc(ConstFSEventStreamRef streamRef, void* data, size_t numEvents, void* eventPaths,
                      const FSEventStreamEventFlags eventFlags[], const FSEventStreamEventId eventIds[])
{
    FileWatcher* fswData = (FileWatcher*)data;
    const char** paths = (const char**)eventPaths;

    for (size_t i = 0; i < numEvents; ++i)
    {
        if ((fswData->mEventMask & FWE_MODIFIED) &&
            ((eventFlags[i] & kFSEventStreamEventFlagItemModified) || (eventFlags[i] & kFSEventStreamEventFlagItemInodeMetaMod)))
        {
            fswData->mCallback(paths[i], FWE_MODIFIED, fswData->mCallbackUserData);
        }
        if ((fswData->mEventMask & FWE_ACCESSED) && (eventFlags[i] & kFSEventStreamEventFlagItemInodeMetaMod))
        {
            fswData->mCallback(paths[i], FWE_ACCESSED, fswData->mCallbackUserData);
        }
        if ((fswData->mEventMask & FWE_CREATED) && (eventFlags[i] & kFSEventStreamEventFlagItemCreated))
        {
            fswData->mCallback(paths[i], FWE_CREATED, fswData->mCallbackUserData);
        }
        if ((fswData->mEventMask & FWE_DELETED) && (eventFlags[i] & kFSEventStreamEventFlagItemRemoved))
        {
            fswData->mCallback(paths[i], FWE_DELETED, fswData->mCallbackUserData);
        }
    }
};

FileWatcher* fsCreateFileWatcher(const char* path, FileWatcherEventMask eventMask, FileWatcherCallback callback, void* callbackUserData)
{
    FileWatcher* watcher = (FileWatcher*)tf_calloc(1, sizeof(FileWatcher));
    watcher->mEventMask = eventMask;
    watcher->mCallback = callback;
    watcher->mCallbackUserData = callbackUserData;
    CFStringRef          paths[] = { CFStringCreateWithCString(NULL, path, kCFStringEncodingUTF8) };
    CFArrayRef           pathsToWatch = CFArrayCreate(NULL, (const void**)paths, 1, &kCFTypeArrayCallBacks);
    CFAbsoluteTime       latency = 0.125; // in seconds
    FSEventStreamContext context = { 0, watcher, NULL, NULL, NULL };
    watcher->mStream = FSEventStreamCreate(NULL, fswCbFunc, &context, pathsToWatch, kFSEventStreamEventIdSinceNow, latency,
                                           kFSEventStreamCreateFlagFileEvents);
    CFRelease(pathsToWatch);
    CFRelease(paths[0]);
    if (watcher->mStream)
    {
        FSEventStreamScheduleWithRunLoop(watcher->mStream, CFRunLoopGetMain(), kCFRunLoopDefaultMode);
        if (!FSEventStreamStart(watcher->mStream))
        {
            FSEventStreamInvalidate(watcher->mStream);
            FSEventStreamRelease(watcher->mStream);
        }
    }

    return watcher;
}

void fsFreeFileWatcher(FileWatcher* fileWatcher)
{
    if (fileWatcher->mStream)
    {
        FSEventStreamStop(fileWatcher->mStream);
        FSEventStreamInvalidate(fileWatcher->mStream);
        FSEventStreamRelease(fileWatcher->mStream);
    }

    tf_free(fileWatcher);
}

bool fsRemoveFile(const ResourceDirectory resourceDir, const char* fileName)
{
    const char* resourcePath = fsGetResourceDirectory(resourceDir);
    char        filePath[FS_MAX_PATH] = {};
    fsAppendPathComponent(resourcePath, fileName, filePath);

    return !remove(filePath);
}

bool fsRenameFile(const ResourceDirectory resourceDir, const char* fileName, const char* newFileName)
{
    const char* resourcePath = fsGetResourceDirectory(resourceDir);
    char        filePath[FS_MAX_PATH] = {};
    fsAppendPathComponent(resourcePath, fileName, filePath);

    char newfilePath[FS_MAX_PATH] = {};
    fsAppendPathComponent(resourcePath, newFileName, newfilePath);

    return !rename(filePath, newfilePath);
}

bool fsCopyFile(const ResourceDirectory sourceResourceDir, const char* sourceFileName, const ResourceDirectory destResourceDir,
                const char* destFileName)
{
    const char* sourceResourcePath = fsGetResourceDirectory(sourceResourceDir);
    const char* destResourcePath = fsGetResourceDirectory(destResourceDir);

    char sourceFilePath[FS_MAX_PATH] = {};
    fsAppendPathComponent(sourceResourcePath, sourceFileName, sourceFilePath);

    char destFilePath[FS_MAX_PATH] = {};
    fsAppendPathComponent(destResourcePath, destFileName, destFilePath);

    NSString* nsSourceFileName = [NSString stringWithUTF8String:sourceFilePath];
    NSString* nsDestFileName = [NSString stringWithUTF8String:destFilePath];

    NSFileManager* fileManager = [NSFileManager defaultManager];
    if ([fileManager copyItemAtPath:nsSourceFileName toPath:nsDestFileName error:NULL])
    {
        return true;
    }

    return false;
}

bool fsFileExist(const ResourceDirectory resourceDir, const char* fileName)
{
    const char* resourcePath = fsGetResourceDirectory(resourceDir);
    char        filePath[FS_MAX_PATH] = {};
    fsAppendPathComponent(resourcePath, fileName, filePath);

    BOOL isDir;

    NSFileManager* fileManager = [NSFileManager defaultManager];
    NSString*      nsFilePath = [NSString stringWithUTF8String:filePath];
    if ([fileManager fileExistsAtPath:nsFilePath isDirectory:&isDir] && !isDir)
        return true;
    return false;
}

bool fsCreateDirectory(const ResourceDirectory resourceDir, const char* path, bool recursive)
{
    if (fsFileExist(resourceDir, path))
        return true;

    const char* resourcePath = fsGetResourceDirectory(resourceDir);
    char        directoryPath[FS_MAX_PATH] = {};
    fsAppendPathComponent(resourcePath, path, directoryPath);

    BOOL           isRecusive = recursive ? YES : NO;
    NSURL*         pathURL = [NSURL fileURLWithPath:[NSString stringWithUTF8String:directoryPath]];
    NSFileManager* fileManager = [NSFileManager defaultManager];
    if (![fileManager createDirectoryAtURL:pathURL withIntermediateDirectories:isRecusive attributes:nil error:nil])
    {
        LOGF(LogLevel::eERROR, "Unable to create directory {%s}!", directoryPath);
        return false;
    }

    return true;
}
