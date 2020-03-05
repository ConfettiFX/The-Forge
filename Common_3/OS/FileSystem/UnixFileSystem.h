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

#ifndef UnixFileSystem_h
#define UnixFileSystem_h

#include "FileSystemInternal.h"

class UnixFileSystem: public FileSystem
{
	public:
    inline UnixFileSystem() : FileSystem(FSK_SYSTEM) {};

	bool   IsReadOnly() const override;
	char   GetPathDirectorySeparator() const override;
	size_t GetRootPathLength() const override;

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
};

#endif /* UnixFileSystem_h */
