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

#include "../Interfaces/ILog.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <unistd.h>
#include <pwd.h>
#include <fcntl.h>           //for open and O_* enums
#include <dirent.h>

char gResourceMounts[RM_COUNT][FS_MAX_PATH];

bool initFileSystem(FileSystemInitDesc* pDesc)
{
	if (!pDesc->pAppName)
	{
		return false;
	}
	
	// Get application directory and name
	char applicationPath[FS_MAX_PATH] = {};
	readlink("/proc/self/exe", applicationPath, FS_MAX_PATH);
	fsGetParentPath(applicationPath, gResourceMounts[RM_CONTENT]);
	fsGetParentPath(applicationPath, gResourceMounts[RM_DEBUG]);

	// Get user directory
	const char* homedir;
	if ((homedir = getenv("HOME")) == NULL)
	{
		homedir = getpwuid(getuid())->pw_dir;
	}
	char userDir[FS_MAX_PATH] = { "Documents/"};
	strncat(userDir, pDesc->pAppName, strlen(pDesc->pAppName));
	fsAppendPathComponent(homedir, userDir, gResourceMounts[RM_SAVE_0]);

	// Get temp directory
	//const char* tempdir;
	//if ((tempdir = getenv("TMPDIR")) == NULL)
	//{
	//	tempdir = getpwuid(getuid())->pw_dir;
	//}
	//fsAppendPathComponent(tempdir, "tmp", gTempDirectory);

	return true;
}

void exitFileSystem(void)
{

}
