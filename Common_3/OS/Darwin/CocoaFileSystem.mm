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

#import <Foundation/Foundation.h>

#include "../../Utilities/Interfaces/IFileSystem.h"
#include "../../Utilities/Interfaces/ILog.h"
#include "../Interfaces/IOperatingSystem.h"

#include "../../Utilities/Interfaces/IMemory.h"

static bool gInitialized = false;

extern "C"
{
    void                         parse_path_statement(char* PathStatment, size_t size);
    extern ResourceDirectoryInfo gResourceDirectories[RD_COUNT];
}

bool initFileSystem(FileSystemInitDesc* pDesc)
{
    if (gInitialized)
    {
        LOGF(LogLevel::eWARNING, "FileSystem already initialized.");
        return true;
    }
    ASSERT(pDesc);

    if (!pDesc->mIsTool)
    {
        char debugDir[FS_MAX_PATH] = { 0 };
        char bundleDir[FS_MAX_PATH] = { 0 };

        const char* resourcePath = [[[[[NSBundle mainBundle] resourceURL] absoluteURL] path] UTF8String];
        size_t      resourcePathLen = strlen(resourcePath);
        ASSERT(resourcePathLen + 2 < FS_MAX_PATH);
        strncpy(bundleDir, resourcePath, resourcePathLen);
        bundleDir[resourcePathLen] = '/';

        NSFileManager* fileManager = [NSFileManager defaultManager];
        NSError*       error = nil;

#if defined(TARGET_IOS)
        // in iOS we use documents folder as working directory
        // for non bundled and generated resource dirs usually RD_DEBUG, RD_LOG
        NSURL* pAppDocumentsUrl = [fileManager URLForDirectory:NSDocumentDirectory
                                                      inDomain:NSUserDomainMask
                                             appropriateForURL:nil
                                                        create:true
                                                         error:&error];
        if (!error)
        {
            const char* documentsPath = [[pAppDocumentsUrl path] UTF8String];
            size_t      documentPathLen = strlen(documentsPath);
            ASSERT(documentPathLen + 2 < FS_MAX_PATH);
            strncpy(debugDir, documentsPath, documentPathLen);
            debugDir[documentPathLen] = '/';
        }
        else
        {
            ASSERT(FALSE);
        }
#else
        // for macos we use the same folder as the bundle since there's
        // not a good way of setting working dir for bundles
        NSString* bundlePathsNString = [[[NSBundle mainBundle] bundlePath] stringByDeletingLastPathComponent];
        [fileManager changeCurrentDirectoryPath:bundlePathsNString];

        const char* bundlePath = [bundlePathsNString UTF8String];
        size_t      bundlePathLen = strlen(bundlePath);
        ASSERT(bundlePathLen + 2 < FS_MAX_PATH);
        strncpy(debugDir, bundlePath, bundlePathLen);
        debugDir[bundlePathLen] = '/';
#endif

        char pathStatmentsPath[FS_MAX_PATH] = { 0 };
        strcat(pathStatmentsPath, resourcePath);
        strcat(pathStatmentsPath, "/" PATHSTATEMENT_FILE_NAME);

        FILE* pathStatement = fopen(pathStatmentsPath, "r");
        if (!pathStatement)
        {
            ASSERT(false);
            return false;
        }

        fseek(pathStatement, 0, SEEK_END);
        size_t fileSize = ftell(pathStatement);
        rewind(pathStatement);

        char* buffer = (char*)tf_malloc(fileSize);
        fileSize = fread(buffer, 1, fileSize, pathStatement);
        fclose(pathStatement);
        parse_path_statement(buffer, fileSize);
        tf_free(buffer);

        for (uint32_t i = 0; i < RD_COUNT; i++)
        {
            char dirPath[FS_MAX_PATH] = { 0 };
            strcat(dirPath, gResourceDirectories[i].mBundled ? bundleDir : debugDir);
            strcat(dirPath, gResourceDirectories[i].mPath);
            strcpy(gResourceDirectories[i].mPath, dirPath);
        }

#if defined(AUTOMATED_TESTING) && defined(ENABLE_SCREENSHOT)
        NSURL* dirURL = [NSURL fileURLWithPath:[NSString stringWithUTF8String:gResourceDirectories[RD_SCREENSHOTS].mPath] isDirectory:TRUE];
        [fileManager createDirectoryAtURL:dirURL withIntermediateDirectories:YES attributes:nil error:&error];
#else
        UNREF_PARAM(error);
#endif
    }

    gInitialized = true;
    return true;
}

void exitFileSystem() { gInitialized = false; }
