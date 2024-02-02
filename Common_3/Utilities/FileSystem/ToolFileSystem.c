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

#include "../../Utilities/Interfaces/ILog.h"
#include "../Interfaces/IToolFileSystem.h"

#if defined(_WINDOWS)
#define strncasecmp(x, y, z) _strnicmp(x, y, z)
#endif

/************************************************************************/
// MARK: - File iteration
/************************************************************************/

bool fsGetFilesWithExtension(ResourceDirectory resourceDir, const char* subDirectory, const char* extension, char*** out, int* count)
{
    *out = NULL;
    *count = 0;

    char directoryPath[FS_MAX_PATH] = { 0 };
    fsAppendPathComponent(fsGetResourceDirectory(resourceDir), subDirectory, directoryPath);

    FsDirectoryIterator iterator = NULL;
    if (!fsDirectoryIteratorOpen(resourceDir, subDirectory, &iterator))
        return false;

    if (extension)
    {
        if (*extension == '*')
            ++extension;
        if (*extension == '.')
            ++extension;
        if (!*extension)
            extension = NULL;
    }

    int filesFound = 0;

    bool success = true;

    for (;;)
    {
        struct FsDirectoryIteratorEntry entry;
        success = fsDirectoryIteratorNext(iterator, &entry);
        if (!success || !entry.name)
            break;

        if (entry.isDir)
            continue;

        char fileExt[FS_MAX_PATH] = { 0 };
        fsGetPathExtension(entry.name, fileExt);
        size_t fileExtLen = strlen(fileExt);

        if ((!extension) || (extension[0] == 0 && fileExtLen == 0) || (fileExtLen > 0 && strncasecmp(fileExt, extension, fileExtLen) == 0))
        {
            filesFound += 1;
        }
    }

    if (!success || !filesFound)
    {
        fsDirectoryIteratorClose(iterator);
        return success;
    }

    size_t stringsSize = filesFound * sizeof(char*) + filesFound * sizeof(char) * FS_MAX_PATH;

    char** stringList = (char**)tf_malloc(stringsSize);

    char* firstString = ((char*)stringList + filesFound * sizeof(char*));
    for (int i = 0; i < filesFound; ++i)
    {
        stringList[i] = firstString + (sizeof(char) * FS_MAX_PATH * i);
    }

    int strIndex = 0;
    for (;;)
    {
        struct FsDirectoryIteratorEntry entry;
        success = fsDirectoryIteratorNext(iterator, &entry);
        if (!success || !entry.name)
            break;

        if (entry.isDir)
            continue;

        char fileExt[FS_MAX_PATH] = { 0 };
        fsGetPathExtension(entry.name, fileExt);
        size_t fileExtLen = strlen(fileExt);

        if ((!extension) || (extension[0] == 0 && fileExtLen == 0) || (fileExtLen > 0 && strncasecmp(fileExt, extension, fileExtLen) == 0))
        {
            char result[FS_MAX_PATH] = { 0 };
            fsAppendPathComponent(subDirectory, entry.name, result);
            char* dest = stringList[strIndex++];
            strcpy(dest, result);
        }
    }

    fsDirectoryIteratorClose(iterator);

    if (!success)
    {
        tf_free(*out);
    }
    else
    {
        *out = stringList;
        *count = filesFound;
    }

    return success;
}

bool fsGetSubDirectories(ResourceDirectory resourceDir, const char* subDirectory, char*** out, int* count)
{
    *out = NULL;
    *count = 0;

    char directoryPath[FS_MAX_PATH] = { 0 };
    fsAppendPathComponent(fsGetResourceDirectory(resourceDir), subDirectory, directoryPath);

    FsDirectoryIterator iterator = NULL;
    if (!fsDirectoryIteratorOpen(resourceDir, subDirectory, &iterator))
        return false;

    bool success = true;

    int filesFound = 0;

    for (;;)
    {
        struct FsDirectoryIteratorEntry entry;
        success = fsDirectoryIteratorNext(iterator, &entry);
        if (!success || !entry.name)
            break;

        if (entry.isDir)
            ++filesFound;
    }

    if (!success || !filesFound)
    {
        fsDirectoryIteratorClose(iterator);
        return success;
    }

    size_t stringsSize = filesFound * sizeof(char*) + filesFound * sizeof(char) * FS_MAX_PATH;
    char** stringList = (char**)tf_malloc(stringsSize);
    char*  firstString = ((char*)stringList + filesFound * sizeof(char*));
    for (int i = 0; i < filesFound; ++i)
    {
        stringList[i] = firstString + (sizeof(char) * FS_MAX_PATH * i);
    }

    *out = stringList;
    *count = filesFound;

    int strIndex = 0;
    for (;;)
    {
        struct FsDirectoryIteratorEntry entry;
        success = fsDirectoryIteratorNext(iterator, &entry);
        if (!success || !entry.name)
            break;

        if (!entry.isDir)
            continue;

        char result[FS_MAX_PATH] = { 0 };
        fsAppendPathComponent(subDirectory, entry.name, result);
        char* dest = stringList[strIndex++];
        strcpy(dest, result);
    }

    fsDirectoryIteratorClose(iterator);

    if (!success)
    {
        tf_free(*out);
    }
    else
    {
        *out = stringList;
        *count = filesFound;
    }

    return success;
}

// Windows version implemented in WindowsToolsFileSystem.cpp
#if !defined(_WINDOWS)
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>

bool fsCheckPath(ResourceDirectory rd, const char* path, bool* exist, bool* isDir, bool* isFile)
{
    const char* resourcePath = fsGetResourceDirectory(rd);
    char        fullPath[FS_MAX_PATH] = { 0 };
    fsAppendPathComponent(resourcePath, path, fullPath);

    // 'lstat' won't resolve symlink entries, so we use 'stat'.
    // We are only interested in entry targeted by symlink
    struct stat st = { 0 };
    if (stat(fullPath, &st) == 0)
    {
        *exist = true;
        *isDir = S_ISDIR(st.st_mode);
        *isFile = S_ISREG(st.st_mode);
        return true;
    }

    *exist = false;
    *isDir = false;
    *isFile = false;

    if (errno == ENOENT)
        return true;

    LOGF(eERROR, "Failed to retreive path '%s' info: '%'", fullPath, strerror(errno));
    return false;
}

bool fsDirectoryIteratorOpen(ResourceDirectory rd, const char* dir, FsDirectoryIterator* out)
{
    *out = 0;

    const char* resourcePath = fsGetResourceDirectory(rd);
    char        fullPath[FS_MAX_PATH] = { 0 };
    fsAppendPathComponent(resourcePath, dir, fullPath);

    DIR* directory = opendir(fullPath);
    if (!directory)
    {
        if (errno == ENOENT)
            return false;
        LOGF(eERROR, "Failed to open directory '%s': '%'", fullPath, strerror(errno));
        return false;
    }

    *out = (FsDirectoryIterator)directory;
    return true;
}

void fsDirectoryIteratorClose(FsDirectoryIterator iterator)
{
    DIR* directory = (DIR*)iterator;

    if (closedir(directory))
    {
        // We expect only EBADF here - Invalid directory stream descriptor.
        LOGF(eERROR, "Failed to close directory: '%'", strerror(errno));
    }
}

bool fsDirectoryIteratorNext(FsDirectoryIterator iterator, struct FsDirectoryIteratorEntry* outEntry)
{
    *outEntry = (struct FsDirectoryIteratorEntry){ 0 };

    DIR* directory = (DIR*)iterator;

    for (;;)
    {
        errno = 0;
        struct dirent* entry = readdir(directory);
        if (errno)
        {
            LOGF(eERROR, "Failed to iterate directory: '%'", strerror(errno));
            return false;
        }

        if (!entry)
        {
            rewinddir(directory);
            return true;
        }

        bool is_dir = false;
        bool is_file = false;

        switch (entry->d_type)
        {
        case DT_DIR:
            is_dir = true;
            break;
        case DT_REG:
            is_file = true;
            break;

        case DT_LNK:     // link to file, needed to resolve
        case DT_UNKNOWN: // filesystem doesn't support getting type from dirent
        {
            struct stat st = { 0 };
            if (stat(entry->d_name, &st))
            {
                LOGF(eERROR, "Failed to retreive path '%s' info: '%'", entry->d_name, strerror(errno));
                continue;
            }

            is_dir = S_ISDIR(st.st_mode);
            is_file = S_ISREG(st.st_mode);
        }
        break;
        }

        if (!is_dir && !is_file)
            continue;

        // Avoid ".", ".." directories
        if (is_dir && entry->d_name[0] == '.' && (entry->d_name[1] == 0 || (entry->d_name[1] == '.' && entry->d_name[2] == 0)))
        {
            continue;
        }

        outEntry->isDir = is_dir;
        outEntry->name = entry->d_name;
        return true;
    }
}
#endif
