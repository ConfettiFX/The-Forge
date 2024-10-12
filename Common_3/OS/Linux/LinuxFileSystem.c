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

static bool gInitialized = false;

extern ResourceDirectoryInfo gResourceDirectories[RD_COUNT];
void                         parse_path_statement(char* PathStatment, size_t size);

bool initFileSystem(FileSystemInitDesc* pDesc)
{
    if (gInitialized)
    {
        LOGF(eWARNING, "FileSystem already initialized.");
        return true;
    }
    ASSERT(pDesc);

    // Get application directory
    if (!pDesc->mIsTool)
    {
        const char* pathStatementPath = PATHSTATEMENT_FILE_NAME;

        FILE* pathStatments = fopen(pathStatementPath, "r");
        if (!pathStatments)
        {
            ASSERT(false);
            return false;
        }

        fseek(pathStatments, 0, SEEK_END);
        size_t fileSize = ftell(pathStatments);
        rewind(pathStatments);

        char* buffer = (char*)tf_malloc(fileSize);
        // size may change after converting \r\n into \n
        fileSize = fread(buffer, 1, fileSize, pathStatments);
        fclose(pathStatments);

        parse_path_statement(buffer, fileSize);
        tf_free(buffer);
    }

    gInitialized = true;
    return true;
}

void exitFileSystem(void) { gInitialized = false; }
