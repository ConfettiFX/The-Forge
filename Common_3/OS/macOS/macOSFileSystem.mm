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
#include "../Interfaces/ILogManager.h"
#include "../Interfaces/IOperatingSystem.h"
#include "../Interfaces/IMemoryManager.h"

#include <unistd.h>
#include <limits.h>       // for UINT_MAX
#include <sys/stat.h>     // for mkdir
#include <sys/errno.h>    // for errno
#include <dirent.h>

#define RESOURCE_DIR "Shaders/Metal"

const char* pszRoots[FSR_Count] = {
	RESOURCE_DIR "/Binary/",    // FSR_BinShaders
	RESOURCE_DIR "/",           // FSR_SrcShaders
	"Textures/",                // FSR_Textures
	"Meshes/",                  // FSR_Meshes
	"Fonts/",                   // FSR_Builtin_Fonts
	"GPUCfg/",                  // FSR_GpuConfig
	"Animation/",               // FSR_Animation
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

size_t get_file_last_modified_time(const char* _fileName)
{
	struct stat fileInfo;

	if (!stat(_fileName, &fileInfo))
	{
		return (size_t)fileInfo.st_mtime;
	}
	else
	{
		// return an impossible large mod time as the file doesn't exist
		return ~0;
	}
}

tinystl::string get_current_dir()
{
	char cwd[256] = "";
	getcwd(cwd, sizeof(cwd));
	tinystl::string str(cwd);
	return str;
}

tinystl::string get_exe_path()
{
	const char*     exePath = [[[[NSBundle mainBundle] bundlePath] stringByStandardizingPath] cStringUsingEncoding:NSUTF8StringEncoding];
	tinystl::string str(exePath);
	return str;
}

tinystl::string get_app_prefs_dir(const char* org, const char* app)
{
	const char* rawUserPath = [[[[NSFileManager defaultManager] homeDirectoryForCurrentUser] absoluteString] UTF8String];
	const char* path;
	path = strstr(rawUserPath, "/Users/");
	return tinystl::string(path) + tinystl::string("Library/") + tinystl::string(org) + tinystl::string("/") + tinystl::string(app);
}

tinystl::string get_user_documents_dir()
{
	const char* rawUserPath = [[[[NSFileManager defaultManager] homeDirectoryForCurrentUser] absoluteString] UTF8String];
	const char* path;
	path = strstr(rawUserPath, "/Users/");
	return tinystl::string(path);
}

void set_current_dir(const char* path) { chdir(path); }

void get_files_with_extension(const char* dir, const char* ext, tinystl::vector<tinystl::string>& filesOut)
{
	tinystl::string path = FileSystem::GetNativePath(FileSystem::AddTrailingSlash(dir));
	DIR*            pDir = opendir(dir);
	if (!pDir)
	{
		LOGWARNINGF("Could not open directory: %s", dir);
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

void get_sub_directories(const char* dir, tinystl::vector<tinystl::string>& subDirectoriesOut)
{
	tinystl::string path = FileSystem::GetNativePath(FileSystem::AddTrailingSlash(dir));
	DIR*            pDir = opendir(dir);
	if (!pDir)
	{
		LOGWARNINGF("Could not open directory: %s", dir);
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
				tinystl::string subDirectory = path + entry->d_name;
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
		LOGINFOF("Failed to copy file with error : %s", [[error localizedDescription] UTF8String]);
		return false;
	}

	return true;
}

static void FormatFileExtensionsFilter(
	tinystl::string const& fileDesc, tinystl::vector<tinystl::string> const& extFiltersIn, tinystl::string& extFiltersOut)
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
	const tinystl::vector<tinystl::string>& fileExtensions)
{
	tinystl::string extFilter;
	FormatFileExtensionsFilter(fileDesc, fileExtensions, extFilter);

	// Create array of filtered extentions
	NSString* extString = [NSString stringWithCString:extFilter encoding:[NSString defaultCStringEncoding]];
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
	const tinystl::vector<tinystl::string>& fileExtensions)
{
	tinystl::string extFilter;
	FormatFileExtensionsFilter(fileDesc, fileExtensions, extFilter);

	// Create array of filtered extentions
	NSString* extString = [NSString stringWithCString:extFilter encoding:[NSString defaultCStringEncoding]];
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

#endif    // __APPLE__
