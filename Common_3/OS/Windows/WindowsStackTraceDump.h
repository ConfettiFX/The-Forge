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

#pragma once
#include "../../Application/Config.h"

#include "../../OS/Interfaces/IOperatingSystem.h"
#include "../../Utilities/Interfaces/IThread.h"

struct WindowsStackTraceLineInfo
{
    char  mFunctionName[512];
    char  mModuleName[512];
    char  mFileName[512];
    DWORD mLineNumber;
};

class WindowsStackTrace
{
public:
    static bool Init();
    static void Exit();
    static LONG Dump(EXCEPTION_POINTERS* pExceptionInfo);

private:
#ifdef ENABLE_FORGE_STACKTRACE_DUMP
    static bool         mInit;
    static Mutex        mDbgHelpMutex;
    static const size_t mPreallocatedMemorySize = 1024LL * 1024LL;
    static uint8_t      mPreallocatedMemory[mPreallocatedMemorySize];
    static size_t       mUsedMemorySize;

    static void* Alloc(size_t size);
    static void  Log(const char* msg, ...);
#endif // ENABLE_FORGE_STACKTRACE_DUMP
};
