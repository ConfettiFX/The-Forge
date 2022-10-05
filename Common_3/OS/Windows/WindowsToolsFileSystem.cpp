/*
 * Copyright (c) 2017-2022 The Forge Interactive Inc.
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

#include "../../Application/Config.h"

#include "shlobj.h"
#include "commdlg.h"

#include "../../Utilities/Interfaces/IToolFileSystem.h"

#include "../../Utilities/Interfaces/ILog.h"
#include "../Interfaces/IOperatingSystem.h"
#include "../../Utilities/Interfaces/IThread.h"
#include "../../Utilities/Interfaces/IMemory.h"

// static
template <typename T>
static inline T withUTF16Path(const char* path, T (*function)(const wchar_t*))
{
	size_t   len = strlen(path);
	wchar_t* buffer = (wchar_t*)alloca((len + 1) * sizeof(wchar_t));

	size_t resultLength = MultiByteToWideChar(CP_UTF8, 0, path, (int)len, buffer, (int)len);
	buffer[resultLength] = 0;

	return function(buffer);
}

#if defined(_WINDOWS) || defined(__APPLE__) || defined(__linux__)
typedef struct FileWatcher
{
	char                mPath[FS_MAX_PATH];
	DWORD               mNotifyFilter;
	FileWatcherCallback mCallback;
	HANDLE              hExitEvt;
	ThreadHandle        mThread;
	volatile int        mRun;
} FileWatcher;

void fswThreadFunc(void* data)
{
	setCurrentThreadName("FileWatcher");
	FileWatcher* fs = (FileWatcher*)data;

	HANDLE hDir = withUTF16Path<HANDLE>(fs->mPath, [](const wchar_t* pathStr) {
		return CreateFileW(
			pathStr, FILE_LIST_DIRECTORY, FILE_SHARE_WRITE | FILE_SHARE_READ | FILE_SHARE_DELETE, NULL, OPEN_EXISTING,
			FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, NULL);
	});
	HANDLE hEvt = ::CreateEvent(NULL, TRUE, FALSE, NULL);

	BYTE       notifyBuffer[1024];
	char       utf8Name[100];
	OVERLAPPED ovl = { 0 };
	ovl.hEvent = hEvt;

	while (fs->mRun)
	{
		DWORD dwBytesReturned = 0;
		ResetEvent(hEvt);
		if (ReadDirectoryChangesW(hDir, &notifyBuffer, sizeof(notifyBuffer), TRUE, fs->mNotifyFilter, NULL, &ovl, NULL) == 0)
		{
			break;
		}

		HANDLE pHandles[2] = { hEvt, fs->hExitEvt };
		WaitForMultipleObjects(2, pHandles, FALSE, INFINITE);

		if (!fs->mRun)
		{
			break;
		}

		GetOverlappedResult(hDir, &ovl, &dwBytesReturned, FALSE);

		DWORD offset = 0;
		BYTE* p = notifyBuffer;
		for (;;)
		{
			FILE_NOTIFY_INFORMATION* fni = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(p);
			uint32_t                 action = 0;
			switch (fni->Action)
			{
				case FILE_ACTION_ADDED:
				case FILE_ACTION_RENAMED_NEW_NAME: action = FWE_CREATED; break;
				case FILE_ACTION_MODIFIED:
					if (fs->mNotifyFilter & FILE_NOTIFY_CHANGE_LAST_WRITE)
						action = FWE_MODIFIED;
					if (fs->mNotifyFilter & FILE_NOTIFY_CHANGE_LAST_ACCESS)
						action = FWE_ACCESSED;
					break;
				case FILE_ACTION_REMOVED:
				case FILE_ACTION_RENAMED_OLD_NAME: action = FWE_DELETED; break;
				default: break;
			}

			memset(utf8Name, 0, sizeof(utf8Name));
			int outputLength = WideCharToMultiByte(
				CP_UTF8, 0, fni->FileName, fni->FileNameLength / sizeof(WCHAR), utf8Name, sizeof(utf8Name) - 1, NULL, NULL);
			if (outputLength == 0)
			{
				continue;
			}

			char fullPathToFile[FS_MAX_PATH] = { 0 };
			strcat(fullPathToFile, fs->mPath);
			strcat(fullPathToFile, "\\");
			strcat(fullPathToFile, utf8Name);

			LOGF(LogLevel::eINFO, "Monitoring activity of file: %s -- Action: %d", fs->mPath, fni->Action);
			fs->mCallback(fullPathToFile, action);

			if (!fni->NextEntryOffset)
				break;
			p += fni->NextEntryOffset;
		}
	}

	CloseHandle(hDir);
	CloseHandle(hEvt);
};

FileWatcher* fsCreateFileWatcher(const char* path, FileWatcherEventMask eventMask, FileWatcherCallback callback)
{
	FileWatcher* watcher = (FileWatcher*)tf_calloc(1, sizeof(FileWatcher));
	//watcher->mWatchDir = fsCopyPath(path);
	watcher->mCallback = callback;
	strcpy(watcher->mPath, path);

	uint32_t notifyFilter = 0;
	if (eventMask & FWE_MODIFIED)
	{
		notifyFilter |= FILE_NOTIFY_CHANGE_LAST_WRITE;
	}
	if (eventMask & FWE_ACCESSED)
	{
		notifyFilter |= FILE_NOTIFY_CHANGE_LAST_ACCESS;
	}
	if (eventMask & (FWE_DELETED | FWE_CREATED))
	{
		notifyFilter |= FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME;
	}

	watcher->mNotifyFilter = notifyFilter;
	watcher->hExitEvt = ::CreateEvent(NULL, TRUE, FALSE, NULL);
	watcher->mCallback = callback;
	watcher->mRun = TRUE;

	ThreadDesc threadDesc = {};
	threadDesc.pFunc = fswThreadFunc;
	threadDesc.pData = watcher;
	strncpy(threadDesc.mThreadName, "FileWatcher", sizeof(threadDesc.mThreadName));
	initThread(&threadDesc, &watcher->mThread);

	return watcher;
}

void fsFreeFileWatcher(FileWatcher* fileWatcher)
{
	fileWatcher->mRun = FALSE;
	SetEvent(fileWatcher->hExitEvt);
	joinThread(fileWatcher->mThread);
	CloseHandle(fileWatcher->hExitEvt);
	tf_free(fileWatcher);
}
#endif

void fsGetFilesWithExtension(
	ResourceDirectory resourceDir, const char* subDirectory, const char* extension, char *** out, int * count)
{
	char directory[FS_MAX_PATH] = {};
	fsAppendPathComponent(fsGetResourceDirectory(resourceDir), subDirectory, directory);

	size_t extensionLen = strlen(extension);
	if (extension[0] == '.')
	{
		extension += 1;
	}

	bool hasPattern = false;
	for (size_t i = 0; i < extensionLen - 1; ++i)
	{
		if (extension[i] == '*' || extension[i] == '.')
		{
			hasPattern = true;
			break;
		}
	}

	uint32_t extensionOffset = (hasPattern) ? 1 : 3;
	size_t   filePathLen = strlen(directory);
	wchar_t* buffer = (wchar_t*)alloca((FS_MAX_PATH) * sizeof(wchar_t));
	size_t   utf16Len = MultiByteToWideChar(CP_UTF8, 0, directory, (int)filePathLen, buffer, (int)filePathLen);

	buffer[utf16Len + 0] = '\\';
	buffer[utf16Len + 1] = '*';
	buffer[utf16Len + 2] = '.';

	for (size_t i = 0; i < extensionLen; ++i)
	{
		buffer[utf16Len + extensionOffset + i] = (wchar_t)extension[i];
	}
	buffer[utf16Len + extensionOffset + extensionLen] = 0;

	*count = 0;

	WIN32_FIND_DATAW fd;
	HANDLE           hFind = ::FindFirstFileW(buffer, &fd);
	if (hFind != INVALID_HANDLE_VALUE)
	{
		int filesFound = 0;
		do
		{
			if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				continue;

			filesFound += 1;
		} while (::FindNextFileW(hFind, &fd));
		::FindClose(hFind);

		char** stringList = (char**)tf_malloc(filesFound * sizeof(char*) + filesFound * sizeof(char) * FS_MAX_PATH);
		char* firstString = ((char*)stringList + filesFound * sizeof(char*));
		for (int i = 0; i < filesFound; ++i)
		{
			stringList[i] = firstString + (sizeof(char) * FS_MAX_PATH * i);
		}
		*out = stringList;
		*count = filesFound;

		hFind = ::FindFirstFileW(buffer, &fd);
		int strIndex = 0;
		do
		{
			if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				continue;

			char utf8Name[FS_MAX_PATH] = {};
			WideCharToMultiByte(CP_UTF8, 0, fd.cFileName, -1, utf8Name, MAX_PATH, NULL, NULL);

			char result[FS_MAX_PATH] = {};
			fsAppendPathComponent(subDirectory, utf8Name, result);

			char * dest = (*out)[strIndex++];
			strcpy(dest, result);
		} while (::FindNextFileW(hFind, &fd));
		::FindClose(hFind);
	}
}

void fsGetSubDirectories(ResourceDirectory resourceDir, const char* subDirectory, char *** out, int * count)
{
	char directory[FS_MAX_PATH] = {};
	fsAppendPathComponent(fsGetResourceDirectory(resourceDir), subDirectory, directory);

	wchar_t buffer[FS_MAX_PATH] = {};
	size_t  utf16Len = mbstowcs(buffer, directory, FS_MAX_PATH);

	buffer[utf16Len + 0] = '\\';
	buffer[utf16Len + 1] = '*';
	buffer[utf16Len + 2] = 0;

	WIN32_FIND_DATAW fd;
	HANDLE           hFind = ::FindFirstFileW(buffer, &fd);
	if (hFind != INVALID_HANDLE_VALUE)
	{
		int filesFound = 0;
		do
		{
			// skip files, ./ and ../
			if (!wcschr(fd.cFileName, '.'))
			{
                filesFound += 1;
			}
		} while (::FindNextFileW(hFind, &fd));
		::FindClose(hFind);

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

            hFind = ::FindFirstFileW(buffer, &fd);
            int strIndex = 0;
            do
            {
                // skip files, ./ and ../
                if (!wcschr(fd.cFileName, '.'))
                {
                    char utf8Name[FS_MAX_PATH] = {};
                    WideCharToMultiByte(CP_UTF8, 0, fd.cFileName, -1, utf8Name, MAX_PATH, NULL, NULL);
                    char result[FS_MAX_PATH] = {};
                    fsAppendPathComponent(subDirectory, utf8Name, result);

                    char * dest = (*out)[strIndex++];
                    strcpy(dest, result);
                }
            } while (::FindNextFileW(hFind, &fd));
            ::FindClose(hFind);
        }
	}
}

bool fsRemoveFile(const ResourceDirectory resourceDir, const char* fileName)
{
	const char* resourcePath = fsGetResourceDirectory(resourceDir);
	char        filePath[FS_MAX_PATH] = {};
	fsAppendPathComponent(resourcePath, fileName, filePath);

	// Path utf-16 conversion
	size_t   filePathLen = strlen(filePath);
	wchar_t* pathStr = (wchar_t*)alloca((filePathLen + 1) * sizeof(wchar_t));
	size_t   pathStrLength = MultiByteToWideChar(CP_UTF8, 0, filePath, (int)filePathLen, pathStr, (int)filePathLen);
	pathStr[pathStrLength] = 0;

	return !_wremove(pathStr);
}

bool fsRenameFile(const ResourceDirectory resourceDir, const char* fileName, const char* newFileName)
{
	const char* resourcePath = fsGetResourceDirectory(resourceDir);
	wchar_t*    pathStr = NULL;
	wchar_t*    newpathStr = NULL;

	{
		char filePath[FS_MAX_PATH] = {};
		fsAppendPathComponent(resourcePath, fileName, filePath);

		// Path utf-16 conversion
		size_t filePathLen = strlen(filePath);
		pathStr = (wchar_t*)alloca((filePathLen + 1) * sizeof(wchar_t));
		size_t pathStrLength = MultiByteToWideChar(CP_UTF8, 0, filePath, (int)filePathLen, pathStr, (int)filePathLen);
		pathStr[pathStrLength] = 0;
	}

	{
		char newfilePath[FS_MAX_PATH] = {};
		fsAppendPathComponent(resourcePath, newFileName, newfilePath);

		// New path utf-16 conversion
		size_t newfilePathLen = strlen(newfilePath);
		newpathStr = (wchar_t*)alloca((newfilePathLen + 1) * sizeof(wchar_t));
		size_t newpathStrLength = MultiByteToWideChar(CP_UTF8, 0, newfilePath, (int)newfilePathLen, newpathStr, (int)newfilePathLen);
		newpathStr[newpathStrLength] = 0;
	}

	return !_wrename(pathStr, newpathStr);
}

bool fsCopyFile(
	const ResourceDirectory sourceResourceDir, const char* sourceFileName, const ResourceDirectory destResourceDir,
	const char* destFileName)
{
	const char* sourceResourcePath = fsGetResourceDirectory(sourceResourceDir);
	const char* destResourcePath = fsGetResourceDirectory(destResourceDir);

	char sourceFilePath[FS_MAX_PATH] = {};
	fsAppendPathComponent(sourceResourcePath, sourceFileName, sourceFilePath);
	// Path utf-16 conversion
	size_t   sourcefilePathLen = strlen(sourceFilePath);
	wchar_t* sourcePathStr = (wchar_t*)alloca((sourcefilePathLen + 1) * sizeof(wchar_t));
	size_t   sourcepathStrLength =
		MultiByteToWideChar(CP_UTF8, 0, sourceFilePath, (int)sourcefilePathLen, sourcePathStr, (int)sourcefilePathLen);
	sourcePathStr[sourcepathStrLength] = 0;

	char destFilePath[FS_MAX_PATH] = {};
	fsAppendPathComponent(destResourcePath, destFileName, destFilePath);
	// Path utf-16 conversion
	size_t   destfilePathLen = strlen(destFilePath);
	wchar_t* destPathStr = (wchar_t*)alloca((destfilePathLen + 1) * sizeof(wchar_t));
	size_t   destpathStrLength = MultiByteToWideChar(CP_UTF8, 0, destFilePath, (int)destfilePathLen, destPathStr, (int)destfilePathLen);
	destPathStr[destpathStrLength] = 0;

	return CopyFileW(sourcePathStr, destPathStr, FALSE);
}

bool fsFileExist(const ResourceDirectory resourceDir, const char* fileName)
{
	const char* resourcePath = fsGetResourceDirectory(resourceDir);
	char        filePath[FS_MAX_PATH] = {};
	fsAppendPathComponent(resourcePath, fileName, filePath);

	// Path utf-16 conversion
	size_t   filePathLen = strlen(filePath);
	wchar_t* pathStr = (wchar_t*)alloca((filePathLen + 1) * sizeof(wchar_t));
	size_t   pathStrLength = MultiByteToWideChar(CP_UTF8, 0, filePath, (int)filePathLen, pathStr, (int)filePathLen);
	pathStr[pathStrLength] = 0;

	struct _stat s = {};
	int          res = _wstat(pathStr, &s);
	if (res != 0)
		return false;
	return true;
}
