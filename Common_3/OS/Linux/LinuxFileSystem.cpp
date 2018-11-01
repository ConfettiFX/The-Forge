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

#ifdef __linux__

#include "../Interfaces/IFileSystem.h"
#include "../Interfaces/ILogManager.h"
#include "../Interfaces/IOperatingSystem.h"
#include "../Interfaces/IMemoryManager.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>
#include <linux/limits.h> //PATH_MAX declaration
#define MAX_PATH PATH_MAX

#define RESOURCE_DIR "Shaders/LINUXVulkan"

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
	"Animation/",							// FSR_Animation
	"",									// FSR_OtherFiles
};


FileHandle _openFile(const char* filename, const char* flags)
{
	FILE* fp;
	fp = fopen(filename, flags);
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
	return fread(buffer, 1, byteCount, (::FILE*)handle);
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

tinystl::string _getCurrentDir()
{
	char curDir[MAX_PATH];
	getcwd(curDir, sizeof(curDir));
	return tinystl::string (curDir);
}

tinystl::string _getExePath()
{
	char exeName[MAX_PATH];
	exeName[0] = 0;
	ssize_t count = readlink( "/proc/self/exe", exeName, MAX_PATH );
	exeName[count] = '\0';
	return tinystl::string(exeName);
}

tinystl::string _getAppPrefsDir(const char *org, const char *app)
{
	const char* homedir;

	if ((homedir = getenv("HOME")) == NULL)
	{
		homedir = getpwuid(getuid())->pw_dir;
	}
	return tinystl::string(homedir);
}

tinystl::string _getUserDocumentsDir()
{
	const char* homedir;
	if ((homedir = getenv("HOME")) == NULL)
	{
		homedir = getpwuid(getuid())->pw_dir;
	}
	tinystl::string homeString = tinystl::string(homedir);
	const char* doc = "Documents";
	homeString.append(doc, doc + strlen(doc));
	return homeString;
}

void _setCurrentDir(const char* path)
{
	// change working directory
	// http://man7.org/linux/man-pages/man2/chdir.2.html
	chdir(path);
}

#endif
