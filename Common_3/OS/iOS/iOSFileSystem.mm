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

FileHandle _openFile(const char* filename, const char* flags)
{
	//first we need to look into main bundle which has read-only access
	NSString * fileUrl = [[NSBundle mainBundle] pathForResource:[NSString stringWithUTF8String:filename] ofType:@""];
	
	//there is no write permission for files in bundle,
	//iOS can only write in documents
	//if 'w' flag is present then look in documents folder of Application (Appdata equivalent)
	//if file has been found in bundle but we want to write to it that means we have to use the one in Documents.
	if(strstr(flags,"w") != NULL) {
		NSString * documentsDirectory = [NSHomeDirectory() stringByAppendingPathComponent:@"Documents"];
		fileUrl = [documentsDirectory stringByAppendingString:[NSString stringWithUTF8String:filename]];
	}
	
	//No point in calling fopen if file path is null
	if(fileUrl == nil)
		return NULL;
	
	filename = [fileUrl fileSystemRepresentation];
	

  FILE* fp = fopen(filename, flags);
  return fp;
}

void _closeFile(FileHandle handle)
{
  fclose((::FILE*)handle);
}

void _flushFile(FileHandle handle)
{
  fflush((::FILE*)handle);
}

size_t _readFile(void *buffer, size_t byteCount, FileHandle handle)
{
  return fread(buffer, byteCount, 1, (::FILE*)handle);
}

bool _seekFile(FileHandle handle, long offset, int origin)
{
  return fseek((::FILE*)handle, offset, origin) == 0;
}

long _tellFile(FileHandle handle)
{
  return ftell((::FILE*)handle);
}

size_t _writeFile(const void *buffer, size_t byteCount, FileHandle handle)
{
  return fwrite(buffer, byteCount, 1, (::FILE*)handle);
}

String _getCurrentDir()
{
    char cwd[256]="";
    getcwd(cwd, sizeof(cwd));
    String str(cwd);
    return str;
}

String _getExePath()
{
    const char* exeDir = [[[[NSBundle mainBundle] bundlePath] stringByDeletingLastPathComponent] cStringUsingEncoding:NSUTF8StringEncoding];
    String str(exeDir);
    return str;
}

size_t _getFileLastModifiedTime(const char* _fileName)
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

String _getAppPrefsDir(const char *org, const char *app)
{
    ASSERT(false && "Unsupported on target iOS");
    return String();
}

String _getUserDocumentsDir()
{
    ASSERT(false && "Unsupported on target iOS");
    return String();
}

void _setCurrentDir(const char* path)
{
    chdir(path);
}
