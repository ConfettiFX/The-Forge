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

#ifdef _WIN32

#include <functional>

#if !defined(XBOX)
#include "shlobj.h"
#include "commdlg.h"
#include <WinBase.h>
#endif

#include "../FileSystem/FileSystemInternal.h"
#include "../FileSystem/SystemFileStream.h"

#include "../Interfaces/ILog.h"
#include "../Interfaces/IOperatingSystem.h"
#include "../Interfaces/IThread.h"
#include "../Interfaces/IMemory.h"

template <typename T>
static inline T withUTF16Path(const Path* path, T (*function)(const wchar_t*))
{
	wchar_t* buffer = (wchar_t*)alloca((path->mPathLength + 1) * sizeof(wchar_t));

	size_t resultLength =
		MultiByteToWideChar(CP_UTF8, 0, fsGetPathAsNativeString(path), (int)path->mPathLength, buffer, (int)path->mPathLength);
	buffer[resultLength] = 0;

	return function(buffer);
}

static Path* fsCreatePathFromWideString(const wchar_t* systemPath, size_t systemPathLength)
{
	int   utf8Length = WideCharToMultiByte(CP_UTF8, 0, systemPath, (int)systemPathLength, NULL, 0, NULL, NULL);
	char* utf8Path = (char*)alloca((utf8Length + 1) * sizeof(char));
	WideCharToMultiByte(CP_UTF8, 0, systemPath, (int)systemPathLength, utf8Path, utf8Length, NULL, NULL);
	utf8Path[utf8Length] = 0;
	return fsCreatePath(fsGetSystemFileSystem(), utf8Path);
}

struct WindowsFileSystem: public FileSystem
{
	WindowsFileSystem(): FileSystem(FSK_SYSTEM) {}

	bool IsReadOnly() const override { return false; }

	bool IsCaseSensitive() const override { return false; }

	char GetPathDirectorySeparator() const override { return '\\'; }

	size_t GetDefaultRootPathLength() const override
	{
		return 3;    // e.g. { 'C', ':', '\\' }
	}
	
	size_t GetRootPathLength(const Path * path) const override
	{
		// Allocate a stack buffer for the base path.
		struct
		{
			Path path;
			char pathBuffer[16];
		} stackPath = {};

		const char * absolutePathString = fsGetPathAsNativeString(path);

		size_t pathComponentOffset;
		if (!FormRootPath(absolutePathString, &stackPath.path, &pathComponentOffset))
		{
			LOGF(LogLevel::eERROR, "Path string '%s' is not an absolute path", absolutePathString);
			return NULL;
		}

		return pathComponentOffset;    // e.g. { 'C', ':', '\\' } ir { '\\', '\\' }
	}

	/// Fills path's buffer with the canonical root path corresponding to the root of absolutePathString,
	/// and returns an offset into absolutePathString containing the path component after the root by pathComponentOffset.
	/// path is assumed to have storage for up to 16 characters.
	bool FormRootPath(const char* absolutePathString, Path* path, size_t* pathComponentOffset) const override
	{
		if (absolutePathString[0] == '\\' && absolutePathString[1] == '\\') { // Network Paths
			(&path->mPathBufferOffset)[0] = '\\';
			(&path->mPathBufferOffset)[1] = '\\';
			(&path->mPathBufferOffset)[2] = 0;
			path->mPathLength = 2;
			*pathComponentOffset = 1;    // After the :\ in the drive name.
			return true;
		} else { // Normal Windows Paths
			// Find the colon after the drive letter.
			size_t colonOffset = 1;
			for (;;)
			{
				if (absolutePathString[colonOffset] == 0)
				{
					return false;
				}
				if (absolutePathString[colonOffset] == ':')
				{
					break;
				}
				colonOffset += 1;
			}

			if (absolutePathString[colonOffset + 1] != '\\' && absolutePathString[colonOffset + 1] != '/')
			{
				return false;
			}

			char driveLetter = absolutePathString[colonOffset - 1];
			(&path->mPathBufferOffset)[0] = driveLetter;
			(&path->mPathBufferOffset)[1] = ':';
			(&path->mPathBufferOffset)[2] = '\\';
			(&path->mPathBufferOffset)[3] = 0;
			path->mPathLength = 3;

			*pathComponentOffset = colonOffset + 2;    // After the :\ in the drive name.

			return true;
		}

	}

	time_t GetCreationTime(const Path* filePath) const override
	{
		struct stat fileInfo = { 0 };

		stat(fsGetPathAsNativeString(filePath), &fileInfo);
		return fileInfo.st_ctime;
	}

	time_t GetLastAccessedTime(const Path* filePath) const override
	{
		const char* nativePath = fsGetPathAsNativeString( filePath );

		// Fix paths for Windows 7 - needs to be generalized and propagated
		eastl::string path = eastl::string( nativePath );
		auto directoryPos = path.find( ":" );
		eastl::string cleanPath = path.substr( directoryPos - 1 );

		struct stat fileInfo = { 0 };
		stat( cleanPath.c_str(), &fileInfo );
		return fileInfo.st_atime;
	}

	time_t GetLastModifiedTime(const Path* filePath) const override
	{
		const char* nativePath = fsGetPathAsNativeString( filePath );

		// Fix paths for Windows 7 - needs to be generalized and propagated
		eastl::string path = eastl::string( nativePath );
		auto directoryPos = path.find( ":" );
		eastl::string cleanPath = path.substr( directoryPos - 1 );

		struct stat fileInfo = { 0 };
		stat( cleanPath.c_str(), &fileInfo);
		return fileInfo.st_mtime;
	}

	bool CreateDirectory(const Path* directoryPath) const override
	{
		if (Path* parentPath = fsGetParentPath(directoryPath))
		{
			if (!FileExists(parentPath))
			{
				CreateDirectory(parentPath);
			}
			fsFreePath(parentPath);
		}

		return withUTF16Path<bool>(directoryPath, [](const wchar_t* pathStr) { return ::CreateDirectoryW(pathStr, NULL) ? true : false; });
	}

	bool DeleteFile(const Path* path) const override
	{
		return withUTF16Path<bool>(path, [](const wchar_t* pathStr) { return ::DeleteFileW(pathStr) ? true : false; });
	}

	bool FileExists(const Path* path) const override
	{
		return withUTF16Path<bool>(path, [](const wchar_t* pathStr) {
			DWORD attributes = GetFileAttributesW(pathStr);
			return attributes != INVALID_FILE_ATTRIBUTES;
		});
	}

	bool IsDirectory(const Path* path) const override
	{
		return withUTF16Path<bool>(path, [](const wchar_t* pathStr) {
			DWORD attributes = GetFileAttributesW(pathStr);
			return (attributes != INVALID_FILE_ATTRIBUTES &&  (attributes & FILE_ATTRIBUTE_DIRECTORY));
		});
	}

	FileStream* OpenFile(const Path* filePath, FileMode mode) const override
	{
		FILE* fp;
		
		// Path utf-16 conversion
		wchar_t* pathStr = (wchar_t*)alloca((filePath->mPathLength + 1) * sizeof(wchar_t));
		size_t pathStrLength =
			MultiByteToWideChar(CP_UTF8, 0, fsGetPathAsNativeString(filePath), (int)filePath->mPathLength, pathStr, (int)filePath->mPathLength);
		pathStr[pathStrLength] = 0;
		
		// Mode string utf-16 conversion
		const char * modeStr = fsFileModeToString(mode);
		size_t len = strlen(modeStr);
		wchar_t* modeWStr = (wchar_t*)alloca((len + 1) * sizeof(wchar_t));
		size_t modeStrLength =
			MultiByteToWideChar(CP_UTF8, 0, modeStr, (int)len, modeWStr, (int)len);
		modeWStr[modeStrLength] = 0;

		if (0 != (mode & FM_ALLOW_READ)) {
			fp = _wfsopen(pathStr, modeWStr, _SH_DENYWR);
		} else {
			_wfopen_s(&fp, pathStr, modeWStr);
		}

		if (fp)
		{
			return conf_new(SystemFileStream, fp, mode, filePath);
		}
		else
		{
			LOGF(LogLevel::eERROR, "Error opening file: %s -- %s (error: %s)", fsGetPathAsNativeString(filePath), fsFileModeToString(mode), strerror(errno));
		}
		return NULL;
	}

	bool CopyFile(const Path* sourcePath, const Path* destinationPath, bool overwriteIfExists) const override
	{
		if (!overwriteIfExists && FileExists(destinationPath))
		{
			return false;
		}

		// Source Path utf-16 conversion
		wchar_t* sourcePathStr = (wchar_t*)alloca((sourcePath->mPathLength + 1) * sizeof(wchar_t));
		size_t sourcePathStrLength =
			MultiByteToWideChar(CP_UTF8, 0, fsGetPathAsNativeString(sourcePath), (int)sourcePath->mPathLength, sourcePathStr, (int)sourcePath->mPathLength);
		sourcePathStr[sourcePathStrLength] = 0;
		// Destination Path utf-16 conversion
		wchar_t* destinationPathStr = (wchar_t*)alloca((destinationPath->mPathLength + 1) * sizeof(wchar_t));
		size_t destinationPathStrLength =
			MultiByteToWideChar(CP_UTF8, 0, fsGetPathAsNativeString(destinationPath), (int)destinationPath->mPathLength, destinationPathStr, (int)destinationPath->mPathLength);
		destinationPathStr[destinationPathStrLength] = 0;

		return ::CopyFileW(sourcePathStr, destinationPathStr, !overwriteIfExists) ? true : false;
	}

	void EnumerateFilesWithExtension(
		const Path* directory, const char* extension, bool (*processFile)(const Path*, void* userData), void* userData) const override
	{
		size_t extensionLen = strlen(extension);
		if (extension[0] == '*') {
			extension += 1;
		}
		if (extension[0] == '.') {
			extension += 1;
		}

		bool hasPattern = false;
		for(size_t i = 0; i < extensionLen - 1; ++i) {
			if(extension[i] == '*' || extension[i] == '.') {
				hasPattern = true;
				break;
			}
		}

		wchar_t* buffer = (wchar_t*)alloca((directory->mPathLength + 4 + extensionLen) * sizeof(wchar_t));

		size_t utf16Len = MultiByteToWideChar(
			CP_UTF8, 0, fsGetPathAsNativeString(directory), (int)directory->mPathLength, buffer, (int)directory->mPathLength);

		buffer[utf16Len + 0] = '\\';
		buffer[utf16Len + 1] = '*';
		buffer[utf16Len + 2] = '.';
		int extensionOffset = (hasPattern) ? 1 : 3;
		for (size_t i = 0; i < extensionLen; i += 1)
		{
			buffer[utf16Len + extensionOffset + i] = (wchar_t)extension[i];
		}
		buffer[utf16Len + extensionOffset + extensionLen] = 0;

		WIN32_FIND_DATAW fd;
		HANDLE           hFind = ::FindFirstFileW(buffer, &fd);
		if (hFind != INVALID_HANDLE_VALUE)
		{
			do
			{
				char utf8Name[2 * MAX_PATH];
				WideCharToMultiByte(CP_UTF8, 0, fd.cFileName, -1, utf8Name, 2 * MAX_PATH, NULL, NULL);

				Path* path = fsAppendPathComponent(directory, utf8Name);
				processFile(path, userData);
				fsFreePath(path);
			} while (::FindNextFileW(hFind, &fd));
			::FindClose(hFind);
		}
	}

	void
		EnumerateSubDirectories(const Path* directory, bool (*processDirectory)(const Path*, void* userData), void* userData) const override
	{
		wchar_t* buffer = (wchar_t*)alloca((directory->mPathLength + 3) * sizeof(wchar_t));

		size_t utf16Len = MultiByteToWideChar(
			CP_UTF8, 0, fsGetPathAsNativeString(directory), (int)directory->mPathLength, buffer, (int)directory->mPathLength);

		buffer[utf16Len + 0] = '\\';
		buffer[utf16Len + 1] = '*';
		buffer[utf16Len + 2] = 0;

		WIN32_FIND_DATAW fd;
		HANDLE           hFind = ::FindFirstFileW(buffer, &fd);
		if (hFind != INVALID_HANDLE_VALUE)
		{
			do
			{
				// skip files, ./ and ../
				if (!wcschr(fd.cFileName, '.'))
				{
					char utf8Name[2 * MAX_PATH];
					WideCharToMultiByte(CP_UTF8, 0, fd.cFileName, -1, utf8Name, 2 * MAX_PATH, NULL, NULL);

					Path* path = fsAppendPathComponent(directory, utf8Name);
					processDirectory(path, userData);
					fsFreePath(path);
				}
			} while (::FindNextFileW(hFind, &fd));
			::FindClose(hFind);
		}
	}
};

WindowsFileSystem gDefaultFS;

FileSystem* fsGetSystemFileSystem() { return &gDefaultFS; }

Path* fsGetApplicationPath()
{
	wchar_t utf16Path[MAX_PATH];
	DWORD   pathLength = GetModuleFileNameW(0, utf16Path, MAX_PATH);

	return fsCreatePathFromWideString(utf16Path, pathLength);
}

Path* fsGetApplicationDirectory()
{
	Path* programPath = fsGetApplicationPath();
	Path* programDir = fsGetParentPath(programPath);
	fsFreePath(programPath);
	return programDir;
}

Path* fsGetPreferredLogDirectory() { return fsGetApplicationDirectory(); }

Path* fsGetPreferencesDirectoryPath(const char* org, const char* app)
{
	PWSTR preferencesDir = NULL;
	SHGetKnownFolderPath(FOLDERID_RoamingAppData, KF_FLAG_CREATE, NULL, &preferencesDir);
	size_t pathLength = wcslen(preferencesDir);

	Path* preferencesPath = fsCreatePathFromWideString(preferencesDir, pathLength);

	size_t orgLen = strlen(org);
	size_t appLen = strlen(app);

	FileSystem* fileSystem = fsGetPathFileSystem(preferencesPath);

	char* extraComponent = (char*)alloca(orgLen + appLen + 1);
	strncpy(extraComponent, org, orgLen);
	extraComponent[orgLen] = fileSystem->GetPathDirectorySeparator();
	strncpy(extraComponent + orgLen + 1, app, appLen);

	Path* resultPath = fsAppendPathComponent(preferencesPath, extraComponent);
	fsFreePath(preferencesPath);

	return resultPath;
}

Path* fsGetUserSpecificPath()
{
	PWSTR userDocuments = NULL;
	SHGetKnownFolderPath(FOLDERID_Documents, 0, NULL, &userDocuments);
	size_t pathLength = wcslen(userDocuments);

	Path* path = fsCreatePathFromWideString(userDocuments, pathLength);
	CoTaskMemFree(userDocuments);
	return path;
}

Path* fsGetAppTempDirectory()
{
	PWSTR localAppdata = NULL;
	SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, NULL, &localAppdata);
	size_t pathLength = wcslen(localAppdata);

	Path* path = fsCreatePathFromWideString(localAppdata, pathLength);
	CoTaskMemFree(localAppdata);
	return path;
}

//static
void FormatFileExtensionsFilter(const char* fileDesc, const char** fileExtensions, size_t fileExtensionCount, eastl::string& extFiltersOut)
{
	extFiltersOut = fileDesc;
	extFiltersOut.push_back('\0');
	for (size_t i = 0; i < fileExtensionCount; ++i)
	{
		eastl::string ext = fileExtensions[i];
		if (ext.size() && ext[0] == '.')
			ext = (ext.begin() + 1);
		extFiltersOut += "*.";
		extFiltersOut += ext;
		if (i != fileExtensionCount - 1)
			extFiltersOut += ";";
		else
			extFiltersOut.push_back('\0');
	}
}

void fsShowOpenFileDialog(
	const char* title, const Path* directory, FileDialogCallbackFn callback, void* userData, const char* fileDesc,
	const char** fileExtensions, size_t fileExtensionCount)
{
	eastl::string extFilter;
	FormatFileExtensionsFilter(fileDesc, fileExtensions, fileExtensionCount, extFilter);
	OPENFILENAMEW ofn;    // common dialog box structure

	size_t   outputBufferLen = directory->mPathLength + MAX_PATH;
	wchar_t* outputBuffer = (wchar_t*)alloca(outputBufferLen * sizeof(wchar_t));
	// Initialize OPENFILENAME
	ZeroMemory(&ofn, sizeof(ofn));

	size_t   titleLength = strlen(title) + 1;    // include the null terminator.
	wchar_t* titleW = (wchar_t*)alloca(titleLength * sizeof(wchar_t));
	MultiByteToWideChar(CP_UTF8, 0, title, (int)titleLength, titleW, (int)(titleLength * sizeof(wchar_t)));

	ofn.lpstrTitle = titleW;
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = NULL;
	ofn.lpstrFile = outputBuffer;
	// Set lpstrFile[0] to '\0' so that GetOpenFileName does not
	// use the contents of szFile to initialize itself.
	ofn.lpstrFile[0] = '\0';
	ofn.nMaxFile = (DWORD)outputBufferLen;

	size_t   extFilterLength = extFilter.size() + 1;    // include the null terminator.
	wchar_t* extFilterW = (wchar_t*)alloca(extFilterLength * sizeof(wchar_t));
	MultiByteToWideChar(CP_UTF8, 0, extFilter.c_str(), (int)extFilterLength, extFilterW, (int)(extFilterLength * sizeof(wchar_t)));

	ofn.lpstrFilter = extFilterW;
	ofn.nFilterIndex = 1;
	ofn.lpstrFileTitle = NULL;
	ofn.nMaxFileTitle = 0;
	ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

	// Directory Path utf-16 conversion
	wchar_t* directoryPathStr = (wchar_t*)alloca((directory->mPathLength + 1) * sizeof(wchar_t));
	size_t directoryPathStrLength =
		MultiByteToWideChar(CP_UTF8, 0, fsGetPathAsNativeString(directory), (int)directory->mPathLength, directoryPathStr, (int)directory->mPathLength);
	directoryPathStr[directoryPathStrLength] = 0;

	ofn.lpstrInitialDir = directoryPathStr;

	if (::GetOpenFileNameW(&ofn) == TRUE)
	{
		Path* path = fsCreatePathFromWideString(outputBuffer, wcslen(outputBuffer));
		callback(path, userData);
		fsFreePath(path);
	}
}

void fsShowSaveFileDialog(
	const char* title, const Path* directory, FileDialogCallbackFn callback, void* userData, const char* fileDesc,
	const char** fileExtensions, size_t fileExtensionCount)
{
	eastl::string extFilter;
	FormatFileExtensionsFilter(fileDesc, fileExtensions, fileExtensionCount, extFilter);
	OPENFILENAMEW ofn;    // common dialog box structure

	size_t   outputBufferLen = directory->mPathLength + MAX_PATH;
	wchar_t* outputBuffer = (wchar_t*)alloca(outputBufferLen * sizeof(wchar_t));
	// Initialize OPENFILENAME
	ZeroMemory(&ofn, sizeof(ofn));

	size_t   titleLength = strlen(title) + 1;    // include the null terminator.
	wchar_t* titleW = (wchar_t*)alloca(titleLength * sizeof(wchar_t));
	MultiByteToWideChar(CP_UTF8, 0, title, (int)titleLength, titleW, (int)(titleLength * sizeof(wchar_t)));

	ofn.lpstrTitle = titleW;
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = NULL;
	ofn.lpstrFile = outputBuffer;
	// Set lpstrFile[0] to '\0' so that GetOpenFileName does not
	// use the contents of szFile to initialize itself.
	ofn.lpstrFile[0] = '\0';
	ofn.nMaxFile = (DWORD)outputBufferLen;

	size_t   extFilterLength = extFilter.size() + 1;    // include the null terminator.
	wchar_t* extFilterW = (wchar_t*)alloca(extFilterLength * sizeof(wchar_t));
	MultiByteToWideChar(CP_UTF8, 0, extFilter.c_str(), (int)extFilterLength, extFilterW, (int)(extFilterLength * sizeof(wchar_t)));

	ofn.lpstrFilter = extFilterW;
	ofn.nFilterIndex = 1;
	ofn.lpstrFileTitle = NULL;
	ofn.nMaxFileTitle = 0;
	ofn.Flags = OFN_EXPLORER | OFN_NOCHANGEDIR;
	
	// Directory Path utf-16 conversion
	wchar_t* directoryPathStr = (wchar_t*)alloca((directory->mPathLength + 1) * sizeof(wchar_t));
	size_t directoryPathStrLength =
		MultiByteToWideChar(CP_UTF8, 0, fsGetPathAsNativeString(directory), (int)directory->mPathLength, directoryPathStr, (int)directory->mPathLength);
	directoryPathStr[directoryPathStrLength] = 0;

	ofn.lpstrInitialDir = directoryPathStr;

	if (::GetSaveFileNameW(&ofn) == TRUE)
	{
		Path* path = fsCreatePathFromWideString(outputBuffer, wcslen(outputBuffer));
		callback(path, userData);
		fsFreePath(path);
	}
}

typedef struct FileWatcher
{
	Path*               mWatchDir;
	DWORD               mNotifyFilter;
	FileWatcherCallback mCallback;
	HANDLE              hExitEvt;
	ThreadDesc          mThreadDesc;
	ThreadHandle        mThread;
	volatile int        mRun;
} FileWatcher;

void fswThreadFunc(void* data)
{
	Thread::SetCurrentThreadName("FileWatcher");
	FileWatcher* fs = (FileWatcher*)data;

	HANDLE hDir = withUTF16Path<HANDLE>(fs->mWatchDir, [](const wchar_t* pathStr) {
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
				case FILE_ACTION_ADDED: action = FWE_CREATED; break;
				case FILE_ACTION_MODIFIED:
					if (fs->mNotifyFilter & FILE_NOTIFY_CHANGE_LAST_WRITE)
						action = FWE_MODIFIED;
					if (fs->mNotifyFilter & FILE_NOTIFY_CHANGE_LAST_ACCESS)
						action = FWE_ACCESSED;
					break;
				case FILE_ACTION_REMOVED: action = FWE_DELETED; break;
				case FILE_ACTION_RENAMED_NEW_NAME: action = FWE_CREATED; break;
				case FILE_ACTION_RENAMED_OLD_NAME: action = FWE_DELETED; break;
				default: break;
			}

			int outputLength = WideCharToMultiByte(CP_UTF8, 0, fni->FileName, fni->FileNameLength / sizeof(WCHAR), utf8Name, sizeof(utf8Name) - 1, NULL, NULL);
			if (outputLength == 0)
			{
				continue;
			}

			utf8Name[outputLength] = 0;
			Path* path = fsAppendPathComponent(fs->mWatchDir, utf8Name);
			LOGF(LogLevel::eINFO, "Monitoring activity of file: %s -- Action: %d", fsGetPathAsNativeString(path), fni->Action);
			fs->mCallback(path, action);
			fsFreePath(path);

			if (!fni->NextEntryOffset)
				break;
			p += fni->NextEntryOffset;
		}
	}

	CloseHandle(hDir);
	CloseHandle(hEvt);
};

FileWatcher* fsCreateFileWatcher(const Path* path, FileWatcherEventMask eventMask, FileWatcherCallback callback)
{
	FileWatcher* watcher = conf_new(FileWatcher);
	watcher->mWatchDir = fsCopyPath(path);
	watcher->mCallback = callback;

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

	watcher->mThreadDesc.pFunc = fswThreadFunc;
	watcher->mThreadDesc.pData = watcher;

	watcher->mThread = create_thread(&watcher->mThreadDesc);

	return watcher;
}

void fsFreeFileWatcher(FileWatcher* fileWatcher)
{
	fileWatcher->mRun = FALSE;
	SetEvent(fileWatcher->hExitEvt);
	destroy_thread(fileWatcher->mThread);
	CloseHandle(fileWatcher->hExitEvt);
	fsFreePath(fileWatcher->mWatchDir);
	conf_delete(fileWatcher);
}

#endif
