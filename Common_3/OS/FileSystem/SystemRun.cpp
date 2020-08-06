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

#include <errno.h>

#include "../Interfaces/IOperatingSystem.h"
#include "../Interfaces/IFileSystem.h"
#include "../Interfaces/ILog.h"
#include "../../ThirdParty/OpenSource/EASTL/string.h"
#include "../../ThirdParty/OpenSource/EASTL/vector.h"

int systemRun(const char* command, const char** arguments, size_t argumentCount, const char* stdOutFile)
{
#if defined(XBOX)
	ASSERT(!"UNIMPLEMENTED");
	return -1;

#elif defined(_WIN32)

	eastl::string commandLine = "\"" + eastl::string(command) + "\"";
	for (size_t i = 0; i < argumentCount; ++i)
		commandLine += " " + eastl::string(arguments[i]);

	HANDLE stdOut = NULL;
	if (stdOutFile)
	{
		SECURITY_ATTRIBUTES sa;
		sa.nLength = sizeof(sa);
		sa.lpSecurityDescriptor = NULL;
		sa.bInheritHandle = TRUE;

		size_t   pathLength = strlen(stdOutFile) + 1;
		wchar_t* buffer = (wchar_t*)alloca(pathLength * sizeof(wchar_t));
		MultiByteToWideChar(CP_UTF8, 0, stdOutFile, (int)pathLength, buffer, (int)pathLength);
		stdOut = CreateFileW(buffer, GENERIC_ALL, FILE_SHARE_WRITE | FILE_SHARE_READ, &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	}

	STARTUPINFOA        startupInfo;
	PROCESS_INFORMATION processInfo;
	memset(&startupInfo, 0, sizeof startupInfo);
	memset(&processInfo, 0, sizeof processInfo);
	startupInfo.cb = sizeof(STARTUPINFO);
	startupInfo.dwFlags |= STARTF_USESTDHANDLES;
	startupInfo.hStdOutput = stdOut;
	startupInfo.hStdError = stdOut;

	if (!CreateProcessA(
			NULL, (LPSTR)commandLine.c_str(), NULL, NULL, stdOut ? TRUE : FALSE, CREATE_NO_WINDOW, NULL, NULL, &startupInfo, &processInfo))
		return -1;

	WaitForSingleObject(processInfo.hProcess, INFINITE);
	DWORD exitCode;
	GetExitCodeProcess(processInfo.hProcess, &exitCode);

	CloseHandle(processInfo.hProcess);
	CloseHandle(processInfo.hThread);

	if (stdOut)
	{
		CloseHandle(stdOut);
	}

	return exitCode;
#elif defined(__linux__)
	eastl::vector<const char*> argPtrs;
	eastl::string              cmd(command);
	char                       space = ' ';
	cmd.append(&space, &space + 1);
	for (size_t i = 0; i < argumentCount; ++i)
	{
		cmd += eastl::string(arguments[i]) + " ";
	}

	int res = system(cmd.c_str());
	return res;
#elif TARGET_OS_IPHONE

	ASSERT(false && "processRun is unsupported on iOS");
	return -1;
#elif NX64
	ASSERT(false && "processRun is unsupported on NX");
	return -1;
#elif defined(ORBIS)
	ASSERT(false && "processRun is unsupported on Orbis");
	return -1;
#elif defined(PROSPERO)
	ASSERT(false && "processRun is unsupported on Prospero");
	return -1;
#else

	// NOTE:  do not use eastl in the forked process!  It will use unsafe functions (such as malloc) which will hang the whole thing
	eastl::vector<const char*> argPtrs;
	argPtrs.push_back(command);
	for (size_t i = 0; i < argumentCount; ++i)
		argPtrs.push_back(arguments[i]);
	argPtrs.push_back(NULL);

	pid_t pid = fork();
	if (!pid)
	{
		execvp(argPtrs[0], (char**)&argPtrs[0]);
		return -1;    // Return -1 if we could not spawn the process
	}
	else if (pid > 0)
	{
		int exitCode = EINTR;
		while (exitCode == EINTR)
			wait(&exitCode);
		return exitCode;
	}
	else
		return -1;
#endif
}
