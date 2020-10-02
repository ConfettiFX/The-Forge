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

#pragma once

#include "../../OS/Interfaces/IOperatingSystem.h"
#include "../../OS/Interfaces/IFileSystem.h"

#include "../../ThirdParty/OpenSource/EASTL/vector.h"
#include "../../ThirdParty/OpenSource/EASTL/string.h"

/************************************************************************/
// MARK: - File Watcher
/************************************************************************/
#if defined(_WIN32) || defined(__APPLE__) || defined(__linux__)
typedef struct FileWatcher FileWatcher;

typedef void (*FileWatcherCallback)(const char* fileName, uint32_t action);

typedef enum FileWatcherEventMask
{
	FWE_MODIFIED = 1 << 0,
	FWE_ACCESSED = 1 << 1,
	FWE_CREATED = 1 << 2,
	FWE_DELETED = 1 << 3,
} FileWatcherEventMask;

/// Creates a new FileWatcher that watches for changes specified by `eventMask` at `path` and calls `callback` when changes occur.
/// The return value must have `fsFreeFileWatcher` called to free it.
FileWatcher* fsCreateFileWatcher(const char* path, FileWatcherEventMask eventMask, FileWatcherCallback callback);

/// Invalidates and frees `fileWatcher.
void fsFreeFileWatcher(FileWatcher* fileWatcher);
#endif
/************************************************************************/
// MARK: - File iteration
/************************************************************************/
#ifdef TARGET_IOS
void fsRegisterUTIForExtension(const char* uti, const char* extension);
#endif

/// Returns array of all directories inside input resourceDir + directory. String also contains subDirectory
void fsGetSubDirectories(ResourceDirectory resourceDir, const char* subDirectory, eastl::vector<eastl::string>& out);

/// Returns array of all files inside input resourceDir + subDirectory. String also contains subDirectory
/// Specifying empty extension will collect all files
void fsGetFilesWithExtension(ResourceDirectory resourceDir, const char* subDirectory, const char* extension, eastl::vector<eastl::string>& out);
/************************************************************************/
/************************************************************************/
