/*
 * Copyright (c) 2017-2024 The Forge Interactive Inc.
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
#include <stdlib.h>

#include "../../../../Common_3/Utilities/Interfaces/ILog.h"
#include "../../../../Common_3/Utilities/ThirdParty/OpenSource/Nothings/stb_ds.h"

#ifdef __APPLE__
#include <TargetConditionals.h>
#if TARGET_OS_MAC
#define USE_UNIX
#endif
#endif

#ifdef __linux__
#define USE_UNIX
#endif

#if defined(USE_UNIX)
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#if defined(_WINDOWS)
static const char* GetErrorMessage(DWORD err)
{
    static __declspec(thread) char errorBuf[1024];
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_MAX_WIDTH_MASK, NULL, err,
                   MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), errorBuf, sizeof(errorBuf), NULL);
    return errorBuf;
}
#endif

int systemRun(const char* command, const char** arguments, size_t argumentCount, FileStream* pFs)
{
    UNREF_PARAM(command);
    UNREF_PARAM(arguments);
    UNREF_PARAM(argumentCount);
    UNREF_PARAM(pFs);
#if defined(XBOX)
    ASSERT(false && "processRun is unsupported on Xbox");
    return -1;

#elif defined(_WINDOWS)

    unsigned char commandBuf[256];

    bstring commandLine = bemptyfromarr(commandBuf);
    bformat(&commandLine, "\"%s\"", command);
    for (size_t i = 0; i < argumentCount; ++i)
        bformata(&commandLine, " %s", arguments[i]);

    ASSERT(bisvalid(&commandLine) && biscstr(&commandLine));

    HANDLE stdOut = NULL;
    if (pFs)
    {
        stdOut = (HANDLE)fsGetSystemHandle(pFs);
    }

    STARTUPINFOA        startupInfo;
    PROCESS_INFORMATION processInfo;
    memset(&startupInfo, 0, sizeof startupInfo);
    memset(&processInfo, 0, sizeof processInfo);
    startupInfo.cb = sizeof(STARTUPINFO);
    startupInfo.dwFlags |= STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    startupInfo.hStdOutput = stdOut;
    startupInfo.hStdError = stdOut;
    startupInfo.wShowWindow = FALSE;

    if (!CreateProcessA(NULL, (LPSTR)commandLine.data, NULL, NULL, stdOut ? TRUE : FALSE, 0, NULL, NULL, &startupInfo, &processInfo))
    {
        bdestroy(&commandLine);
        DWORD err = GetLastError();
        LOGF(eERROR, "Error when creating process: (%d) %s", (int)err, GetErrorMessage(err));
        LOGF(eERROR, "COMMAND:");
        LOGF(eERROR, "    %s", command);
        for (size_t i = 0; i < argumentCount; ++i)
        {
            LOGF(eERROR, "    %s", arguments[i]);
        }
        return -1;
    }

    WaitForSingleObject(processInfo.hProcess, 10000);
    DWORD exitCode;
    GetExitCodeProcess(processInfo.hProcess, &exitCode);

    CloseHandle(processInfo.hProcess);
    CloseHandle(processInfo.hThread);

    bdestroy(&commandLine);
    return exitCode;
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
#elif defined(USE_UNIX)
    // NOTE: make sure this is the last case since other platforms may define __unix__
    // NOTE:  do not use eastl in the forked process!  It will use unsafe functions (such as malloc) which will hang the whole thing
    const char** argPtrs = NULL;
    arrsetcap(argPtrs, argumentCount + 2);
    arrpush(argPtrs, command);
    for (size_t i = 0; i < argumentCount; ++i)
        arrpush(argPtrs, arguments[i]);

    arrpush(argPtrs, NULL);

    pid_t pid = fork();
    if (pid < 0)
    {
        LOGF(eWARNING, "Can't fork another process");
        return -1;
    }

    if (pid == 0)
    {
        if (pFs)
        {
            // windows return a HANDLE which is 64bits unix use
            // fd which is int so cast to ssize_t then int
            int fd = (int)(ssize_t)fsGetSystemHandle(pFs);
            if (dup2(fd, STDOUT_FILENO) < 0)
            {
                LOGF(eWARNING, "Can't redirect stdout to the file");
                exit(1);
            }
        }

        execvp(argPtrs[0], (char**)&argPtrs[0]);
        exit(1);
    }
    else
    {
        // parent
        int waitStatus = 0;

        // Use waitpid to make sure we wait for our forked process, not some other process created in another thread.
        waitpid(pid, &waitStatus, 0);
        arrfree(argPtrs);

        if (WIFEXITED(waitStatus))
        {
            // Child process terminated normally
            const int exitCode = WEXITSTATUS(waitStatus);
            return exitCode;
        }
        // else process didn't terminate normally, to see how to extract
        // information about termination reason check the man page for wait:
        // https://man7.org/linux/man-pages/man2/waitpid.2.html

        LOGF(eINFO, "Child Procress failed");
        if (WIFSIGNALED(waitStatus))
        {
            LOGF(eWARNING, "Child process terminated by signal: %d", WTERMSIG(waitStatus));
        }

        return -1;
    }
#else
    ASSERT(false && "Unkown platform");
    return -1;
#endif
}
