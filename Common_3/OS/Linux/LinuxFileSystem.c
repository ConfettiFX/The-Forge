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
#include <fcntl.h> //for open and O_* enums
#include <pwd.h>
#include <stdlib.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "../../Utilities/Interfaces/IFileSystem.h"
#include "../../Utilities/Interfaces/ILog.h"

static bool        gInitialized = false;
static const char* gResourceMounts[RM_COUNT];

static const char* getResourceMount(ResourceMount mount) { return gResourceMounts[mount]; }

static char gApplicationPath[FS_MAX_PATH] = { 0 };

static const char* gHomedir;

void fsGetParentPath(const char* path, char* output);

bool initFileSystem(FileSystemInitDesc* pDesc)
{
    if (gInitialized)
    {
        LOGF(eWARNING, "FileSystem already initialized.");
        return true;
    }
    ASSERT(pDesc);

    // Get application directory
    char applicationFilePath[FS_MAX_PATH] = { 0 };
    if (readlink("/proc/self/exe", applicationFilePath, FS_MAX_PATH) < 0)
    {
        LOGF(eERROR, "Failed to acquire self directory.");
        return false;
    }

    pSystemFileIO->GetResourceMount = getResourceMount;

    for (uint32_t i = 0; i < RM_COUNT; ++i)
        gResourceMounts[i] = "";

    fsGetParentPath(applicationFilePath, gApplicationPath);
    gResourceMounts[RM_CONTENT] = gApplicationPath;
    gResourceMounts[RM_DEBUG] = gApplicationPath;
    gResourceMounts[RM_SAVE_0] = gApplicationPath;

    // Get user directory
    if ((gHomedir = getenv("HOME")) == NULL)
    {
        gHomedir = getpwuid(getuid())->pw_dir;
    }
    gResourceMounts[RM_DOCUMENTS] = gHomedir;

    // Override Resource mounts
    for (uint32_t i = 0; i < RM_COUNT; ++i)
    {
        if (pDesc->pResourceMounts[i])
            gResourceMounts[i] = pDesc->pResourceMounts[i];
    }

    // Get temp directory
    // const char* tempdir;
    // if ((tempdir = getenv("TMPDIR")) == NULL)
    //{
    //	tempdir = getpwuid(getuid())->pw_dir;
    //}
    // fsAppendPathComponent(tempdir, "tmp", gTempDirectory);

    gInitialized = true;
    return true;
}

void exitFileSystem(void) { gInitialized = false; }
