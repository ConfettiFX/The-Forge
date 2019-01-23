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

#ifdef _WIN32

#include <io.h>    // _isatty

// interfaces
#include "../Interfaces/IOperatingSystem.h"
#include "../Interfaces/ILogManager.h"
#include "../Interfaces/IMemoryManager.h"

void outputLogString(const char* pszStr)
{
	OutputDebugStringA(pszStr);
	OutputDebugStringA("\n");
}

void _ErrorMsg(int line, const char* file, const char* string, ...)
{
	ASSERT(string);
	//Eval the string
	const unsigned BUFFER_SIZE = 65536;
	char           buf[BUFFER_SIZE];
	// put source code file name at the begin
	sprintf_s(buf, BUFFER_SIZE, file);
	// put line positoin in code
	sprintf_s(buf + strlen(buf), BUFFER_SIZE - strlen(buf), "(%d)\t", line);

	va_list arglist;
	va_start(arglist, string);
	//  vsprintf_s(buf + strlen(buf), BUFFER_SIZE - strlen(buf), string, arglist);
	vsprintf_s(buf + strlen(buf), BUFFER_SIZE - strlen(buf), string, arglist);
	va_end(arglist);
	// no log, just output to a message box
	MessageBoxA(NULL, buf, "Error", MB_OK | MB_ICONERROR);
}

void _WarningMsg(int line, const char* file, const char* string, ...)
{
	ASSERT(string);

	//Eval the string
	const unsigned BUFFER_SIZE = 65536;
	char           buf[BUFFER_SIZE];

	// put source code file name at the begin
	sprintf_s(buf, BUFFER_SIZE, file);
	// put line positoin in code
	sprintf_s(buf + strlen(buf), BUFFER_SIZE - strlen(buf), "(%d)\t", line);

	va_list arglist;
	va_start(arglist, string);
	vsprintf_s(buf + strlen(buf), BUFFER_SIZE - strlen(buf), string, arglist);
	va_end(arglist);

	// no log, just output to a message box
	MessageBoxA(NULL, buf, "Warning", MB_OK | MB_ICONWARNING);
}

void _InfoMsg(int line, const char* file, const char* string, ...)
{
	ASSERT(string);

	//Eval the string
	const unsigned BUFFER_SIZE = 65536;
	char           buf[BUFFER_SIZE];

	// put source code file name at the begin
	sprintf_s(buf, BUFFER_SIZE, file);
	// put line positoin in code
	sprintf_s(buf + strlen(buf), BUFFER_SIZE - strlen(buf), "(%d)\t", line);

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

	OutputDebugStringA(buf);
	OutputDebugStringA("\n");
#endif
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
			wcscat(str, L"Display more asserts?");
			if (MessageBoxW(NULL, str, L"Assert failed", MB_YESNO | MB_ICONERROR | MB_DEFBUTTON2) != IDYES)
			{
				debug = false;
			}
		}
	}
}

void _PrintUnicode(const tinystl::string& str, bool error)
{
	// If the output stream has been redirected, use fprintf instead of WriteConsoleW,
	// though it means that proper Unicode output will not work
	FILE* out = error ? stderr : stdout;
	if (!_isatty(_fileno(out)))
		fprintf(out, "%s\n", str.c_str());
	else
	{
		if (error)
			printf("%s\n", str.c_str());    // use this for now because WriteCosnoleW sometimes cause blocking
		else
			printf("%s\n", str.c_str());
	}

	outputLogString(str.c_str());
}

void _PrintUnicodeLine(const tinystl::string& str, bool error) { _PrintUnicode(str, error); }

#endif
