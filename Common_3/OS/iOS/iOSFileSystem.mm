/*
 * Copyright (c) 2018 Confetti Interactive Inc.
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

#import <Foundation/Foundation.h>
#include "../Interfaces/IFileSystem.h"
#include "../Interfaces/ILogManager.h"
#include "../Interfaces/IOperatingSystem.h"
#include "../Interfaces/IMemoryManager.h"

#include <sys/stat.h>
#include <unistd.h>

#define RESOURCE_DIR "Shaders/OSXMetal"

const char* pszRoots[FSR_Count] =
{
	RESOURCE_DIR "/Binary/",			// FSR_BinShaders
	RESOURCE_DIR "/",					// FSR_SrcShaders
	RESOURCE_DIR "/Binary/",			// FSR_BinShaders_Common
	RESOURCE_DIR "/",					// FSR_SrcShaders_Common
	"Textures/",						// FSR_Textures
	"Meshes/",							// FSR_Meshes
	"Fonts/",							// FSR_Builtin_Fonts
	"GPUCfg/",							// FSR_GpuConfig
	"Animation/",						// FSR_Animation
	"",									// FSR_OtherFiles
};


//-------------------------------------------------------------------------------------------------------------
// CAUTION using URLs for file system directories!
//
// We should resolve the symlinks:
//
// iOS URLs contain a 'private' folder in the link when debugging on the device.
// Without using URLByResolvingSymlinksInPath below, we would see the following scenario:
// - directoryURL = file:///private/var/...
// - filePathURL  = file:///var/...
// In this case, the substring() logic will fail.
//
//-------------------------------------------------------------------------------------------------------------
FileHandle open_file(const char* filename, const char* flags)
{
#if 0  // OLD IMPLEMENTATION HERE FOR REFERENCE
	//first we need to look into main bundle which has read-only access
	NSString * fileUrl = [[NSBundle mainBundle] pathForResource:[NSString stringWithUTF8String:filename] ofType:@""];

	//there is no write permission for files in bundle,
	//iOS can only write in documents
	//if 'w' flag is present then look in documents folder of Application (Appdata equivalent)
	//if file has been found in bundle but we want to write to it that means we have to use the one in Documents.
	if(strstr(flags,"w") != NULL)
	{
		NSString * documentsDirectory = [NSHomeDirectory() stringByAppendingPathComponent:@"Documents"];
		fileUrl = [documentsDirectory stringByAppendingString:[NSString stringWithUTF8String:filename]];
	}

	//No point in calling fopen if file path is null
	if(fileUrl == nil)
	{
		LOGWARNINGF("Path '%s' is invalid!", filename);
		return NULL;
	}
	
	filename = [fileUrl fileSystemRepresentation];
	//LOGINFOF("Open File: %s", filename); // debug log
#endif
	
	// iOS file system has security measures for directories hence not all directories are writeable by default.
	// The executable directory (the bundle directory) is one of the write-protected directories.
	// iOS file system has pre-defined folders that are write-able. Documents is one of them.
	// Hence, if we request to open a file with write permissions, we will use Documents for that.
	if(strstr(flags,"w") != NULL)
	{
		const tinystl::string currDir = get_current_dir();
		tinystl::string strFileName(filename);
		
		// @filename will have absolute path before being passed into this function.
		// if we want to write, we want to change the directory. Hence, strip away the
		// resolved bundle path (which as prepended to the actual file name the App wants to open)
		// and prepend the 'Documents' folder to the filename
		const unsigned findPos = strFileName.find(currDir, 0);
		if( findPos != tinystl::string::npos )
		{
			strFileName.replace(currDir, "");
		}
		
		NSString * documentsDirectory = [NSHomeDirectory() stringByAppendingPathComponent:@"Documents"];
		filename = [[documentsDirectory stringByAppendingString:[NSString stringWithUTF8String:strFileName.c_str()]] fileSystemRepresentation];
	}
	
	FILE* fp = fopen(filename, flags);
	return fp; // error logging is done from the caller in case fp == NULL
}

void close_file(FileHandle handle)
{
	//ASSERT(FALSE);
	fclose((::FILE*)handle);
}

void flush_file(FileHandle handle)
{// TODO: use NSBundle
	fflush((::FILE*)handle);
}

size_t read_file(void *buffer, size_t byteCount, FileHandle handle)
{// TODO: use NSBundle
	return fread(buffer, 1, byteCount, (::FILE*)handle);
}

bool seek_file(FileHandle handle, long offset, int origin)
{// TODO: use NSBundle
	return fseek((::FILE*)handle, offset, origin) == 0;
}

long tell_file(FileHandle handle)
{// TODO: use NSBundle
	return ftell((::FILE*)handle);
}

size_t write_file(const void *buffer, size_t byteCount, FileHandle handle)
{// TODO: use NSBundle
	return fwrite(buffer, 1, byteCount, (::FILE*)handle);
}

tinystl::string get_current_dir()
{
	return tinystl::string([[[NSBundle mainBundle] bundlePath] cStringUsingEncoding:NSUTF8StringEncoding]);
}

tinystl::string get_exe_path()
{
	const char* exeDir = [[[[NSBundle mainBundle] bundlePath] stringByDeletingLastPathComponent] cStringUsingEncoding:NSUTF8StringEncoding];
	tinystl::string str(exeDir);
	return str;
}

size_t get_file_last_modified_time(const char* _fileName)
{// TODO: use NSBundle
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

tinystl::string get_app_prefs_dir(const char *org, const char *app)
{
	ASSERT(false && "Unsupported on target iOS");
	return tinystl::string();
}

tinystl::string get_user_documents_dir()
{
	ASSERT(false && "Unsupported on target iOS");
	return tinystl::string();
}

void set_current_dir(const char* path)
{// TODO: use NSBundle
	chdir(path);
}


void get_files_with_extension(const char* dir, const char* ext, tinystl::vector<tinystl::string>& filesIn)
{
	if(!dir || strlen(dir) == 0)
	{
		LOGWARNINGF("%s directory passed as argument!", (!dir ? "NULL" : "Empty"));
		return;
	}
	
	// Use Paths instead of URLs
	NSFileManager* fileMan = [NSFileManager defaultManager];
	NSString* pStrDir = [NSString stringWithUTF8String:dir];
	NSArray* pathFragments = [NSArray arrayWithObjects:[[NSBundle mainBundle] bundlePath], pStrDir, nil];
	NSString* pStrSearchDir = [NSString pathWithComponents:pathFragments];
	
	BOOL isDir = YES;
	if(![fileMan fileExistsAtPath:pStrSearchDir isDirectory:&isDir])
	{
		LOGERRORF("Directory '%s' doesn't exist.", dir);
		return;
	}
	NSArray* pContents = [fileMan subpathsAtPath:pStrSearchDir];
	
	const char* extension = ext;
	if(ext[0] == '.')
	{
		extension = ext + 1; // get the next address after '.' 
	}
	
	// predicate for querying files with extension 'ext'
	NSString * formattedPredicate = @"pathExtension == '";
	formattedPredicate = [formattedPredicate stringByAppendingString:[NSString stringWithUTF8String: extension]];
	formattedPredicate = [formattedPredicate stringByAppendingString:@"'"];
	NSPredicate* filePredicate = [NSPredicate predicateWithFormat:formattedPredicate];
	
	// save the files with the given extension in the output container
	for (NSString* filePath in [pContents filteredArrayUsingPredicate:filePredicate])
	{
		pathFragments = [NSArray arrayWithObjects:pStrDir, filePath, nil];
		NSString* fullFilePath = [NSString pathWithComponents:pathFragments];
		filesIn.push_back([fullFilePath cStringUsingEncoding:NSUTF8StringEncoding]);
	}
}

bool file_exists(const char* fileFullPath)
{
	NSFileManager *fileMan = [NSFileManager defaultManager];
	BOOL isDir = NO;
	BOOL fileExists = [fileMan fileExistsAtPath:[NSString stringWithUTF8String:fileFullPath] isDirectory:&isDir];
	return fileExists ? true : false;
}

bool absolute_path(const char* fileFullPath)
{
	return (([NSString stringWithUTF8String:fileFullPath].absolutePath == YES) ? true : false);
}

bool copy_file(const char* src, const char* dst)
{
	NSError* error = nil;
	if (NO == [[NSFileManager defaultManager] copyItemAtPath:[NSString stringWithUTF8String:src] toPath:[NSString stringWithUTF8String:dst] error:&error])
	{
		LOGINFOF("Failed to copy file with error : %s", [[error localizedDescription] UTF8String]);
		return false;
	}
	
	return true;
}
