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

#include <UIKit/UIKit.h>

#include "../../Utilities/Interfaces/ILog.h"
#include "../../Utilities/Interfaces/IToolFileSystem.h"

bool fsGetFilesWithExtension(ResourceDirectory resourceDir, const char* subDirectory, const char* extension, char*** out, int* count)
{
    char directory[FS_MAX_PATH] = {};
    fsAppendPathComponent(fsGetResourceDirectory(resourceDir), subDirectory, directory);

    NSURL*                         pathURL = [NSURL fileURLWithPath:[NSString stringWithUTF8String:directory]];
    NSDirectoryEnumerator<NSURL*>* enumerator =
        [[NSFileManager defaultManager] enumeratorAtURL:pathURL
                             includingPropertiesForKeys:@[ NSURLPathKey ]
                                                options:NSDirectoryEnumerationSkipsSubdirectoryDescendants
                                           errorHandler:^BOOL(NSURL* _Nonnull url, NSError* _Nonnull error) {
                                               LOGF(LogLevel::eWARNING, "Error enumerating directory at url %s: %s",
                                                    [[url path] UTF8String], [[error description] UTF8String]);
                                               return YES;
                                           }];
    NSString* pathComponent = nil;

    if (extension[0] == '*')
    {
        ++extension;
    }
    if (extension[0] == '.')
    {
        ++extension;
    }

    bool hasAnyRegex = false;

    NSString* beforeAnyRegex = nil;
    NSString* afterAnyRegex = nil;
    if (extension != 0)
    {
        pathComponent = [NSString stringWithUTF8String:extension];

        const char* fullExpression = extension;
        while (*fullExpression != '\0')
        {
            if (*fullExpression == '*')
            {
                size_t index = fullExpression - extension;
                size_t last = [pathComponent length] - 1;

                if (index == 0 || index == last)
                {
                    break;
                }

                hasAnyRegex = true;

                NSRange range = NSMakeRange(0, index);
                beforeAnyRegex = [pathComponent substringWithRange:range];
                range = NSMakeRange(index + 1, last - index);
                afterAnyRegex = [pathComponent substringWithRange:range];

                break;
            }

            ++fullExpression;
        }
    }

    NSMutableArray* unsortedArray = [NSMutableArray array];

    for (NSURL* url in enumerator)
    {
        NSString* lastPathComponent = url.lastPathComponent;
        if (hasAnyRegex)
        {
            if (![lastPathComponent containsString:beforeAnyRegex] || ![lastPathComponent containsString:afterAnyRegex])
            {
                continue;
            }
        }
        else
        {
            if ([pathComponent length] && ![lastPathComponent containsString:pathComponent])
            {
                continue;
            }
        }

        [unsortedArray addObject:url];
    }

    NSArray* sortedArray = [unsortedArray sortedArrayUsingComparator:^NSComparisonResult(NSURL* _Nonnull first, NSURL* _Nonnull second) {
        return [[first path] compare:[second path]];
    }];

    NSEnumerator* sortedEnumerator = [sortedArray objectEnumerator];

    int filesFound = 0;
    for (NSURL* url in sortedEnumerator)
    {
        (void)url;
        filesFound += 1;
    }

    *out = NULL;
    *count = 0;
    if (filesFound > 0)
    {
        char** stringList = (char**)tf_malloc(filesFound * sizeof(char*) + filesFound * sizeof(char) * FS_MAX_PATH);
        char*  firstString = ((char*)stringList + filesFound * sizeof(char*));
        for (int i = 0; i < filesFound; ++i)
        {
            stringList[i] = firstString + (sizeof(char) * FS_MAX_PATH * i);
        }
        *out = stringList;
        *count = filesFound;
    }
    else
        return false;

    int strIndex = 0;
    sortedEnumerator = [sortedArray objectEnumerator];
    for (NSURL* url in sortedEnumerator)
    {
        char        result[FS_MAX_PATH] = {};
        const char* filename = [[url lastPathComponent] UTF8String];
        fsAppendPathComponent(subDirectory, filename, result);

        char* dest = (*out)[strIndex++];
        strcpy(dest, result);
    }

    return true;
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

    NSFileManager* fileManager = [NSFileManager defaultManager];
    NSString*      nsFilePath = [NSString stringWithUTF8String:filePath];
    if ([fileManager fileExistsAtPath:nsFilePath])
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