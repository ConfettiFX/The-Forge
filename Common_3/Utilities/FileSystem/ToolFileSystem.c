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

extern ResourceDirectoryInfo gResourceDirectories[RD_COUNT];

const char* fsGetResourceDirectory(ResourceDirectory resourceDir)
{
    const ResourceDirectoryInfo* dir = &gResourceDirectories[resourceDir];

    if (!dir->pIO)
    {
        LOGF_IF(eERROR, !dir->mPath[0],
                "Trying to get an unset resource directory '%d', make sure the resourceDirectory is set on start of the application",
                resourceDir);
        ASSERT(dir->mPath[0] != 0);
    }
    return dir->mPath;
}

/************************************************************************/
// MARK: - Minor filename manipulation
/************************************************************************/

static inline bool isDirectorySeparator(char c) { return (c) == '/' || (c) == '\\'; }

static inline char separatorFilter(char c, char separator) { return isDirectorySeparator(c) ? separator : c; }

static inline bool isDriveLetter(const char* path) { return path[0] && path[1] == ':' && isDirectorySeparator(path[2]); }

FORGE_TOOL_API size_t fsNormalizePathContinue(const char* nextPath, char separator, const char* beg, char* cur, const char* end);

FORGE_TOOL_API bool fsIsNormalizedPath(const char* path, char separator)
{
    const char* cur = path;

    // skip drive letter
    if (isDriveLetter(cur))
        cur += 2;

    // Whenever we found any entry except ".."
    bool realEntryFound = 0;

    // -1  cur is not an entry start
    // 0   cur must be an entry start
    // >0  number of dots in the beginning of the entry
    int dotCounter = -1;

    if (cur[0] == '.')
    {
        if (cur[1] == 0)
            return true;

        dotCounter = 1;
        ++cur;
    }

    for (;; ++cur)
    {
        char c = *cur;
        bool isSep = isDirectorySeparator(c);
        if (isSep || !c)
        {
            switch (dotCounter)
            {
            case 0: // double separator "//" detected
                if (isSep)
                    goto RETURN_FALSE;
                break;
            case 1: // entry "." is detected
                goto RETURN_FALSE;
            case 2: // entry ".." is detected
                if (realEntryFound)
                    goto RETURN_FALSE;
                break;
            }

            if (!c)
                break;

            // wrong separator
            if (c != separator)
                goto RETURN_FALSE;

            dotCounter = 0;
        }
        else if (c == '.')
        {
            if (dotCounter >= 0)
            {
                ++dotCounter;
                if (dotCounter > 2)
                    realEntryFound = true;
            }
        }
        else
        {
            realEntryFound = true;
            dotCounter = -1;
        }
    }

    return true;

RETURN_FALSE:
    // Test if opinion of fsNormalizePath and fsIsNormalizedPath matches.
    // Test failure can cause infinite recursion here.
#if defined(FORGE_DEBUG)
    if (strlen(path) > FS_MAX_PATH - 1)
        return false;
    char buffer[FS_MAX_PATH] = { 0 };
    fsNormalizePathContinue(path, separator, buffer, buffer, (char*)UINTPTR_MAX);
    ASSERT(strcmp(buffer, path) != 0);
#endif
    return false;
}

FORGE_TOOL_API size_t fsNormalizePathContinue(const char* nextPath, char separator, const char* beg, char* cur, const char* end)
{
    ASSERT(nextPath);
    ASSERT(separator);
    ASSERT(beg <= cur && cur <= end && beg < end);
    if (end <= beg)
        return 0;

    const bool notEmptyAtStart = cur > beg;

    // noback points to after last separator of "../../../" sequence

    // e.g.:
    // /a/../../b/../c/
    //  ^noback

    // /../a/../b/
    //     ^ noback

    // set noback to beg (dropping const)
    char* noback = cur - (cur - beg);

    if (cur > beg)
    {
        if (isDriveLetter(noback))
            noback += 2;

        if (separatorFilter(*noback, separator) == separator)
            ++noback;

        for (; noback <= cur - 3;)
        {
            char c = noback[0];
            char nc = noback[1];
            char nnc = noback[2];

            if (c != '.' || nc != '.' || nnc != separator)
                break;

            noback += 3;
        }
    }
    else
    {
        if (noback == cur && separatorFilter(*nextPath, separator) == separator)
            ++noback;
    }

    for (const char* src = nextPath; *src; ++src)
    {
        char c = separatorFilter(*src, separator);

        if (c == separator)
        {
            if (                                              // test for "a//b" case
                (cur != beg && isDirectorySeparator(cur[-1])) //
                ||                                            // test for "a/..//b" case
                (cur == beg && src != nextPath))
            {
                // Detailed explanaition

                // "a/..///b" path resolves to "//b".
                // "(cur == dst && src != path)" fixes this to "b"

                // "a/b/..///c" resolves to "a///c"
                // (isDirectorySeparator(cur[-1])) fixes this to "a/c".

                // Phew!
                continue;
            }

            *(cur++) = separator;
            if (cur == end)
                break;
            continue;
        }

        const bool entryStart = cur == beg || cur[-1] == separator;

        if (!entryStart || c != '.')
        {
            *(cur++) = c;
            if (cur == end)
                break;
            continue;
        }

        // c='.' nc='.' nnc='/'

        const char nc = separatorFilter(src[1], separator);
        if (!nc)
            break;

        if (nc == separator)
        {
            // resolve "./"
            ++src;
            continue;
        }

        const char nnc = separatorFilter(src[2], separator);

        // backlink is ".." entry
        const bool backlink = nc == '.' && (nnc == separator || !nnc);

        // Are we having parent dir to resolve backlink?
        bool isNoback = cur == noback;

        if (backlink && isNoback)
            noback += 3; // strlen("../");

        if (!backlink || isNoback)
        {
            // skip unresolvable "../" or whatever characters here
            *(cur++) = c;
            if (cur == end)
                break;
            *(cur++) = nc;
            if (cur == end)
                break;
            if (nnc)
            {
                *(cur++) = nnc;
                if (cur == end)
                    break;
            }
        }
        else
        {
            // resolve ".." (remove "parentdir/..")
            for (cur -= 2; cur >= beg && *cur != separator; --cur)
                ;
            ++cur;
        }

        // In the for(;;) loop we are skipping 1 char
        // We are passing ".." or "../" here.
        // We need to skip only 2 chars if '/' not presented
        src += 1 + (nnc != 0);
    }

    size_t size = (size_t)(cur - beg);

    ASSERT(cur <= end);

    // failure
    if (cur == end)
    {
        cur[-1] = 0;
        return (size_t)(end - beg);
    }

    // If input wasn't empty strings, write "."
    if (!size && (notEmptyAtStart || *nextPath))
    {
        *cur = '.';
        ++cur;
    }

    // success
    *cur = 0;
    ASSERT(fsIsNormalizedPath(beg, separator));
    return size;
}

FORGE_TOOL_API bool fsMergeDirAndFileName(const char* prePath, const char* postPath, char separator, size_t outputSize, char* output)
{
    *output = 0;

    size_t outputLength = fsNormalizePathContinue(prePath, separator, output, output, output + outputSize);

    if (                                         // put separator between paths, if conditions are met:
        outputLength &&                          // if path isn't empty
        output[outputLength - 1] != separator && // if separator is missing
        !isDirectorySeparator(*postPath))        // if separator is missing in postpath
    {
        output[outputLength++] = separator;
        output[outputLength] = 0;
    }

    outputLength = fsNormalizePathContinue(postPath, separator, output, output + outputLength, output + outputSize);

    ASSERT(outputLength <= outputSize);

    bool success = outputLength < outputSize;
    if (!success)
    {
        LOGF(eERROR, "Failed to append path: path exceeds path limit of %llu.", (unsigned long long)outputSize);
        LOGF(eERROR, "Base path is '%s'", prePath);
        LOGF(eERROR, "Appending path is '%s'", postPath);
        LOGF(eERROR, "Only this part that fits: '%s'", output);
    }

    // Delete any trailing directory separator.
    if (outputLength && output[outputLength - 1] == separator)
        output[outputLength - 1] = 0;
    return success;
}

// output size is FS_MAX_PATH
FORGE_TOOL_API void fsAppendPathExtension(const char* basePath, const char* extension, char* output)
{
    size_t       extensionLength = strlen(extension);
    const size_t baseLength = strlen(basePath);

    // + 1 due to a possible added directory slash.
    const size_t maxPathLength = baseLength + extensionLength + 1;

    if (!VERIFYMSG(maxPathLength < FS_MAX_PATH, "Extension path length '%zu' greater than FS_MAX_PATH", maxPathLength))
    {
        return;
    }

    memcpy(output, basePath, baseLength);
    output[baseLength] = 0;

    if (extensionLength == 0)
    {
        return;
    }

    // Extension validation
    for (size_t i = 0; i < extensionLength; i += 1)
    {
        LOGF_IF(eERROR, isDirectorySeparator(extension[i]), "Extension '%s' contains directory specifiers", extension);
        ASSERT(!isDirectorySeparator(extension[i]));
    }
    LOGF_IF(eERROR, extension[extensionLength - 1] == '.', "Extension '%s' ends with a '.' character", extension);

    if (extension[0] == '.')
    {
        extension += 1;
        extensionLength -= 1;
    }

    output[baseLength] = '.';
    memcpy(output + baseLength + 1, extension, extensionLength);
    output[baseLength + 1 + extensionLength] = 0;
}

FORGE_TOOL_API void fsGetParentPath(const char* path, char* output)
{
    size_t pathLength = strlen(path);
    if (!pathLength)
    {
        // We do this just before exit instead of at the beggining of the function to support calls
        // like fsGetParentPath(path, path), were the output path points to the same memory as the input path
        output[0] = 0;
        return;
    }

    const char* dirSeperatorLoc = NULL;
    for (const char* cur = path + pathLength - 1; cur >= path; --cur)
    {
        if (!isDirectorySeparator(*cur))
            continue;
        dirSeperatorLoc = cur;
        break;
    }

    if (!dirSeperatorLoc)
    {
        // We do this just before exit instead of at the beggining of the function to support calls
        // like fsGetParentPath(path, path), were the output path points to the same memory as the input path
        output[0] = 0;
        return;
    }

    size_t outputLength = (size_t)(dirSeperatorLoc - path);

    size_t reslen = outputLength > FS_MAX_PATH - 1 ? FS_MAX_PATH - 1 : outputLength;
    memcpy(output, path, reslen);
    output[reslen] = 0;
}

FORGE_TOOL_API void fsGetPathExtension(const char* path, char* output)
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

    // Make sure it is not "../"
    if (extensionLength == 0 || isDirectorySeparator(dotLocation[0]))
    {
        return;
    }

    strncpy(output, dotLocation, FS_MAX_PATH - 1);
    output[FS_MAX_PATH - 1] = 0;
}

FORGE_TOOL_API void fsReplacePathExtension(const char* path, const char* newExtension, char* output)
{
    size_t       newExtensionLength = strlen(newExtension);
    const size_t baseLength = strlen(path);

    // + 1 due to a possible added directory slash.
    const size_t maxPathLength = baseLength + newExtensionLength + 1;

    ASSERT(baseLength != 0);
    if (!VERIFYMSG(maxPathLength < FS_MAX_PATH, "New extension path length '%zu' greater than FS_MAX_PATH", maxPathLength))
    {
        return;
    }

    strncpy(output, path, FS_MAX_PATH - 1);
    output[FS_MAX_PATH - 1] = 0;

    size_t newPathLength = baseLength;

    if (newExtensionLength == 0)
    {
        return;
    }

    // Extension validation
    for (size_t i = 0; i < newExtensionLength; i += 1)
    {
        LOGF_IF(eERROR, isDirectorySeparator(newExtension[i]), "Extension '%s' contains directory specifiers", newExtension);
        ASSERT(!isDirectorySeparator(newExtension[i]));
    }
    LOGF_IF(eERROR, newExtension[newExtensionLength - 1] == '.', "Extension '%s' ends with a '.' character", newExtension);

    if (newExtension[0] == '.')
    {
        newExtension += 1; // Skip over the first '.'.
        newExtensionLength -= 1;
    }

    char currentExtension[FS_MAX_PATH] = { 0 };
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

FORGE_TOOL_API void fsGetPathFileName(const char* path, char* output)
{
    const size_t pathLength = strlen(path);
    ASSERT(pathLength != 0);

    char parentPath[FS_MAX_PATH] = { 0 };
    fsGetParentPath(path, parentPath);
    size_t parentPathLength = strlen(parentPath);

    if (parentPathLength < pathLength && (isDirectorySeparator(path[parentPathLength])))
    {
        parentPathLength += 1;
    }

    char extension[FS_MAX_PATH] = { 0 };
    fsGetPathExtension(path, extension);

    // Include dot in the length
    const size_t extensionLength = extension[0] != 0 ? strlen(extension) + 1 : 0;

    const size_t outputLength = pathLength - parentPathLength - extensionLength;
    strncpy(output, path + parentPathLength, outputLength);
    output[outputLength] = '\0';
}

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

time_t fsGetLastModifiedTime(ResourceDirectory rd, const char* fileName)
{
    char filePath[FS_MAX_PATH];
    fsMergeDirAndFileName(fsGetResourceDirectory(rd), fileName, '/', sizeof filePath, filePath);

    struct stat fileInfo = { 0 };
    stat(filePath, &fileInfo);
    return fileInfo.st_mtime;
}

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
