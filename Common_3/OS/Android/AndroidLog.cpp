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
#ifdef __ANDROID__

#include <cstdio>
#include <iostream>
#include <unistd.h>
#include "../Interfaces/IOperatingSystem.h"

// interfaces
#include "../Interfaces/ILog.h"
#include <assert.h>
#include "../Interfaces/IMemory.h"
#include <android/log.h>

void outputLogString(const char* pszStr)
{
	_OutputDebugString(pszStr);
	_OutputDebugString("\n");
}

void _OutputDebugStringV(const char* str, va_list args)
{
#if defined(_DEBUG) || defined(AUTOMATED_TESTING)
	const unsigned BUFFER_SIZE = 4096;
    char           buf[BUFFER_SIZE];

    vsprintf_s(buf, BUFFER_SIZE, str, args);

	// stdout doesn't work on android. we need to use ndk log functionality.
    //printf("%s\n", buf);
	__android_log_print(ANDROID_LOG_INFO, "The-Forge", buf);
#endif
}

void _OutputDebugString(const char* str, ...)
{
	va_list arglist;
	va_start(arglist, str);
	_OutputDebugStringV(str, arglist);
	va_end(arglist);
}

void _FailedAssert(const char* file, int line, const char* statement)
{
	static bool debug = true;

	if (debug)
	{
		printf("Failed: (%s)\n\nFile: %s\nLine: %d\n\n", statement, file, line);
	}
	assert(0);
}

void _PrintUnicode(const eastl::string& str, bool error) { 

	if (error)
	{
		__android_log_print(ANDROID_LOG_ERROR, "The-Forge", str.c_str());
	}
	else
	{
		outputLogString(str.c_str());
	}
}

void _PrintUnicodeLine(const eastl::string& str, bool error) { _PrintUnicode(str, error); }
#endif    // ifdef __ANDROID__