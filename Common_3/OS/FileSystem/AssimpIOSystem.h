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

#ifndef AssimpIOSystem_h
#define AssimpIOSystem_h

#include "FileSystemInternal.h"
#include "../../ThirdParty/OpenSource/assimp/4.1.0/include/assimp/IOSystem.hpp"
#include "../../ThirdParty/OpenSource/EASTL/vector.h"

class AssimpIOSystem: public Assimp::IOSystem
{
	const FileSystem*    pFileSystem;
	eastl::vector<Path*> mPathStack;
	Path*                pDefaultPath;

	const Path* CurrentRootPath() const;
	Path* ToPath(const char* pathStr) const;

	public:
	// NOTE: fileSystem must outlive the AssimpIOSystem.
	// For the default FileSystem, this is automatically true;
	// however, for e.g. zip file systems, the lifetime of the
	// FileSystem must be extended until Assimp no longer holds
	// this AssimpIOSystem.
	AssimpIOSystem(const FileSystem* fileSystem);
	~AssimpIOSystem();

	bool Exists(const char* pFile) const override;

	char getOsSeparator() const override;

	Assimp::IOStream* Open(const char* pFile, const char* pMode = "rb") override;

	void Close(Assimp::IOStream* pFile) override;

	bool ComparePaths(const char* one, const char* second) const override;

	bool PushDirectory(const std::string& path) override;

	bool PopDirectory() override;

	bool CreateDirectory(const std::string& path) override;

	bool DeleteFile(const std::string& file) override;
};

#endif /* AssimpIOSystem_h */
