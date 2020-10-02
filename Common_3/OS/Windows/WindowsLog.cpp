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

#include <io.h>    // _isatty

// interfaces
#include "../Interfaces/IOperatingSystem.h"
#include "../Interfaces/ILog.h"
#include "../Interfaces/IMemory.h"

void _OutputDebugStringV(const char* str, va_list args)
{
    const unsigned BUFFER_SIZE = 4096;
    char           buf[BUFFER_SIZE];

    vsprintf_s(buf, BUFFER_SIZE, str, args);
	OutputDebugStringA(buf);
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
		WCHAR str[1024];
		WCHAR message[1024];
		WCHAR wfile[1024];
		mbstowcs(message, statement, 1024);
		mbstowcs(wfile, file, 1024);
		wsprintfW(str, L"Failed: (%s)\n\nFile: %s\nLine: %d\n\n", message, wfile, line);

		if (IsDebuggerPresent())
		{
			wcscat(str, L"Debug?");
			int res = MessageBoxW(NULL, str, L"Assert failed", MB_YESNOCANCEL | MB_ICONERROR);
			if (res == IDYES)
			{
#if _MSC_VER >= 1400
				__debugbreak();
#else
				_asm int 0x03;
#endif
			}
			else if (res == IDCANCEL)
			{
				debug = false;
			}
		}
		else
		{
#ifdef FORGE_STACKTRACE_DUMP
			__debugbreak();
#else
			wcscat(str, L"Display more asserts?");
			if (MessageBoxW(NULL, str, L"Assert failed", MB_YESNO | MB_ICONERROR | MB_DEFBUTTON2) != IDYES)
			{
				debug = false;
			}
#endif
		}
	}
}

void _PrintUnicode(const char* str, bool error)
{
	// If the output stream has been redirected, use fprintf instead of WriteConsoleW,
	// though it means that proper Unicode output will not work
	FILE* out = error ? stderr : stdout;
	if (!_isatty(_fileno(out)))
		fprintf(out, "%s", str);
	else
	{
		if (error)
			printf("%s", str);    // use this for now because WriteCosnoleW sometimes cause blocking
		else
			printf("%s", str);
	}

	_OutputDebugString(str);
}

#endif
