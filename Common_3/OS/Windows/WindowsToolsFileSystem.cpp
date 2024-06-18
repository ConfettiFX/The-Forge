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

#include "../../Application/Config.h"

#include <malloc.h> // alloca
#include <wchar.h>  // _wrename

#include "../../Utilities/Interfaces/ILog.h"
#include "../../Utilities/Interfaces/IThread.h"
#include "../../Utilities/Interfaces/IToolFileSystem.h"
#include "../Interfaces/IOperatingSystem.h"

#include "commdlg.h"
#include "shlobj.h"

#include "../../Utilities/Interfaces/IMemory.h"

// static
template<typename T>
static inline T withUTF16Path(const char* path, T (*function)(const wchar_t*))
{
    size_t   len = strlen(path);
    wchar_t* buffer = (wchar_t*)alloca((len + 1) * sizeof(wchar_t));

    size_t resultLength = MultiByteToWideChar(CP_UTF8, 0, path, (int)len, buffer, (int)len);
    buffer[resultLength] = 0;

    return function(buffer);
}

#if defined(_WINDOWS) || defined(__APPLE__) || defined(__linux__)
typedef struct FileWatcher
{
    char                 mPath[FS_MAX_PATH];
    FileWatcherEventMask mEventMask;
    DWORD                mNotifyFilter;
    FileWatcherCallback  mCallback;
    void*                mCallbackUserData;
    HANDLE               hExitEvt;
    ThreadHandle         mThread;
    volatile int         mRun;
} FileWatcher;

void fswThreadFunc(void* data)
{
    setCurrentThreadName("FileWatcher");
    FileWatcher* fs = (FileWatcher*)data;

    HANDLE hDir =
        withUTF16Path<HANDLE>(fs->mPath,
                              [](const wchar_t* pathStr)
                              {
                                  return CreateFileW(pathStr, FILE_LIST_DIRECTORY, FILE_SHARE_WRITE | FILE_SHARE_READ | FILE_SHARE_DELETE,
                                                     NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, NULL);
                              });
    HANDLE hEvt = ::CreateEvent(NULL, TRUE, FALSE, NULL);

    BYTE       notifyBuffer[1024];
    char       utf8Name[100];
    OVERLAPPED ovl = { 0 };
    ovl.hEvent = hEvt;

    while (fs->mRun)
    {
        DWORD dwBytesReturned = 0;
        ResetEvent(hEvt);
        if (ReadDirectoryChangesW(hDir, &notifyBuffer, sizeof(notifyBuffer), TRUE, fs->mNotifyFilter, NULL, &ovl, NULL) == 0)
        {
            break;
        }

        HANDLE pHandles[2] = { hEvt, fs->hExitEvt };
        WaitForMultipleObjects(2, pHandles, FALSE, INFINITE);

        if (!fs->mRun)
        {
            break;
        }

        GetOverlappedResult(hDir, &ovl, &dwBytesReturned, FALSE);

        BYTE* p = notifyBuffer;
        for (;;)
        {
            FILE_NOTIFY_INFORMATION* fni = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(p);
            uint32_t                 action = 0;
            bool                     ignoreAction = FALSE;

            switch (fni->Action)
            {
            case FILE_ACTION_ADDED:
            case FILE_ACTION_RENAMED_NEW_NAME:
                action = FWE_CREATED;
                ignoreAction = !(fs->mEventMask & FWE_CREATED);
                break;
            case FILE_ACTION_MODIFIED:
                if (fs->mNotifyFilter & FILE_NOTIFY_CHANGE_LAST_WRITE)
                    action = FWE_MODIFIED;
                if (fs->mNotifyFilter & FILE_NOTIFY_CHANGE_LAST_ACCESS)
                    action = FWE_ACCESSED;
                break;
            case FILE_ACTION_REMOVED:
            case FILE_ACTION_RENAMED_OLD_NAME:
                action = FWE_DELETED;
                ignoreAction = !(fs->mEventMask & FWE_DELETED);
                break;
            default:
                break;
            }

            if (!ignoreAction)
            {
                memset(utf8Name, 0, sizeof(utf8Name));
                int outputLength = WideCharToMultiByte(CP_UTF8, 0, fni->FileName, fni->FileNameLength / sizeof(WCHAR), utf8Name,
                                                       sizeof(utf8Name) - 1, NULL, NULL);
                if (outputLength == 0)
                {
                    continue;
                }

                char fullPathToFile[FS_MAX_PATH] = { 0 };
                strcat(fullPathToFile, fs->mPath);
                strcat(fullPathToFile, "\\");
                strcat(fullPathToFile, utf8Name);

                LOGF(LogLevel::eINFO, "Monitoring activity of file: %s -- Action: %u", fs->mPath, fni->Action);
                fs->mCallback(fullPathToFile, action, fs->mCallbackUserData);
            }

            if (!fni->NextEntryOffset)
                break;
            p += fni->NextEntryOffset;
        }
    }

    CloseHandle(hDir);
    CloseHandle(hEvt);
};

FileWatcher* fsCreateFileWatcher(const char* path, FileWatcherEventMask eventMask, FileWatcherCallback callback, void* callbackUserData)
{
    FileWatcher* watcher = (FileWatcher*)tf_calloc(1, sizeof(FileWatcher));
    // watcher->mWatchDir = fsCopyPath(path);
    strcpy(watcher->mPath, path);

    uint32_t notifyFilter = 0;
    if (eventMask & FWE_MODIFIED)
    {
        notifyFilter |= FILE_NOTIFY_CHANGE_LAST_WRITE;
    }
    if (eventMask & FWE_ACCESSED)
    {
        notifyFilter |= FILE_NOTIFY_CHANGE_LAST_ACCESS;
    }
    if (eventMask & (FWE_DELETED | FWE_CREATED))
    {
        notifyFilter |= FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME;
    }

    watcher->mEventMask = eventMask;
    watcher->mNotifyFilter = notifyFilter;
    watcher->hExitEvt = ::CreateEvent(NULL, TRUE, FALSE, NULL);
    watcher->mCallback = callback;
    watcher->mCallbackUserData = callbackUserData;
    watcher->mRun = TRUE;

    ThreadDesc threadDesc = {};
    threadDesc.pFunc = fswThreadFunc;
    threadDesc.pData = watcher;
    strncpy(threadDesc.mThreadName, "FileWatcher", sizeof(threadDesc.mThreadName));
    initThread(&threadDesc, &watcher->mThread);

    return watcher;
}

void fsFreeFileWatcher(FileWatcher* fileWatcher)
{
    fileWatcher->mRun = FALSE;
    SetEvent(fileWatcher->hExitEvt);
    joinThread(fileWatcher->mThread);
    CloseHandle(fileWatcher->hExitEvt);
    tf_free(fileWatcher);
}
#endif

bool fsRemoveFile(const ResourceDirectory resourceDir, const char* fileName)
{
    const char* resourcePath = fsGetResourceDirectory(resourceDir);
    char        filePath[FS_MAX_PATH] = {};
    fsAppendPathComponent(resourcePath, fileName, filePath);

    // Path utf-16 conversion
    size_t   filePathLen = strlen(filePath);
    wchar_t* pathStr = (wchar_t*)alloca((filePathLen + 1) * sizeof(wchar_t));
    size_t   pathStrLength = MultiByteToWideChar(CP_UTF8, 0, filePath, (int)filePathLen, pathStr, (int)filePathLen);
    pathStr[pathStrLength] = 0;

    return !_wremove(pathStr);
}

bool fsRenameFile(const ResourceDirectory resourceDir, const char* fileName, const char* newFileName)
{
    const char* resourcePath = fsGetResourceDirectory(resourceDir);
    wchar_t*    pathStr = NULL;
    wchar_t*    newpathStr = NULL;

    {
        char filePath[FS_MAX_PATH] = {};
        fsAppendPathComponent(resourcePath, fileName, filePath);

        // Path utf-16 conversion
        size_t filePathLen = strlen(filePath);
        pathStr = (wchar_t*)alloca((filePathLen + 1) * sizeof(wchar_t));
        size_t pathStrLength = MultiByteToWideChar(CP_UTF8, 0, filePath, (int)filePathLen, pathStr, (int)filePathLen);
        pathStr[pathStrLength] = 0;
    }

    {
        char newfilePath[FS_MAX_PATH] = {};
        fsAppendPathComponent(resourcePath, newFileName, newfilePath);

        // New path utf-16 conversion
        size_t newfilePathLen = strlen(newfilePath);
        newpathStr = (wchar_t*)alloca((newfilePathLen + 1) * sizeof(wchar_t));
        size_t newpathStrLength = MultiByteToWideChar(CP_UTF8, 0, newfilePath, (int)newfilePathLen, newpathStr, (int)newfilePathLen);
        newpathStr[newpathStrLength] = 0;
    }

    return !_wrename(pathStr, newpathStr);
}

bool fsCopyFile(const ResourceDirectory sourceResourceDir, const char* sourceFileName, const ResourceDirectory destResourceDir,
                const char* destFileName)
{
    const char* sourceResourcePath = fsGetResourceDirectory(sourceResourceDir);
    const char* destResourcePath = fsGetResourceDirectory(destResourceDir);

    char sourceFilePath[FS_MAX_PATH] = {};
    fsAppendPathComponent(sourceResourcePath, sourceFileName, sourceFilePath);
    // Path utf-16 conversion
    size_t   sourcefilePathLen = strlen(sourceFilePath);
    wchar_t* sourcePathStr = (wchar_t*)alloca((sourcefilePathLen + 1) * sizeof(wchar_t));
    size_t   sourcepathStrLength =
        MultiByteToWideChar(CP_UTF8, 0, sourceFilePath, (int)sourcefilePathLen, sourcePathStr, (int)sourcefilePathLen);
    sourcePathStr[sourcepathStrLength] = 0;

    char destFilePath[FS_MAX_PATH] = {};
    fsAppendPathComponent(destResourcePath, destFileName, destFilePath);
    // Path utf-16 conversion
    size_t   destfilePathLen = strlen(destFilePath);
    wchar_t* destPathStr = (wchar_t*)alloca((destfilePathLen + 1) * sizeof(wchar_t));
    size_t   destpathStrLength = MultiByteToWideChar(CP_UTF8, 0, destFilePath, (int)destfilePathLen, destPathStr, (int)destfilePathLen);
    destPathStr[destpathStrLength] = 0;

    return CopyFileW(sourcePathStr, destPathStr, FALSE);
}

bool file_exist(const char* filePath)
{
    // Path utf-16 conversion
    size_t   filePathLen = strlen(filePath);
    wchar_t* pathStr = (wchar_t*)alloca((filePathLen + 1) * sizeof(wchar_t));
    size_t   pathStrLength = MultiByteToWideChar(CP_UTF8, 0, filePath, (int)filePathLen, pathStr, (int)filePathLen);
    pathStr[pathStrLength] = 0;

    struct _stat s = {};
    int          res = _wstat(pathStr, &s);
    if (res != 0)
        return false;
    return true;
}

bool fsFileExist(const ResourceDirectory resourceDir, const char* fileName)
{
    const char* resourcePath = fsGetResourceDirectory(resourceDir);
    char        filePath[FS_MAX_PATH] = {};
    fsAppendPathComponent(resourcePath, fileName, filePath);

    return file_exist(filePath);
}

bool create_directories(const char* path, bool recursive)
{
    if (file_exist(path))
        return true;

    char directoryPath[FS_MAX_PATH] = {};
    strncpy(directoryPath, path, FS_MAX_PATH - 1);
    directoryPath[FS_MAX_PATH - 1] = 0;

    if (recursive)
    {
        // Windows has two options for seperator, therefore check given path which one it contains
        char directorySeparator = '\\';
        if (strchr(directoryPath, '/') != NULL)
        {
            directorySeparator = '/';
        }

        char* loc = strrchr(directoryPath, directorySeparator);
        if (loc)
        {
            char parentPath[FS_MAX_PATH] = {};
            strncpy(parentPath, directoryPath, loc - directoryPath);
            // Recurse parent directories
            if (!create_directories(parentPath, recursive))
                return false;
        }
    }

    // Path utf-16 conversion
    size_t   pathLen = strlen(directoryPath);
    wchar_t* pathStr = (wchar_t*)alloca((pathLen + 1) * sizeof(wchar_t));
    size_t   pathStrLength = MultiByteToWideChar(CP_UTF8, 0, directoryPath, (int)pathLen, pathStr, (int)pathLen);
    pathStr[pathStrLength] = 0;

    if (!CreateDirectoryW(pathStr, NULL))
    {
        DWORD winError = GetLastError();
        if (winError == ERROR_ALREADY_EXISTS)
        {
            return true;
        }

        if (winError == ERROR_PATH_NOT_FOUND)
        {
            LOGF(LogLevel::eERROR, "Unable to create directory {%s}!", directoryPath);
            return false;
        }
    }
    return true;
}

bool fsCreateDirectory(const ResourceDirectory resourceDir, const char* path, bool recursive)
{
    const char* resourcePath = fsGetResourceDirectory(resourceDir);
    char        directoryPath[FS_MAX_PATH] = {};
    fsAppendPathComponent(resourcePath, path, directoryPath);
    if (!*directoryPath)
        return true;

    return create_directories(directoryPath, recursive);
}

bool fsCheckPath(ResourceDirectory rd, const char* path, bool* exist, bool* isDir, bool* isFile)
{
    char fullPath[FS_MAX_PATH] = { 0 };
    fsAppendPathComponent(fsGetResourceDirectory(rd), path, fullPath);

    DWORD attrs = GetFileAttributesA(fullPath);

    if (attrs == INVALID_FILE_ATTRIBUTES)
    {
        *exist = false;
        *isDir = false;
        *isFile = false;

        switch (GetLastError())
        {
        case ERROR_FILE_NOT_FOUND:
        case ERROR_PATH_NOT_FOUND:
            return true;
        }

        // TODO write FormatMessage monstrosity to print strerror()
        LOGF(eERROR, "Failed to retreive path '%s' info: %u", fullPath, (unsigned)GetLastError());
        return false;
    }

    *exist = true;
    *isDir = attrs & FILE_ATTRIBUTE_DIRECTORY;
    *isFile = !*isDir;
    return true;
}

struct WindowsFsDirectoryIterator
{
    bool             finished;
    bool             entryUsed;
    BOOL             success;
    HANDLE           hFind;
    WIN32_FIND_DATAW fd;
    char             buffer[FS_MAX_PATH];
    char             openedPath[FS_MAX_PATH];
};

static bool windowsFsDirectoryIteratorReopen(struct WindowsFsDirectoryIterator* iterator)
{
    wchar_t buffer[FS_MAX_PATH] = {};
    size_t  utf16Len;
    if (mbstowcs_s(&utf16Len, buffer, iterator->openedPath, FS_MAX_PATH - 2))
        return false;

    buffer[utf16Len - 1] = '\\';
    buffer[utf16Len + 0] = '*';
    buffer[utf16Len + 1] = 0;

    iterator->hFind = FindFirstFileW(buffer, &iterator->fd);
    if (iterator->hFind == INVALID_HANDLE_VALUE)
    {
        iterator->hFind = NULL;
        return false;
    }
    iterator->finished = false;
    iterator->success = 1;
    iterator->entryUsed = false;
    return true;
}

bool fsDirectoryIteratorOpen(ResourceDirectory rd, const char* dir, FsDirectoryIterator* out)
{
    *out = 0;

    struct WindowsFsDirectoryIterator* iterator = (struct WindowsFsDirectoryIterator*)tf_calloc(1, sizeof *iterator);

    fsAppendPathComponent(fsGetResourceDirectory(rd), dir, iterator->openedPath);

    if (!windowsFsDirectoryIteratorReopen(iterator))
    {
        fsDirectoryIteratorClose(iterator);
        return false;
    }

    *out = iterator;
    return true;
}

void fsDirectoryIteratorClose(FsDirectoryIterator data)
{
    if (!data)
        return;

    struct WindowsFsDirectoryIterator* iterator = (struct WindowsFsDirectoryIterator*)data;

    if (iterator->hFind)
        FindClose(iterator->hFind);
    tf_free(iterator);
}

bool fsDirectoryIteratorNext(FsDirectoryIterator data, struct FsDirectoryIteratorEntry* outEntry)
{
    struct WindowsFsDirectoryIterator* iterator = (struct WindowsFsDirectoryIterator*)data;

    if (iterator->finished)
    {
        if (!windowsFsDirectoryIteratorReopen(iterator))
            return false;
    }

    for (;;)
    {
        // to use first entry from fsDirectoryIteratorOpen
        if (iterator->entryUsed)
            iterator->success = FindNextFileW(iterator->hFind, &iterator->fd);
        iterator->entryUsed = true;

        if (!iterator->success)
        {
            if (GetLastError() == ERROR_NO_MORE_FILES)
            {
                outEntry->isDir = false;
                outEntry->name = NULL;
                iterator->finished = true;
                return true;
            }
            return false;
        }

        if (iterator->fd.dwFileAttributes & FILE_ATTRIBUTE_OFFLINE)
            continue;

        // To check if symlink we can use this
        // if (iterator->fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)

        bool isDir = iterator->fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;

        if (!WideCharToMultiByte(CP_UTF8, 0, iterator->fd.cFileName, -1, iterator->buffer, sizeof iterator->buffer, NULL, NULL))
            continue;

        // Avoid ".", ".." directories
        if (isDir && iterator->buffer[0] == '.' && (iterator->buffer[1] == 0 || (iterator->buffer[1] == '.' && iterator->buffer[2] == 0)))
        {
            continue;
        }

        outEntry->isDir = isDir;
        outEntry->name = iterator->buffer;
        return true;
    }
}
