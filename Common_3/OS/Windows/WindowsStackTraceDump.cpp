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

#include "WindowsStackTraceDump.h"

#ifdef ENABLE_FORGE_STACKTRACE_DUMP

#include "../../Utilities/Interfaces/ILog.h"
#pragma warning(push)
#pragma warning(disable : 4091)
#include <DbgHelp.h>
#pragma warning(pop)
#pragma comment(lib, "DbgHelp.lib")
#include <Psapi.h>

bool    WindowsStackTrace::mInit = false;
Mutex   WindowsStackTrace::mDbgHelpMutex;
uint8_t WindowsStackTrace::mPreallocatedMemory[mPreallocatedMemorySize];
size_t  WindowsStackTrace::mUsedMemorySize = 0;

static LONG WINAPI dumpStackTrace(EXCEPTION_POINTERS* pExceptionInfo) { return WindowsStackTrace::Dump(pExceptionInfo); }

bool WindowsStackTrace::Init()
{
    if (mInit)
    {
        return false;
    }

    ULONG stackSize = 20000;
    if (!SetThreadStackGuarantee(&stackSize))
        return false;

    SetUnhandledExceptionFilter(&dumpStackTrace);

    initMutex(&mDbgHelpMutex);

    mInit = true;
    return true;
}

void WindowsStackTrace::Exit() { mInit = false; }

void* WindowsStackTrace::Alloc(size_t size)
{
    if ((mUsedMemorySize + size) > mPreallocatedMemorySize)
    {
        Log("Not enough preallocated memory remaining for allocation of %u bytes", size);
        return NULL;
    }

    void* pMem = ((uint8_t*)mPreallocatedMemory + mUsedMemorySize);
    mUsedMemorySize += size;
    return pMem;
}

void WindowsStackTrace::Log(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    if (mInit)
    {
        // Use the normal log output if we haven't shutdown the log system yet
        writeLogVaList(LogLevel::eERROR, __FILE__, __LINE__, fmt, args);
    }
    else
    {
        // Manually write output if we have already shutdown OS systems.
        const char* outputBuf;
        char        fmtBuf[1024];
        int         charsWritten = vsnprintf(fmtBuf, sizeof(fmtBuf), fmt, args);
        if (charsWritten < 0)
        {
            // Some kind of formatting error occurred, just print the format string instead
            outputBuf = fmt;
        }
        else
        {
            charsWritten += 2; // "\n\0"
            if (charsWritten > sizeof(fmtBuf))
                charsWritten = sizeof(fmtBuf);

            // Ensure there's a newline and null terminator
            fmtBuf[charsWritten - 2] = '\n';
            fmtBuf[charsWritten - 1] = '\0';
            outputBuf = fmtBuf;
        }

        // We really really don't want to miss these messages.
        // Write to stdout, stderr, and the debugger.
        fputs(outputBuf, stdout);
        fputs(outputBuf, stderr);
        OutputDebugStringA(outputBuf);
    }

    va_end(args);
}

LONG WindowsStackTrace::Dump(EXCEPTION_POINTERS* pExceptionInfo)
{
    MutexLock dbgHelpLock(mDbgHelpMutex);

    Log("APP CRASHED - See the stack trace below. Output might be duplicated if you are capturing both stdout and stderr");

    HANDLE thread = GetCurrentThread();
    HANDLE process = GetCurrentProcess();

    SymInitialize(process, NULL, FALSE);
    // DWORD options = SymGetOptions() | (SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);

    HMODULE* pModuleHandles;
    DWORD    requiredSize;
    EnumProcessModules(process, NULL, 0, &requiredSize);
    DWORD numModules = requiredSize / sizeof(*pModuleHandles);

    pModuleHandles = (HMODULE*)WindowsStackTrace::Alloc(requiredSize);
    if (!pModuleHandles)
    {
        Log("Failed to allocate storage for pModuleHandles during stack trace dump");
        return EXCEPTION_EXECUTE_HANDLER;
    }

    EnumProcessModules(process, pModuleHandles, requiredSize, &requiredSize);

    void* moduleBaseAddress = NULL;
    for (DWORD i = 0; i < numModules; ++i)
    {
        MODULEINFO moduleInfo;
        GetModuleInformation(process, pModuleHandles[i], &moduleInfo, sizeof(moduleInfo));

        if (!moduleBaseAddress)
        {
            moduleBaseAddress = moduleInfo.lpBaseOfDll;
        }

        char imageName[512] = {};
        GetModuleFileNameExA(process, pModuleHandles[i], imageName, sizeof(imageName));
        char moduleName[512] = {};
        GetModuleBaseNameA(process, pModuleHandles[i], moduleName, sizeof(moduleName));
        SymLoadModule64(process, 0, imageName, moduleName, (DWORD64)moduleInfo.lpBaseOfDll, moduleInfo.SizeOfImage);
    }

    CONTEXT      context = *(pExceptionInfo->ContextRecord);
    STACKFRAME64 stackFrame = {};
#ifdef _M_IX86
    stackFrame.AddrPC.Offset = context.Eip;
    stackFrame.AddrPC.Mode = AddrModeFlat;
    stackFrame.AddrStack.Offset = context.Esp;
    stackFrame.AddrStack.Mode = AddrModeFlat;
    stackFrame.AddrFrame.Offset = context.Ebp;
    stackFrame.AddrFrame.Mode = AddrModeFlat;
#else
    stackFrame.AddrPC.Offset = context.Rip;
    stackFrame.AddrPC.Mode = AddrModeFlat;
    stackFrame.AddrStack.Offset = context.Rsp;
    stackFrame.AddrStack.Mode = AddrModeFlat;
    stackFrame.AddrFrame.Offset = context.Rbp;
    stackFrame.AddrFrame.Mode = AddrModeFlat;
#endif

    IMAGE_NT_HEADERS* pImageHeader = ImageNtHeader(moduleBaseAddress);
    DWORD             imageType = pImageHeader->FileHeader.Machine;

    const DWORD                        maxLength = 1024;
    alignas(IMAGEHLP_SYMBOL64) uint8_t symbolMem[sizeof(IMAGEHLP_SYMBOL64) + maxLength];
    IMAGEHLP_SYMBOL64*                 pSymbol = (IMAGEHLP_SYMBOL64*)symbolMem;

    DWORD                      maxFunctionNameLength = 0;
    int                        maxNumLines = 100;
    int                        numLines = 0;
    WindowsStackTraceLineInfo* stackTraceLines =
        (WindowsStackTraceLineInfo*)WindowsStackTrace::Alloc(maxNumLines * sizeof(*stackTraceLines));
    if (!stackTraceLines)
    {
        Log("Failed to allocate WindowsStackTraceLineInfo");
        return EXCEPTION_EXECUTE_HANDLER;
    }

    do
    {
        if (!StackWalk64(imageType, process, thread, &stackFrame, &context, NULL, &SymFunctionTableAccess64, &SymGetModuleBase64, NULL))
            break;

        if (stackFrame.AddrPC.Offset != 0)
        {
            if (numLines >= maxNumLines)
            {
                Log("You need to allocate more memory for stackTraceLines");
                break;
            }

            WindowsStackTraceLineInfo& lineInfo = stackTraceLines[numLines++];
            memset(&lineInfo, 0, sizeof(lineInfo));

            memset(pSymbol, 0, sizeof(symbolMem));
            pSymbol->SizeOfStruct = sizeof(*pSymbol);
            pSymbol->MaxNameLength = maxLength;
            DWORD64 displacement = 0;
            SymGetSymFromAddr64(process, stackFrame.AddrPC.Offset, &displacement, pSymbol);
            DWORD functionNameLength =
                UnDecorateSymbolName(pSymbol->Name, lineInfo.mFunctionName, sizeof(lineInfo.mFunctionName), UNDNAME_COMPLETE);
            if (maxFunctionNameLength < functionNameLength)
                maxFunctionNameLength = functionNameLength;

            IMAGEHLP_LINE64 line = {};
            line.SizeOfStruct = sizeof(line);
            DWORD offsetFromSymbol = 0;
#ifdef _M_IX86
            SymGetLineFromAddr64(process, stackFrame.AddrPC.Offset, &offsetFromSymbol, &line);
#else
            SymGetLineFromAddr(process, stackFrame.AddrPC.Offset, &offsetFromSymbol, &line);
#endif
            if (line.FileName)
                strcpy(lineInfo.mFileName, line.FileName);
            lineInfo.mLineNumber = line.LineNumber;

            IMAGEHLP_MODULE64 module = {};
            module.SizeOfStruct = sizeof(module);
            if (SymGetModuleInfo64(process, stackFrame.AddrPC.Offset, &module))
            {
                size_t lastSlash = 0;
                for (size_t i = 0; i < sizeof(module.ImageName); i++)
                {
                    if (module.ImageName[i] == '\0')
                        break;

                    if (module.ImageName[i] == '/' || module.ImageName[i] == '\\')
                        lastSlash = i + 1;
                }

                strncpy(lineInfo.mModuleName, &module.ImageName[lastSlash], sizeof(lineInfo.mModuleName));
            }
        }
    } while (stackFrame.AddrReturn.Offset != 0);

    for (int i = 0; i < numLines; ++i)
    {
        DWORD                      padding = 5;
        WindowsStackTraceLineInfo& lineInfo = stackTraceLines[i];
        Log("%-*s | %s!%s(%d)", maxFunctionNameLength + padding, lineInfo.mFunctionName, lineInfo.mModuleName, lineInfo.mFileName,
            lineInfo.mLineNumber);
    }

    SymCleanup(process);

    return EXCEPTION_EXECUTE_HANDLER;
}
#else

bool WindowsStackTrace::Init() { return false; }

void WindowsStackTrace::Exit() {}

LONG WindowsStackTrace::Dump(EXCEPTION_POINTERS* pExceptionInfo)
{
    UNREF_PARAM(pExceptionInfo);
    return EXCEPTION_EXECUTE_HANDLER;
}

#endif // ENABLE_FORGE_STACKTRACE_DUMP
