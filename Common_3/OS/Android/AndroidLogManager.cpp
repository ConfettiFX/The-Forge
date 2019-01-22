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

#include <cstdio>
#include <iostream>
#include <unistd.h>
#include "../Interfaces/IOperatingSystem.h"

// interfaces
#include "../Interfaces/ILogManager.h"
#include "../Interfaces/IMemoryManager.h"
#include <assert.h>

void outputLogString(const char* pszStr)
{
	_OutputDebugString(pszStr);
	_OutputDebugString("\n");
}

void _ErrorMsg(int line, const char* file, const char* string, ...)
{
	ASSERT(string);
	//Eval the string
	const unsigned BUFFER_SIZE = 65536;
	char           buf[BUFFER_SIZE];
	// put source code file name at the begin
	snprintf(buf, BUFFER_SIZE, "%s", file);
	// put line positoin in code
	snprintf(buf + strlen(buf), BUFFER_SIZE - strlen(buf), "(%d)\t", line);

	va_list arglist;
	va_start(arglist, string);
	//  vsprintf_s(buf + strlen(buf), BUFFER_SIZE - strlen(buf), string, arglist);
	vsprintf_s(buf + strlen(buf), BUFFER_SIZE - strlen(buf), string, arglist);
	va_end(arglist);

	printf("Error: %s", buf);
}

void _WarningMsg(int line, const char* file, const char* string, ...)
{
	ASSERT(string);

	//Eval the string
	const unsigned BUFFER_SIZE = 65536;
	char           buf[BUFFER_SIZE];

	// put source code file name at the begin
	snprintf(buf, BUFFER_SIZE, "%s", file);
	// put line positoin in code
	snprintf(buf + strlen(buf), BUFFER_SIZE - strlen(buf), "(%d)\t", line);

	va_list arglist;
	va_start(arglist, string);
	vsprintf_s(buf + strlen(buf), BUFFER_SIZE - strlen(buf), string, arglist);
	va_end(arglist);

	printf("Warning: %s", buf);
}

void _InfoMsg(int line, const char* file, const char* string, ...)
{
	ASSERT(string);

	//Eval the string
	const unsigned BUFFER_SIZE = 65536;
	char           buf[BUFFER_SIZE];

	// put source code file name at the begin
	snprintf(buf, BUFFER_SIZE, "%s", file);
	// put line positoin in code
	snprintf(buf + strlen(buf), BUFFER_SIZE - strlen(buf), "(%d)\t", line);

	va_list arglist;
	va_start(arglist, string);
	vsprintf_s(buf + strlen(buf), BUFFER_SIZE - strlen(buf), string, arglist);
	va_end(arglist);

	_OutputDebugString(buf);
}

void _OutputDebugString(const char* str, ...)
{
#ifdef _DEBUG
	const unsigned BUFFER_SIZE = 4096;
	char           buf[BUFFER_SIZE];

	va_list arglist;
	va_start(arglist, str);
	vsprintf_s(buf, BUFFER_SIZE, str, arglist);
	va_end(arglist);

	printf("%s\n", buf);
#endif
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

void _PrintUnicode(const tinystl::string& str, bool error) { outputLogString(str.c_str()); }

void _PrintUnicodeLine(const tinystl::string& str, bool error) { _PrintUnicode(str, error); }
#endif    // ifdef __ANDROID__