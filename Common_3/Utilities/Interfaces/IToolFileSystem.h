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

#pragma once
#include "IFileSystem.h"

#if !defined(_WINDOWS) && !defined(__APPLE__) && !defined(__linux__)
#error IToolFileSystem.h only implemented on Mac, Windows, Linux
#endif

#ifdef __cplusplus
extern "C"
{
#endif

    /************************************************************************/
    // MARK: - File Watcher
    /************************************************************************/
    typedef struct FileWatcher FileWatcher;

    typedef void (*FileWatcherCallback)(const char* fileName, uint32_t action, void* userData);

    typedef enum FileWatcherEventMask
    {
        FWE_MODIFIED = 1 << 0,
        FWE_ACCESSED = 1 << 1,
        FWE_CREATED = 1 << 2,
        FWE_DELETED = 1 << 3,
    } FileWatcherEventMask;

    /// Creates a new FileWatcher that watches for changes specified by `eventMask` at `path` and calls `callback` when changes occur.
    /// The return value must have `fsFreeFileWatcher` called to free it.
    FORGE_TOOL_API FileWatcher* fsCreateFileWatcher(const char* path, FileWatcherEventMask eventMask, FileWatcherCallback callback,
                                                    void* callbackUserData);

    /// Invalidates and frees `fileWatcher.
    FORGE_TOOL_API void fsFreeFileWatcher(FileWatcher* fileWatcher);

/************************************************************************/
// MARK: - File iteration
/************************************************************************/
#ifdef TARGET_IOS
    void fsRegisterUTIForExtension(const char* uti, const char* extension);
#endif

    typedef void*       FsDirectoryIterator;
    FORGE_TOOL_API bool fsDirectoryIteratorOpen(ResourceDirectory rd, const char* dir, FsDirectoryIterator* out);
    FORGE_TOOL_API void fsDirectoryIteratorClose(FsDirectoryIterator iterator);

    // If outEntry->name is NULL, than this is last entry in the loop.
    // Loop goes to the first entry after last one, so it never stops.
    // Iterator skips everything that is not a file or directory.
    // Returns false only on system IO failure.
    struct FsDirectoryIteratorEntry
    {
        bool        isDir;
        const char* name;
    };
    FORGE_TOOL_API bool fsDirectoryIteratorNext(FsDirectoryIterator iterator, struct FsDirectoryIteratorEntry* outEntry);

    /// Returns and allocates an array of all directories inside input resourceDir + directory. String also contains subDirectory
    FORGE_TOOL_API bool fsGetSubDirectories(ResourceDirectory resourceDir, const char* subDirectory, char*** out, int* count);

    /// Returns and allocates array of all files inside input resourceDir + subDirectory. String also contains subDirectory
    /// Specifying empty extension will collect all files
    FORGE_TOOL_API bool fsGetFilesWithExtension(ResourceDirectory resourceDir, const char* subDirectory, const char* extension, char*** out,
                                                int* count);
    /************************************************************************/
    /************************************************************************/

    // Fetches entry info.
    // On some platforms entry types are not limited to file or directory.
    // Returns false if fetch is failed.
    FORGE_TOOL_API bool fsCheckPath(ResourceDirectory resourceDir, const char* path, bool* exist, bool* isDir, bool* isFile);

    FORGE_TOOL_API bool fsRemoveFile(ResourceDirectory resourceDir, const char* fileName);

    FORGE_TOOL_API bool fsRenameFile(ResourceDirectory resourceDir, const char* fileName, const char* newFileName);

    FORGE_TOOL_API bool fsCopyFile(ResourceDirectory sourceResourceDir, const char* sourceFileName, const ResourceDirectory destResourceDir,
                                   const char* destFileName);

    FORGE_TOOL_API bool fsFileExist(ResourceDirectory resourceDir, const char* fileName);

    /// Recursively create directories for given resourceDir and path
    /// Returns true if directories are created or already exist
    FORGE_TOOL_API bool fsCreateDirectory(ResourceDirectory resourceDir, const char* path, bool recursive);

    /************************************************************************/
    // MARK: - Minor filename manipulation
    /************************************************************************/

    /// Unnormalized path:
    /// - has / or \ not matching correct separator.
    /// - has shrinkable ./ (current dir), // (double slash), ../ (back hardlink)
    ///   *- current dir shrinks from "./" to "." or from "./a" to "a"
    ///   *- double slash shrinks to single slash
    ///   *- back hardlink shrinks from "a/../b" to "b", but "../a" won't shrink
    ///
    /// Note: "a/.." normalizes into ".", but "" is also normalized empty string
    ///
    /// Result is true when 'path' is normalized with custom 'separator'.
    FORGE_TOOL_API bool fsIsNormalizedPath(const char* path, char separator);

    /// Appends string buffer 'outputBeg' with normalized version of 'nextPath'
    ///
    ///   'separator'  prefered character to replace all \ and /.
    ///   'outputBeg'  start of output buffer.
    ///   'outputCur'  position to put 'nextPath' in.
    ///   'outputEnd'  end of output buffer (outputEnd = buffer + bufferSize)
    ///
    ///   Path between 'outputBeg' and 'outputCur' can be altered when 'nextPath'
    ///   contains backlinks "/../".
    ///
    ///   Returns new length of string. Can be less than length between beg and cur.
    ///   On success, length is less than size of the buffer.
    ///   On failure, length is equal to size of the buffer.
    ///   Failure is when 'nextPath' insertion is unfinished.
    FORGE_TOOL_API size_t fsNormalizePathContinue(const char* nextPath, char separator, //
                                                  const char* outputBeg, char* outputCur, const char* outputEnd);

    /// Normalize 'path' with custom 'separator' into 'buffer'
    ///
    ///  'output' buffer size required to be " >= strlen(path)+1",
    ///  use fsNormalizePathContinue to avoid that restriction.
    ///
    ///  'path' and 'output' can point to single buffer:
    ///  fsNormalizePath(str, '/', str)
    ///
    /// Return value: length of 'output' string. Always success.
    static inline size_t fsNormalizePath(const char* path, char separator, char* output)
    {
        return fsNormalizePathContinue(path, separator, output, output, (char*)UINTPTR_MAX);
    }

    /// same as fsAppendPathComponent but with extra options
    /// 'separator' must be '/', but '\' in WindowsFileSystem.cpp
    /// 'dstSize'   size of dst buffer
    FORGE_TOOL_API bool fsMergeDirAndFileName(const char* dir, const char* path, char separator, size_t dstSize, char* dst);

    /// Write normalized version of 'dir'+ 'separator' + 'path' to dst
    /// requires dst buffer size to be >= FS_MAX_PATH
    /// Default use-case:
    ///   char path[FS_MAX_PATH];
    ///   fsAppendPathComponent(fsGetResourceDirectory(rd), fileName, path);
    static inline bool fsAppendPathComponent(const char* dir, const char* path, char* dst)
    {
        return fsMergeDirAndFileName(dir, path, '/', FS_MAX_PATH, dst);
    }

    /// Appends `newExtension` to `basePath`.
    /// If `basePath` already has an extension, `newExtension` will be appended to the end.
    /// `output` buffer size required to be >= FS_MAX_PATH
    FORGE_TOOL_API void fsAppendPathExtension(const char* basePath, const char* newExtension, char* output);

    /// Appends `newExtension` to `basePath`.
    /// If `basePath` already has an extension, its previous extension will be replaced by `newExtension`.
    /// `output` buffer size required to be >= FS_MAX_PATH
    FORGE_TOOL_API void fsReplacePathExtension(const char* path, const char* newExtension, char* output);

    /// Get `path`'s parent path, excluding the end seperator.
    /// `output` buffer size required to be >= FS_MAX_PATH
    FORGE_TOOL_API void fsGetParentPath(const char* path, char* output);

    /// Get `path`'s file name, without extension or parent path.
    /// `output` buffer size required to be >= FS_MAX_PATH
    FORGE_TOOL_API void fsGetPathFileName(const char* path, char* output);

    /// Returns `path`'s extension, excluding the '.'.
    /// `output` buffer size required to be >= FS_MAX_PATH
    FORGE_TOOL_API void fsGetPathExtension(const char* path, char* output);

    /************************************************************************/
    /************************************************************************/

    /************************************************************************/
    // MARK: - File Queries
    /************************************************************************/

    /// Gets the time of last modification for the file at `fileName`, within 'resourceDir'.
    FORGE_TOOL_API time_t      fsGetLastModifiedTime(ResourceDirectory resourceDir, const char* fileName);
    FORGE_TOOL_API const char* fsGetResourceDirectory(ResourceDirectory resourceDi);
    /************************************************************************/

#ifdef __cplusplus
} // extern "C"
#endif
