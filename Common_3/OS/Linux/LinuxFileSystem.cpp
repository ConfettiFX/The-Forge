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
#include "../Interfaces/ILog.h"
#include "../Interfaces/IOperatingSystem.h"
#include "../Interfaces/IThread.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <unistd.h>
#include <pwd.h>
#include <fcntl.h>           //for open and O_* enums
#include <linux/limits.h>    //PATH_MAX declaration
#include <dirent.h>

#include "../Interfaces/IMemory.h"

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
	"Audio/",                   // FSR_Audio
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
	char curDir[MAX_PATH];
	getcwd(curDir, sizeof(curDir));
	return eastl::string(curDir);
}

eastl::string get_exe_path()
{
	char exeName[MAX_PATH];
	exeName[0] = 0;
	ssize_t count = readlink("/proc/self/exe", exeName, MAX_PATH);
	exeName[count] = '\0';
	return eastl::string(exeName);
}

eastl::string get_app_prefs_dir(const char* org, const char* app)
{
	const char* homedir;

	if ((homedir = getenv("HOME")) == NULL)
	{
		homedir = getpwuid(getuid())->pw_dir;
	}
	return eastl::string(homedir);
}

eastl::string get_user_documents_dir()
{
	const char* homedir;
	if ((homedir = getenv("HOME")) == NULL)
	{
		homedir = getpwuid(getuid())->pw_dir;
	}
	eastl::string homeString = eastl::string(homedir);
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

void get_files_with_extension(const char* dir, const char* ext, eastl::vector<eastl::string>& filesOut)
{
	eastl::string path = FileSystem::GetNativePath(FileSystem::AddTrailingSlash(dir));

	DIR* directory = opendir(path.c_str());
	if (!directory)
		return;

	eastl::string extension(ext);
	extension.make_lower();
	struct dirent*  entry;
	do
	{
		entry = readdir(directory);
		if (!entry)
			break;

		eastl::string file = entry->d_name;
		file.make_lower();
		if (file.find(extension) != eastl::string::npos)
		{
			file = path + entry->d_name;
			filesOut.push_back(file);
		}

	} while (entry != NULL);

	closedir(directory);
}

void get_sub_directories(const char* dir, eastl::vector<eastl::string>& subDirectoriesOut)
{
	eastl::string path = FileSystem::GetNativePath(FileSystem::AddTrailingSlash(dir));

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
				eastl::string subDirectory = path + entry->d_name;
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
	const eastl::vector<eastl::string>& fileExtensions)
{
	LOGF(LogLevel::eERROR, "Not implemented");
}

void save_file_dialog(
	const char* title, const char* dir, FileDialogCallbackFn callback, void* userData, const char* fileDesc,
	const eastl::vector<eastl::string>& fileExtensions)
{
	LOGF(LogLevel::eERROR, "Not implemented");
}

#include <sys/inotify.h>

struct FileSystem::Watcher::Data
{
	eastl::string mWatchDir;
	uint32_t        mNotifyFilter;
	Callback        mCallback;
	ThreadDesc      mThreadDesc;
	ThreadHandle    mThread;
	volatile int    mRun;
};

static void fswThreadFunc(void* data)
{
	FileSystem::Watcher::Data* fs = (FileSystem::Watcher::Data*)data;

	int  fd, wd;
	char buffer[4096];

	fd = inotify_init();
	if (fd < 0)
	{
		return;
	}

	wd = inotify_add_watch(fd, fs->mWatchDir.c_str(), fs->mNotifyFilter);

	if (wd < 0)
	{
		close(fd);
		return;
	}

	fd_set         rfds;
	struct timeval tv = { 0, 128 << 10 };

	while (fs->mRun)
	{
		FD_ZERO(&rfds);
		FD_SET(fd, &rfds);
		int retval = select(FD_SETSIZE, &rfds, 0, 0, &tv);
		if (retval < 0)
		{
			break;
		}
		if (retval == 0)
		{
			continue;
		}

		int length = read(fd, buffer, sizeof(buffer));
		if (length < 0)
		{
			break;
		}

		size_t offset = 0;
		while (offset < length)
		{
			struct inotify_event* event = (struct inotify_event*)(buffer + offset);
			if (event->len)
			{
				eastl::string path = fs->mWatchDir + event->name;
				if (event->mask & IN_MODIFY)
				{
					fs->mCallback(path.c_str(), FileSystem::Watcher::EVENT_MODIFIED);
				}
				if (event->mask & (IN_ACCESS | IN_OPEN))
				{
					fs->mCallback(path.c_str(), FileSystem::Watcher::EVENT_ACCESSED);
				}
				if (event->mask & (IN_MOVED_TO | IN_CREATE))
				{
					fs->mCallback(path.c_str(), FileSystem::Watcher::EVENT_CREATED);
				}
				if (event->mask & (IN_MOVED_FROM | IN_DELETE))
				{
					fs->mCallback(path.c_str(), FileSystem::Watcher::EVENT_DELETED);
				}
			}
			offset += sizeof(struct inotify_event) + event->len;
		}
	}
	inotify_rm_watch(fd, wd);

	close(fd);
};


FileSystem::Watcher::Watcher(const char* pWatchPath, FSRoot root, uint32_t eventMask, Callback callback)
{

	pData->mWatchDir = FileSystem::FixPath(FileSystem::AddTrailingSlash(pWatchPath), root);
	uint32_t notifyFilter = 0;

	if (eventMask & EVENT_MODIFIED)
	{
		notifyFilter |= IN_MODIFY;
	}
	if (eventMask & EVENT_ACCESSED)
	{
		notifyFilter |= IN_ACCESS | IN_OPEN;
	}
	if (eventMask & EVENT_CREATED)
	{
		notifyFilter |= IN_CREATE | IN_MOVED_TO;
	}
	if (eventMask & EVENT_DELETED)
	{
		notifyFilter |= IN_DELETE | IN_MOVED_FROM;
	}

	pData->mNotifyFilter = notifyFilter;
	pData->mCallback = callback;
	pData->mRun = 1;

	pData->mThreadDesc.pFunc = fswThreadFunc;
	pData->mThreadDesc.pData = pData;

	pData->mThread = create_thread(&pData->mThreadDesc);
}

FileSystem::Watcher::~Watcher()
{
	pData->mRun = 0;
	destroy_thread(pData->mThread);
	conf_delete(pData);
}

#endif
