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

#ifdef __linux__

#include "../Interfaces/IFileSystem.h"
#include "../Interfaces/ILogManager.h"
#include "../Interfaces/IOperatingSystem.h"
#include "../Interfaces/IMemoryManager.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <unistd.h>
#include <pwd.h>
#include <fcntl.h>           //for open and O_* enums
#include <linux/limits.h>    //PATH_MAX declaration
#include <dirent.h>
#define MAX_PATH PATH_MAX

#define RESOURCE_DIR "Shaders/Vulkan"

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
	FILE* fp;
	fp = fopen(filename, flags);
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
	char curDir[MAX_PATH];
	getcwd(curDir, sizeof(curDir));
	return tinystl::string(curDir);
}

tinystl::string get_exe_path()
{
	char exeName[MAX_PATH];
	exeName[0] = 0;
	ssize_t count = readlink("/proc/self/exe", exeName, MAX_PATH);
	exeName[count] = '\0';
	return tinystl::string(exeName);
}

tinystl::string get_app_prefs_dir(const char* org, const char* app)
{
	const char* homedir;

	if ((homedir = getenv("HOME")) == NULL)
	{
		homedir = getpwuid(getuid())->pw_dir;
	}
	return tinystl::string(homedir);
}

tinystl::string get_user_documents_dir()
{
	const char* homedir;
	if ((homedir = getenv("HOME")) == NULL)
	{
		homedir = getpwuid(getuid())->pw_dir;
	}
	tinystl::string homeString = tinystl::string(homedir);
	const char*     doc = "Documents";
	homeString.append(doc, doc + strlen(doc));
	return homeString;
}

void set_current_dir(const char* path)
{
	// change working directory
	// http://man7.org/linux/man-pages/man2/chdir.2.html
	chdir(path);
}

void get_files_with_extension(const char* dir, const char* ext, tinystl::vector<tinystl::string>& filesOut)
{
	tinystl::string path = FileSystem::GetNativePath(FileSystem::AddTrailingSlash(dir));

	DIR* directory = opendir(path.c_str());
	if (!directory)
		return;

	tinystl::string extension(ext);
	struct dirent*  entry;
	do
	{
		entry = readdir(directory);
		if (!entry)
			break;

		tinystl::string file = entry->d_name;
		if (file.find(extension, 0, false) != tinystl::string::npos)
		{
			file = path + file;
			filesOut.push_back(file);
		}

	} while (entry != NULL);

	closedir(directory);
}

void get_sub_directories(const char* dir, tinystl::vector<tinystl::string>& subDirectoriesOut)
{
	tinystl::string path = FileSystem::GetNativePath(FileSystem::AddTrailingSlash(dir));

	DIR* directory = opendir(path.c_str());
	if (!directory)
		return;

	struct dirent* entry;
	do
	{
		entry = readdir(directory);
		if (!entry)
			break;

		if (entry->d_type & DT_DIR)
		{
			if (entry->d_name[0] != '.')
			{
				tinystl::string subDirectory = path + entry->d_name;
				subDirectoriesOut.push_back(subDirectory);
			}
		}

	} while (entry != NULL);

	closedir(directory);
}

bool copy_file(const char* src, const char* dst)
{
	int         source = open(src, O_RDONLY, 0);
	int         dest = open(dst, O_WRONLY);
	struct stat stat_source;
	fstat(source, &stat_source);
	bool ret = sendfile64(dest, source, 0, stat_source.st_size) != -1;
	close(source);
	close(dest);
	return ret;
}

void open_file_dialog(
	const char* title, const char* dir, FileDialogCallbackFn callback, void* userData, const char* fileDesc,
	const tinystl::vector<tinystl::string>& fileExtensions)
{
	LOGERROR("Not implemented");
}

void save_file_dialog(
	const char* title, const char* dir, FileDialogCallbackFn callback, void* userData, const char* fileDesc,
	const tinystl::vector<tinystl::string>& fileExtensions)
{
	LOGERROR("Not implemented");
}

#endif
