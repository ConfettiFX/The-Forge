/*
 * Copyright (c) 2018-2019 Confetti Interactive Inc.
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

#import <Foundation/Foundation.h>
#include <AppKit/NSOpenPanel.h>

#include "../Interfaces/IFileSystem.h"
#include "../Interfaces/ILog.h"
#include "../Interfaces/IOperatingSystem.h"

#include <unistd.h>
#include <limits.h>       // for UINT_MAX
#include <sys/stat.h>     // for mkdir
#include <sys/errno.h>    // for errno
#include <dirent.h>

#include "../Interfaces/IMemory.h"

#define RESOURCE_DIR "Shaders/Metal"

const char* pszRoots[FSR_Count] = {
	RESOURCE_DIR "/Binary/",    // FSR_BinShaders
	RESOURCE_DIR "/",           // FSR_SrcShaders
	"Textures/",                // FSR_Textures
	"Meshes/",                  // FSR_Meshes
	"Fonts/",                   // FSR_Builtin_Fonts
	"GPUCfg/",                  // FSR_GpuConfig
	"Animation/",               // FSR_Animation
	"Audio/",                   // FSR_Audio
	"",                         // FSR_OtherFiles
};

FileHandle open_file(const char* filename, const char* flags)
{
	FILE* fp = fopen(filename, flags);
	return fp;
}

bool close_file(FileHandle handle) { return (fclose((::FILE*)handle) == 0); }

void flush_file(FileHandle handle) { fflush((::FILE*)handle); }

size_t read_file(void* buffer, size_t byteCount, FileHandle handle) { return fread(buffer, 1, byteCount, (::FILE*)handle); }

bool seek_file(FileHandle handle, long offset, int origin) { return fseek((::FILE*)handle, offset, origin) == 0; }

long tell_file(FileHandle handle) { return ftell((::FILE*)handle); }

size_t write_file(const void* buffer, size_t byteCount, FileHandle handle) { return fwrite(buffer, 1, byteCount, (::FILE*)handle); }

time_t get_file_last_modified_time(const char* _fileName)
{
	struct stat fileInfo = {0};

	stat(_fileName, &fileInfo);
	return fileInfo.st_mtime;
}

time_t get_file_last_accessed_time(const char* _fileName)
{
	struct stat fileInfo = {0};

	stat(_fileName, &fileInfo);
	return fileInfo.st_atime;
}

time_t get_file_creation_time(const char* _fileName)
{
	struct stat fileInfo = {0};

	stat(_fileName, &fileInfo);
	return fileInfo.st_ctime;
}

eastl::string get_current_dir()
{
	char cwd[256] = "";
	getcwd(cwd, sizeof(cwd));
	eastl::string str(cwd);
	return str;
}

eastl::string get_exe_path()
{
	const char*     exePath = [[[[NSBundle mainBundle] bundlePath] stringByStandardizingPath] cStringUsingEncoding:NSUTF8StringEncoding];
	eastl::string str(exePath);
	return str;
}

eastl::string get_app_prefs_dir(const char* org, const char* app)
{
	const char* rawUserPath = [[[[NSFileManager defaultManager] homeDirectoryForCurrentUser] absoluteString] UTF8String];
	const char* path;
	path = strstr(rawUserPath, "/Users/");
	return eastl::string(path) + eastl::string("Library/") + eastl::string(org) + eastl::string("/") + eastl::string(app);
}

eastl::string get_user_documents_dir()
{
	const char* rawUserPath = [[[[NSFileManager defaultManager] homeDirectoryForCurrentUser] absoluteString] UTF8String];
	const char* path;
	path = strstr(rawUserPath, "/Users/");
	return eastl::string(path);
}

void set_current_dir(const char* path) { chdir(path); }

void get_files_with_extension(const char* dir, const char* ext, eastl::vector<eastl::string>& filesOut)
{
	eastl::string path = FileSystem::GetNativePath(FileSystem::AddTrailingSlash(dir));
	DIR*            pDir = opendir(dir);
	if (!pDir)
	{
        LOGF(LogLevel::eWARNING, "Could not open directory: %s", dir);
		return;
	}

	// recursively search the directory for files with given extension
	dirent* entry = NULL;
	while ((entry = readdir(pDir)) != NULL)
	{
		if (FileSystem::GetExtension(entry->d_name) == ext)
		{
			filesOut.push_back(path + entry->d_name);
		}
	}

	closedir(pDir);
}

void get_sub_directories(const char* dir, eastl::vector<eastl::string>& subDirectoriesOut)
{
	eastl::string path = FileSystem::GetNativePath(FileSystem::AddTrailingSlash(dir));
	DIR*            pDir = opendir(dir);
	if (!pDir)
	{
		LOGF(LogLevel::eWARNING, "Could not open directory: %s", dir);
		return;
	}

	// recursively search the directory for files with given extension
	dirent* entry = NULL;
	while ((entry = readdir(pDir)) != NULL)
	{
		if (entry->d_type & DT_DIR)
		{
			if (entry->d_name[0] != '.')
			{
				eastl::string subDirectory = path + entry->d_name;
				subDirectoriesOut.push_back(subDirectory);
			}
		}
	}

	closedir(pDir);
}

bool absolute_path(const char* fileFullPath) { return (([NSString stringWithUTF8String:fileFullPath].absolutePath == YES) ? true : false); }

bool copy_file(const char* src, const char* dst)
{
	NSError* error = nil;
	if (NO == [[NSFileManager defaultManager] copyItemAtPath:[NSString stringWithUTF8String:src]
													  toPath:[NSString stringWithUTF8String:dst]
													   error:&error])
	{
        LOGF(LogLevel::eINFO, "Failed to copy file with error : %s", [[error localizedDescription] UTF8String]);
		return false;
	}

	return true;
}

static void FormatFileExtensionsFilter(
	eastl::string const& fileDesc, eastl::vector<eastl::string> const& extFiltersIn, eastl::string& extFiltersOut)
{
	extFiltersOut = "";
	for (size_t i = 0; i < extFiltersIn.size(); ++i)
	{
		extFiltersOut += extFiltersIn[i];
		if (i != extFiltersIn.size() - 1)
			extFiltersOut += ";";
	}
}

void open_file_dialog(
	const char* title, const char* dir, FileDialogCallbackFn callback, void* userData, const char* fileDesc,
	const eastl::vector<eastl::string>& fileExtensions)
{
	eastl::string extFilter;
	FormatFileExtensionsFilter(fileDesc, fileExtensions, extFilter);

	// Create array of filtered extentions
	NSString* extString = [NSString stringWithCString:extFilter.c_str() encoding:[NSString defaultCStringEncoding]];
	NSArray*  extList = [extString componentsSeparatedByString:@";"];

	NSString* objcString = [NSString stringWithCString:title encoding:[NSString defaultCStringEncoding]];
	NSString* objcURL = [NSString stringWithCString:dir encoding:[NSString defaultCStringEncoding]];
	NSURL*    nsURL = [NSURL URLWithString:objcURL];

	// Create the File Open Dialog class.
	NSOpenPanel* openDlg = [NSOpenPanel openPanel];

	// Enable the selection of files in the dialog.
	[openDlg setCanChooseFiles:YES];

	// Multiple files not allowed
	[openDlg setAllowsMultipleSelection:NO];

	// Can't select a directory
	[openDlg setCanChooseDirectories:NO];
	[openDlg setMessage:objcString];
	[openDlg setDirectoryURL:nsURL];

	// Extention filtering
	[openDlg setAllowedFileTypes:extList];

	[openDlg beginSheetModalForWindow:[[NSApplication sharedApplication].windows objectAtIndex:0]
					completionHandler:^(NSInteger result) {
						if (result == NSModalResponseOK)
						{
							NSArray*  urls = [openDlg URLs];
							NSString* url = [urls objectAtIndex:0];
							callback(url.fileSystemRepresentation, userData);
						}
					}];
}

void save_file_dialog(
	const char* title, const char* dir, FileDialogCallbackFn callback, void* userData, const char* fileDesc,
	const eastl::vector<eastl::string>& fileExtensions)
{
	eastl::string extFilter;
	FormatFileExtensionsFilter(fileDesc, fileExtensions, extFilter);

	// Create array of filtered extentions
	NSString* extString = [NSString stringWithCString:extFilter.c_str() encoding:[NSString defaultCStringEncoding]];
	NSArray*  extList = [extString componentsSeparatedByString:@";"];

	NSString* objcString = [NSString stringWithCString:title encoding:[NSString defaultCStringEncoding]];
	NSString* objcURL = [NSString stringWithCString:dir encoding:[NSString defaultCStringEncoding]];
	NSURL*    nsURL = [NSURL URLWithString:objcURL];

	// Create the File Open Dialog class.
	NSSavePanel* saveDlg = [NSSavePanel savePanel];

	// Can't select a directory
	[saveDlg setMessage:objcString];
	[saveDlg setDirectoryURL:nsURL];

	// Extention filtering
	[saveDlg setAllowedFileTypes:extList];

	[saveDlg beginSheetModalForWindow:[[NSApplication sharedApplication].windows objectAtIndex:0]
					completionHandler:^(NSInteger result) {
						if (result == NSModalResponseOK)
						{
							NSURL* url = [saveDlg URL];
							callback(url.fileSystemRepresentation, userData);
						}
					}];
}

struct FileSystem::Watcher::Data
{
	Callback         mCallback;
	uint32_t         mEventMask;
	FSEventStreamRef mStream;
};

static void fswCbFunc(ConstFSEventStreamRef streamRef, void* data, size_t numEvents, void* eventPaths, const FSEventStreamEventFlags eventFlags[], const FSEventStreamEventId eventIds[])
{
	FileSystem::Watcher::Data* fswData = (FileSystem::Watcher::Data*)data;
	const char** paths = (const char**)eventPaths;
	
	for (size_t i=0; i < numEvents; ++i)
	{
		if ((fswData->mEventMask & FileSystem::Watcher::EVENT_MODIFIED) && (eventFlags[i]&kFSEventStreamEventFlagItemModified))
		{
			fswData->mCallback(paths[i], FileSystem::Watcher::EVENT_MODIFIED);
		}
		if ((fswData->mEventMask & FileSystem::Watcher::EVENT_ACCESSED) && (eventFlags[i]&kFSEventStreamEventFlagItemInodeMetaMod))
		{
			fswData->mCallback(paths[i], FileSystem::Watcher::EVENT_ACCESSED);
		}
		if ((fswData->mEventMask & FileSystem::Watcher::EVENT_CREATED) && (eventFlags[i]&kFSEventStreamEventFlagItemCreated))
		{
			fswData->mCallback(paths[i], FileSystem::Watcher::EVENT_CREATED);
		}
		if ((fswData->mEventMask & FileSystem::Watcher::EVENT_DELETED) && (eventFlags[i]&kFSEventStreamEventFlagItemRemoved))
		{
			fswData->mCallback(paths[i], FileSystem::Watcher::EVENT_DELETED);
		}
	}
};

FileSystem::Watcher::Watcher(const char* pWatchPath, FSRoot root, uint32_t eventMask, Callback callback)
{
	pData = conf_new(FileSystem::Watcher::Data);
	pData->mEventMask = eventMask;
	pData->mCallback = callback;
	CFStringRef paths[] = {CFStringCreateWithCString(NULL, pWatchPath, kCFStringEncodingUTF8)};
	CFArrayRef pathsToWatch = CFArrayCreate(NULL, (const void **)paths, 1, &kCFTypeArrayCallBacks);
	CFAbsoluteTime latency = 0.125;   // in seconds
	FSEventStreamContext context = {0, pData, NULL, NULL, NULL};
	pData->mStream = FSEventStreamCreate(NULL, fswCbFunc, &context, pathsToWatch, kFSEventStreamEventIdSinceNow, latency, kFSEventStreamCreateFlagFileEvents);
	CFRelease(pathsToWatch);
	CFRelease(paths[0]);
	if (pData->mStream)
	{
		FSEventStreamScheduleWithRunLoop(pData->mStream, CFRunLoopGetMain(), kCFRunLoopDefaultMode);
		if (!FSEventStreamStart(pData->mStream))
		{
			FSEventStreamInvalidate(pData->mStream);
			FSEventStreamRelease(pData->mStream);
		}
	}
}

FileSystem::Watcher::~Watcher()
{
	if (pData->mStream)
	{
		FSEventStreamStop(pData->mStream);
		FSEventStreamInvalidate(pData->mStream);
		FSEventStreamRelease(pData->mStream);
	}
	conf_delete(pData);
}

#endif    // __APPLE__
