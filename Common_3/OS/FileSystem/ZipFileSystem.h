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

#ifndef ZipFileSystem_h
#define ZipFileSystem_h

#include "FileSystemInternal.h"

typedef struct zip zip;

class ZipFileSystem: public FileSystem
{
	zip*            pZipFile;
	FileSystemFlags mFlags;
	time_t          mCreationTime;
	time_t          mLastAccessedTime;

public:
	ZipFileSystem(zip* zipFile, FileSystemFlags flags, time_t creationTime, time_t lastAccessedTime);
	~ZipFileSystem();

	static ZipFileSystem* CreateWithRootAtPath(const Path* rootPath, FileSystemFlags flags);

	char   GetPathDirectorySeparator() const override;
	size_t GetRootPathLength() const override;
	
	bool   IsReadOnly() const override;
	bool   IsCaseSensitive() const override;

	/// Fills path's buffer with the canonical root path corresponding to the root of absolutePathString,
	/// and returns an offset into absolutePathString containing the path component after the root by pathComponentOffset.
	/// path is assumed to have storage for up to 16 characters.
	bool FormRootPath(const char* absolutePathString, Path* path, size_t* pathComponentOffset) const override;

	time_t GetCreationTime(const Path* filePath) const override;
	time_t GetLastAccessedTime(const Path* filePath) const override;
	time_t GetLastModifiedTime(const Path* filePath) const override;

	bool CreateDirectory(const Path* directoryPath) const override;
	bool DeleteFile(const Path* path) const override;

	bool FileExists(const Path* path) const override;
	bool IsDirectory(const Path* path) const override;

	FileStream* OpenFile(const Path* filePath, FileMode mode) const override;

	bool CopyFile(const Path* sourcePath, const Path* destinationPath, bool overwriteIfExists) const override;
	void EnumerateFilesWithExtension(
		const Path* directory, const char* extension, bool (*processFile)(const Path*, void* userData), void* userData) const override;
	void EnumerateSubDirectories(
		const Path* directory, bool (*processDirectory)(const Path*, void* userData), void* userData) const override;
};

#endif /* ZipFileSystem_h */
