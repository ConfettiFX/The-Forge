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

#import <Foundation/Foundation.h>

#include "../Interfaces/IFileSystem.h"
#include "../Interfaces/ILog.h"
#include "../Interfaces/IOperatingSystem.h"

#include "../Interfaces/IMemory.h"

char gResourceMounts[RM_COUNT][FS_MAX_PATH] = {};
		
bool initFileSystem(FileSystemInitDesc* pDesc)
{
	if (!pDesc)
	{
		return false;
	}
	
	// Get application directory
	strcpy(gResourceMounts[RM_CONTENT], [[[[NSBundle mainBundle] resourceURL] relativePath] UTF8String]);
	
	// Get save directory
    NSFileManager* fileManager = [NSFileManager defaultManager];
	[fileManager changeCurrentDirectoryPath:[[NSBundle mainBundle] bundlePath]];
	
    NSError* error = nil;
    NSURL* url = [fileManager URLForDirectory:NSDocumentDirectory inDomain:NSUserDomainMask appropriateForURL:nil create:true error:&error];
    if (!error)
	{
		fsAppendPathComponent([[url path] UTF8String], pDesc->pAppName, gResourceMounts[RM_SAVE_0]);
    }
	else
	{
		LOGF(LogLevel::eERROR, "Error retrieving user documents directory: %s", [[error description] UTF8String]);
		return false;
	}
	
	// Get debug directory
#ifdef TARGET_IOS
	// Place log files in the application support directory on iOS.
	url = [fileManager URLForDirectory:NSApplicationSupportDirectory inDomain:NSUserDomainMask appropriateForURL:nil create:true error:&error];
	if (!error)
	{
		strcpy(gResourceMounts[RM_DEBUG], [[url path] UTF8String]);
	}
	else
	{
		LOGF(LogLevel::eERROR, "Error retrieving application support directory: %s", [[error description] UTF8String]);
	}
#else
	const char* path = [[[NSBundle mainBundle] bundlePath] UTF8String];
	fsGetParentPath(path, gResourceMounts[RM_DEBUG]);
#endif

	return true;
}

void exitFileSystem()
{
}
