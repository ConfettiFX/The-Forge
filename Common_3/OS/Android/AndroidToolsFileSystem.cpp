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

#include "../../Utilities/Interfaces/IToolFileSystem.h"
#include "../../Utilities/Interfaces/IFileSystem.h"
#include "../../Utilities/Interfaces/ILog.h"

void fsGetFilesWithExtension(ResourceDirectory resourceDir, const char* subDirectory, const char* extension, char *** out, int * count)
{
    (void)resourceDir;
    (void)subDirectory;
    (void)extension;
    (void)out;
    (void)count;

    LOGF(LogLevel::eERROR, "Android fsGetFilesWithExtension is not implemented.");
}

void fsGetSubDirectories(ResourceDirectory resourceDir, const char* subDirectory, char *** out, int * count)
{
    (void)resourceDir;
    (void)subDirectory;
    (void)out;
    (void)count;

    LOGF(LogLevel::eERROR, "Android fsGetSubDirectories is not implemented.");
}

bool fsRemoveFile(const ResourceDirectory resourceDir, const char* fileName)
{
    (void)resourceDir;
    (void)fileName;

    LOGF(LogLevel::eERROR, "Android fsRemoveFile is not implemented.");

    return false;
}

bool fsRenameFile(const ResourceDirectory resourceDir, const char* fileName, const char* newFileName)
{
    (void)resourceDir;
    (void)fileName;
    (void)newFileName;

    LOGF(LogLevel::eERROR, "Android fsRenameFile is not implemented.");

    return false;
}

bool fsCopyFile(const ResourceDirectory sourceResourceDir, const char* sourceFileName, const ResourceDirectory destResourceDir, const char* destFileName)
{
    (void)sourceResourceDir;
    (void)sourceFileName;
    (void)destResourceDir;
    (void)destFileName;

    LOGF(LogLevel::eERROR, "Android fsCopyFile is not implemented.");

    return false;
}

bool fsFileExist(const ResourceDirectory resourceDir, const char* fileName)
{
    FileStream fileStream;
    if (!fsOpenStreamFromPath(resourceDir, fileName, FM_READ, NULL, &fileStream))
    {
        LOGF(LogLevel::eERROR, "%s does not exist.", fileName);
        return false;
    }
    fsCloseStream(&fileStream);

	return true;
}
