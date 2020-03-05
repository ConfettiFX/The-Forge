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

#ifdef __APPLE__

#include <AppKit/AppKit.h>
#include "CocoaFileSystem.h"

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
		Path* path = fsCreatePath(fsGetSystemFileSystem(), paths[i]);
		if ((fswData->mEventMask & FWE_MODIFIED) && (eventFlags[i] & kFSEventStreamEventFlagItemModified))
		{
			fswData->mCallback(path, FWE_MODIFIED);
		}
		if ((fswData->mEventMask & FWE_ACCESSED) && (eventFlags[i] & kFSEventStreamEventFlagItemInodeMetaMod))
		{
			fswData->mCallback(path, FWE_ACCESSED);
		}
		if ((fswData->mEventMask & FWE_CREATED) && (eventFlags[i] & kFSEventStreamEventFlagItemCreated))
		{
			fswData->mCallback(path, FWE_CREATED);
		}
		if ((fswData->mEventMask & FWE_DELETED) && (eventFlags[i] & kFSEventStreamEventFlagItemRemoved))
		{
			fswData->mCallback(path, FWE_DELETED);
		}
	}
};

FileWatcher* fsCreateFileWatcher(const Path* path, FileWatcherEventMask eventMask, FileWatcherCallback callback)
{
	FileWatcher* watcher = conf_new(FileWatcher);
	watcher->mEventMask = eventMask;
	watcher->mCallback = callback;
	CFStringRef          paths[] = { CFStringCreateWithCString(NULL, fsGetPathAsNativeString(path), kCFStringEncodingUTF8) };
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
	conf_delete(fileWatcher);
}

#pragma mark - FileManager Dialogs

void fsShowOpenFileDialog(
	const char* title, const Path* directory, FileDialogCallbackFn callback, void* userData, const char* fileDesc,
	const char** fileExtensions, size_t fileExtensionCount)
{
	NSOpenPanel* openDlg = [NSOpenPanel openPanel];

	if (title)
	{
		[openDlg setMessage:[NSString stringWithUTF8String:title]];
	}

	// Enable the selection of files in the dialog.
	[openDlg setCanChooseFiles:YES];

	// Multiple files not allowed
	[openDlg setAllowsMultipleSelection:NO];

	// Can't select a directory
	[openDlg setCanChooseDirectories:NO];

	if (directory)
	{
		NSString* directoryPath = [NSString stringWithUTF8String:fsGetPathAsNativeString(directory)];
		[openDlg setDirectoryURL:[NSURL fileURLWithPath:directoryPath]];
	}

	if (fileExtensionCount > 0)
	{
		NSMutableArray* extensionsArray = [NSMutableArray arrayWithCapacity:fileExtensionCount];
		for (size_t i = 0; i < fileExtensionCount; i += 1)
		{
			[extensionsArray addObject:[NSString stringWithUTF8String:fileExtensions[i]]];
		}
		[openDlg setAllowedFileTypes:extensionsArray];
	}

	[openDlg beginSheetModalForWindow:[[NSApplication sharedApplication].windows objectAtIndex:0]
					completionHandler:^(NSInteger result) {
						if (result == NSModalResponseOK)
						{
							NSArray<NSURL*>* urls = [openDlg URLs];
							Path*            path = fsCreatePath(fsGetSystemFileSystem(), urls[0].path.UTF8String);
							callback(path, userData);
							fsFreePath(path);
						}
					}];
}

void fsShowSaveFileDialog(
	const char* title, const Path* directory, FileDialogCallbackFn callback, void* userData, const char* fileDesc,
	const char** fileExtensions, size_t fileExtensionCount)
{
	NSSavePanel* saveDlg = [NSSavePanel savePanel];

	if (title)
	{
		[saveDlg setMessage:[NSString stringWithUTF8String:title]];
	}

	if (directory)
	{
		NSString* directoryPath = [NSString stringWithUTF8String:fsGetPathAsNativeString(directory)];
		[saveDlg setDirectoryURL:[NSURL fileURLWithPath:directoryPath]];
	}

	if (fileExtensionCount > 0)
	{
		NSMutableArray* extensionsArray = [NSMutableArray arrayWithCapacity:fileExtensionCount];
		for (size_t i = 0; i < fileExtensionCount; i += 1)
		{
			[extensionsArray addObject:[NSString stringWithUTF8String:fileExtensions[i]]];
		}
		[saveDlg setAllowedFileTypes:extensionsArray];
	}

	[saveDlg beginSheetModalForWindow:[[NSApplication sharedApplication].windows objectAtIndex:0]
					completionHandler:^(NSInteger result) {
						if (result == NSModalResponseOK)
						{
							Path* path = fsCreatePath(fsGetSystemFileSystem(), saveDlg.URL.path.UTF8String);
							callback(path, userData);
							fsFreePath(path);
						}
					}];
}

#endif    // __APPLE__
