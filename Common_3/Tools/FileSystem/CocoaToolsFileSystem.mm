/*
 * Copyright (c) 2018-2021 The Forge Interactive Inc.
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

#include <AppKit/AppKit.h>

#include "IToolFileSystem.h"

#include "../../OS/Interfaces/ILog.h"
#include "../../OS/Interfaces/IMemory.h"

#pragma mark - FileWatcher

struct FileWatcher
{
	FileWatcherCallback mCallback;
	uint32_t            mEventMask;
	FSEventStreamRef    mStream;
};

static void fswCbFunc(
	ConstFSEventStreamRef streamRef, void* data, size_t numEvents, void* eventPaths, const FSEventStreamEventFlags eventFlags[],
	const FSEventStreamEventId eventIds[])
{
	FileWatcher* fswData = (FileWatcher*)data;
	const char** paths = (const char**)eventPaths;

	for (size_t i = 0; i < numEvents; ++i)
	{
		if ((fswData->mEventMask & FWE_MODIFIED) && (eventFlags[i] & kFSEventStreamEventFlagItemModified))
		{
			fswData->mCallback(paths[i], FWE_MODIFIED);
		}
		if ((fswData->mEventMask & FWE_ACCESSED) && (eventFlags[i] & kFSEventStreamEventFlagItemInodeMetaMod))
		{
			fswData->mCallback(paths[i], FWE_ACCESSED);
		}
		if ((fswData->mEventMask & FWE_CREATED) && (eventFlags[i] & kFSEventStreamEventFlagItemCreated))
		{
			fswData->mCallback(paths[i], FWE_CREATED);
		}
		if ((fswData->mEventMask & FWE_DELETED) && (eventFlags[i] & kFSEventStreamEventFlagItemRemoved))
		{
			fswData->mCallback(paths[i], FWE_DELETED);
		}
	}
};

FileWatcher* fsCreateFileWatcher(const char* path, FileWatcherEventMask eventMask, FileWatcherCallback callback)
{
	FileWatcher* watcher = (FileWatcher*)tf_calloc(1, sizeof(FileWatcher));
	watcher->mEventMask = eventMask;
	watcher->mCallback = callback;
	CFStringRef          paths[] = { CFStringCreateWithCString(NULL, path, kCFStringEncodingUTF8) };
	CFArrayRef           pathsToWatch = CFArrayCreate(NULL, (const void**)paths, 1, &kCFTypeArrayCallBacks);
	CFAbsoluteTime       latency = 0.125;    // in seconds
	FSEventStreamContext context = { 0, watcher, NULL, NULL, NULL };
	watcher->mStream = FSEventStreamCreate(
		NULL, fswCbFunc, &context, pathsToWatch, kFSEventStreamEventIdSinceNow, latency, kFSEventStreamCreateFlagFileEvents);
	CFRelease(pathsToWatch);
	CFRelease(paths[0]);
	if (watcher->mStream)
	{
		FSEventStreamScheduleWithRunLoop(watcher->mStream, CFRunLoopGetMain(), kCFRunLoopDefaultMode);
		if (!FSEventStreamStart(watcher->mStream))
		{
			FSEventStreamInvalidate(watcher->mStream);
			FSEventStreamRelease(watcher->mStream);
		}
	}

	return watcher;
}

void fsFreeFileWatcher(FileWatcher* fileWatcher)
{
	if (fileWatcher->mStream)
	{
		FSEventStreamStop(fileWatcher->mStream);
		FSEventStreamInvalidate(fileWatcher->mStream);
		FSEventStreamRelease(fileWatcher->mStream);
	}
	
	tf_free(fileWatcher);
}

void fsGetFilesWithExtension(ResourceDirectory resourceDir, const char* subDirectory, const char* extension, char*** out, int* count)
{
	char directory[FS_MAX_PATH] = {};
	fsAppendPathComponent(fsGetResourceDirectory(resourceDir), subDirectory, directory);
	
    NSURL *pathURL = [NSURL fileURLWithPath:[NSString stringWithUTF8String:directory]];
    NSDirectoryEnumerator<NSURL*>* enumerator = [[NSFileManager defaultManager] enumeratorAtURL:pathURL includingPropertiesForKeys:@[NSURLPathKey] options:NSDirectoryEnumerationSkipsSubdirectoryDescendants errorHandler:^BOOL(NSURL * _Nonnull url, NSError * _Nonnull error)
	{
        LOGF(LogLevel::eWARNING, "Error enumerating directory at url %s: %s", [[url path] UTF8String], [[error description] UTF8String]);
        return YES;
    }];
	NSString *pathComponent = nil;

	if(extension[0] == '*')
	{
		++extension;
	}
	if(extension[0] == '.')
	{
		++extension;
	}

	bool hasAnyRegex = false;

	NSString* beforeAnyRegex = nil;
	NSString* afterAnyRegex = nil;
	if (extension != nil)
	{
		pathComponent = [NSString stringWithUTF8String:extension];

		const char* fullExpression = extension;
		while(*fullExpression != '\0')
		{
			if (*fullExpression == '*')
			{
				size_t index = fullExpression - extension;
				size_t last = [pathComponent length] - 1;

				if(index == 0 || index == last)
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

	NSMutableArray *unsortedArray = [NSMutableArray array];

	for (NSURL* url in enumerator)
	{
		NSString* lastPathComponent = url.lastPathComponent;
		if(hasAnyRegex)
		{
			if(![lastPathComponent containsString:beforeAnyRegex] ||
			   ![lastPathComponent containsString:afterAnyRegex])
			{
				continue;
			}
		}
		else
		{
			if (![lastPathComponent containsString:pathComponent])
			{
				continue;
			}
		}

		[unsortedArray addObject:url];
	}

	NSArray* sortedArray = [unsortedArray sortedArrayUsingComparator:^NSComparisonResult(NSURL* _Nonnull first, NSURL* _Nonnull second)
	{
		return [[first path] compare:[second path]];
	}];

	NSEnumerator* sortedEnumerator = [sortedArray objectEnumerator];

    int filesFound = 0;
    for (NSURL* url in sortedEnumerator)
    {
        filesFound += 1;
    }
    
    *out = NULL;
    *count = 0;
    if (filesFound > 0)
    {
        char** stringList = (char**)tf_malloc(filesFound * sizeof(char*) + filesFound * sizeof(char) * FS_MAX_PATH);
        char* firstString = ((char*)stringList + filesFound * sizeof(char*));
        for (int i = 0; i < filesFound; ++i)
        {
            stringList[i] = firstString + (sizeof(char) * FS_MAX_PATH * i);
        }
        *out = stringList;
        *count = filesFound;
    }
    
    int strIndex = 0;
    sortedEnumerator = [sortedArray objectEnumerator];
	for (NSURL* url in sortedEnumerator)
	{
		char result[FS_MAX_PATH] = {};
		eastl::string filename = [[url lastPathComponent] UTF8String];
		fsAppendPathComponent(subDirectory, filename.c_str(), result);
        
		char * dest = (*out)[strIndex++];
        strcpy(dest, result);
	}
}

void fsGetSubDirectories(ResourceDirectory resourceDir, const char* subDirectory, char*** out, int* count)
{
	char directory[FS_MAX_PATH] = {};
	fsAppendPathComponent(fsGetResourceDirectory(resourceDir), subDirectory, directory);
	
    NSURL *pathURL = [NSURL fileURLWithPath:[NSString stringWithUTF8String:directory]];
    NSDirectoryEnumerator<NSURL*>* enumerator = [[NSFileManager defaultManager] enumeratorAtURL:pathURL includingPropertiesForKeys:@[NSURLPathKey, NSURLIsDirectoryKey] options:NSDirectoryEnumerationSkipsSubdirectoryDescendants | NSDirectoryEnumerationProducesRelativePathURLs errorHandler:^BOOL(NSURL * _Nonnull url, NSError * _Nonnull error)
	{
        LOGF(eWARNING, "Error enumerating directory at url %s: %s", [[url path] UTF8String], [[error description] UTF8String]);
        return YES;
    }];
    
    int filesFound = 0;
    for (NSURL* url in enumerator)
	{
        NSNumber *isDirectory = nil;
        [url getResourceValue:&isDirectory forKey:NSURLIsDirectoryKey error:nil];

        if (![isDirectory boolValue])
		{
            continue;
        }

        filesFound += 1;
    }

    *out = NULL;
    *count = 0;
    if (filesFound > 0)
    {
        char** stringList = (char**)tf_malloc(filesFound * sizeof(char*) + filesFound * sizeof(char) * FS_MAX_PATH);
        char* firstString = ((char*)stringList + filesFound * sizeof(char*));
        for (int i = 0; i < filesFound; ++i)
        {
            stringList[i] = firstString + (sizeof(char) * FS_MAX_PATH * i);
        }
        *out = stringList;
        *count = filesFound;
    }

    enumerator = [[NSFileManager defaultManager] enumeratorAtURL:pathURL includingPropertiesForKeys:@[NSURLPathKey, NSURLIsDirectoryKey] options:NSDirectoryEnumerationSkipsSubdirectoryDescendants | NSDirectoryEnumerationProducesRelativePathURLs errorHandler:^BOOL(NSURL * _Nonnull url, NSError * _Nonnull error)
    {
        LOGF(eWARNING, "Error enumerating directory at url %s: %s", [[url path] UTF8String], [[error description] UTF8String]);
        return YES;
    }];

    int strIndex = 0;
    for (NSURL* url in enumerator)
    {
        NSNumber *isDirectory = nil;
        [url getResourceValue:&isDirectory forKey:NSURLIsDirectoryKey error:nil];

        if (![isDirectory boolValue])
        {
            continue;
        }

        char result[FS_MAX_PATH] = {};
        eastl::string filename = [[url lastPathComponent] UTF8String];
        fsAppendPathComponent(subDirectory, filename.c_str(), result);
        char * dest = (*out)[strIndex++];
        strcpy(dest, result);
    }
}

bool fsRemoveFile(const ResourceDirectory resourceDir, const char* fileName)
{
	const char* resourcePath = fsGetResourceDirectory(resourceDir);
	char filePath[FS_MAX_PATH] = {};
	fsAppendPathComponent(resourcePath, fileName, filePath);

	return !remove(filePath);
}

bool fsRenameFile(const ResourceDirectory resourceDir, const char* fileName, const char* newFileName)
{
	const char* resourcePath = fsGetResourceDirectory(resourceDir);
	char filePath[FS_MAX_PATH] = {};
	fsAppendPathComponent(resourcePath, fileName, filePath);

	char newfilePath[FS_MAX_PATH] = {};
	fsAppendPathComponent(resourcePath, newFileName, newfilePath);

	return !rename(filePath, newfilePath);
}

bool fsCopyFile(const ResourceDirectory sourceResourceDir, const char* sourceFileName, const ResourceDirectory destResourceDir, const char* destFileName)
{
	const char* sourceResourcePath = fsGetResourceDirectory(sourceResourceDir);
	const char* destResourcePath = fsGetResourceDirectory(destResourceDir);
	
	char sourceFilePath[FS_MAX_PATH] = {};
	fsAppendPathComponent(sourceResourcePath, sourceFileName, sourceFilePath);
	
	char destFilePath[FS_MAX_PATH] = {};
	fsAppendPathComponent(destResourcePath, destFileName, destFilePath);
	
	NSString *nsSourceFileName = [NSString stringWithUTF8String:sourceFilePath];
	NSString *nsDestFileName = [NSString stringWithUTF8String:destFilePath];
	
	NSFileManager *fileManager = [NSFileManager defaultManager];
	if ([fileManager copyItemAtPath:nsSourceFileName toPath:nsDestFileName  error:NULL])
	{
		return true;
	}
	
	return false;
}

bool fsFileExist(const ResourceDirectory resourceDir, const char* fileName)
{
	const char* resourcePath = fsGetResourceDirectory(resourceDir);
	char filePath[FS_MAX_PATH] = {};
	fsAppendPathComponent(resourcePath, fileName, filePath);
	
	NSFileManager *fileManager = [NSFileManager defaultManager];
	NSString *nsFilePath = [NSString stringWithUTF8String:filePath];
	if([fileManager fileExistsAtPath:nsFilePath ])
		return true;
	return false;
}
