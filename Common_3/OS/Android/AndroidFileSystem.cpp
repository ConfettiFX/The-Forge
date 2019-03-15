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

#ifdef __ANDROID__
#include "../Interfaces/IFileSystem.h"
#include "../Interfaces/ILogManager.h"
#include "../Interfaces/IOperatingSystem.h"
#include "../Interfaces/IMemoryManager.h"
#include <unistd.h>
#include <android/asset_manager.h>
#define MAX_PATH PATH_MAX

#define RESOURCE_DIR "Shaders"

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

static AAssetManager* _mgr = NULL;

FileHandle open_file(const char* filename, const char* flags)
{
	// Android does not support write to file. All assets accessed through asset manager are read only.
	if(strstr(flags, "w") != NULL)
	{
		LOGERROR("Writing to asset file is not supported on android platform!");
		return NULL;
	}
	if(_mgr == NULL)
		return NULL;

	AAsset* file = AAssetManager_open(_mgr,
		filename, AASSET_MODE_BUFFER);

	if(_mgr == NULL) return NULL;

	return reinterpret_cast<void*>(file);
}

bool close_file(FileHandle handle)
{
	AAsset_close(reinterpret_cast<AAsset*>(handle));
	return true;
}

void flush_file(FileHandle handle)
{
	LOGERROR("FileSystem::Flush not supported on Android!");
	abort();
}

size_t read_file(void *buffer, size_t byteCount, FileHandle handle)
{
	AAsset* assetHandle = reinterpret_cast<AAsset*>(handle);
	size_t  readSize = AAsset_read(assetHandle, buffer, byteCount);
	ASSERT(readSize == byteCount);
	return readSize;
}

bool seek_file(FileHandle handle, long offset, int origin)
{
	// Seek function return -s on error.
	return AAsset_seek(reinterpret_cast<AAsset*>(handle), offset, origin) != -1;
}

long tell_file(FileHandle handle)
{
	size_t total_len = AAsset_getLength(reinterpret_cast<AAsset*>(handle));
	size_t remain_len = AAsset_getRemainingLength(reinterpret_cast<AAsset*>(handle));
	return total_len - remain_len;
	//AAsset_getLength(reinterpret_cast<AAsset*>(handle));
}

size_t write_file(const void *buffer, size_t byteCount, FileHandle handle)
{
	//It cannot be done.It is impossible.
	//https://stackoverflow.com/questions/3760626/how-to-write-files-to-assets-folder-or-raw-folder-in-android
	LOGERROR("FileSystem::Write not supported in Android!");
	abort();
	return -1;
}

size_t get_file_last_modified_time(const char* _fileName)
{
	LOGERROR("FileSystem::Last Modified Time not supported in Android!");
	return -1;
}

tinystl::string get_current_dir()
{
	return tinystl::string ("");
}

tinystl::string get_exe_path()
{
	char exeName[MAX_PATH];
	exeName[0] = 0;
	readlink("/proc/self/exe", exeName, MAX_PATH);
	return tinystl::string(exeName);
}

tinystl::string get_app_prefs_dir(const char *org, const char *app)
{
	return "";
}

tinystl::string get_user_documents_dir()
{
	return "";
}

void set_current_dir(const char* path)
{
	// change working directory
	chdir(path);
}

bool copy_file(const char* src, const char* dst)
{
	LOGERROR("Not supported in Android!");
	return false;
}

void open_file_dialog(
	const char* title, const char* dir, FileDialogCallbackFn callback, void* userData, const char* fileDesc,
	const tinystl::vector<tinystl::string>& fileExtensions)
{
	LOGERROR("Not supported in Android!");
}

void save_file_dialog(
	const char* title, const char* dir, FileDialogCallbackFn callback, void* userData, const char* fileDesc,
	const tinystl::vector<tinystl::string>& fileExtensions)
{
	LOGERROR("Not supported in Android!");
}

#endif
