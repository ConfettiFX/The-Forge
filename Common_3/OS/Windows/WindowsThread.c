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

#include "../../Application/Config.h"

#if defined(_WINDOWS) || defined(XBOX)

#include "../../Utilities/Interfaces/ILog.h"
#include "../../Utilities/Interfaces/IThread.h"
#include "../Interfaces/IOperatingSystem.h"

#include "../../Utilities/Interfaces/IMemory.h"

#if defined(ENABLE_THREAD_PERFORMANCE_STATS)

#if defined(XBOX)
#include <processthreadsapi.h>

int      gThreadIds[MAX_PERFORMANCE_STATS_CORES];
uint64_t gThreadWorkTimes[MAX_PERFORMANCE_STATS_CORES];
uint64_t gPrevThreadWorkTimes[MAX_PERFORMANCE_STATS_CORES];
uint64_t gLastSamplingTime;
Mutex    gThreadTimesMutex;

#else

#include <Wbemidl.h>
#pragma comment(lib, "wbemuuid.lib")

IWbemLocator*  gWbemLocator = NULL;
IWbemServices* gWbemService = NULL;
uint64_t       pOldTimeStamp[MAX_PERFORMANCE_STATS_CORES];
uint64_t       pOldPprocUsage[MAX_PERFORMANCE_STATS_CORES];

#endif

#endif // ENABLE_THREAD_PERFORMANCE_STATS

#include <process.h> // _beginthreadex

/*
 * Function pointer can't be casted to void*,
 * so we wrap it with a struct
 */
typedef struct CallOnceFnWrapper
{
    CallOnceFn fn;
} CallOnceFnWrapper;

static BOOL callOnceImpl(PINIT_ONCE initOnce, PVOID pWrapper, PVOID* ppContext)
{
    UNREF_PARAM(initOnce);
    UNREF_PARAM(ppContext);
    CallOnceFn fn = ((CallOnceFnWrapper*)pWrapper)->fn;
    if (fn)
        fn();

    return TRUE;
}

void callOnce(CallOnceGuard* pGuard, CallOnceFn fn)
{
    CallOnceFnWrapper wrapper = { fn };
    InitOnceExecuteOnce(pGuard, callOnceImpl, &wrapper, NULL);
}

bool initMutex(Mutex* mutex)
{
    return InitializeCriticalSectionAndSpinCount((CRITICAL_SECTION*)&mutex->mHandle, (DWORD)MUTEX_DEFAULT_SPIN_COUNT);
}

void destroyMutex(Mutex* mutex)
{
    CRITICAL_SECTION* cs = (CRITICAL_SECTION*)&mutex->mHandle;
    DeleteCriticalSection(cs);
    memset(&mutex->mHandle, 0, sizeof(mutex->mHandle));
}

void acquireMutex(Mutex* mutex) { EnterCriticalSection((CRITICAL_SECTION*)&mutex->mHandle); }

bool tryAcquireMutex(Mutex* mutex) { return TryEnterCriticalSection((CRITICAL_SECTION*)&mutex->mHandle); }

void releaseMutex(Mutex* mutex) { LeaveCriticalSection((CRITICAL_SECTION*)&mutex->mHandle); }

bool initConditionVariable(ConditionVariable* cv)
{
    cv->pHandle = (CONDITION_VARIABLE*)tf_calloc(1, sizeof(CONDITION_VARIABLE));
    InitializeConditionVariable((PCONDITION_VARIABLE)cv->pHandle);
    return true;
}

void destroyConditionVariable(ConditionVariable* cv) { tf_free(cv->pHandle); }

void waitConditionVariable(ConditionVariable* cv, Mutex* pMutex, uint32_t ms)
{
    SleepConditionVariableCS((PCONDITION_VARIABLE)cv->pHandle, (PCRITICAL_SECTION)&pMutex->mHandle, ms);
}

void wakeOneConditionVariable(ConditionVariable* cv) { WakeConditionVariable((PCONDITION_VARIABLE)cv->pHandle); }

void wakeAllConditionVariable(ConditionVariable* cv) { WakeAllConditionVariable((PCONDITION_VARIABLE)cv->pHandle); }

static ThreadID mainThreadID = 0;

void setMainThread() { mainThreadID = getCurrentThreadID(); }

ThreadID getCurrentThreadID() { return GetCurrentThreadId(); /* Windows built-in function*/ }

char* thread_name()
{
    __declspec(thread) static char name[MAX_THREAD_NAME_LENGTH + 1];
    return name;
}

void getCurrentThreadName(char* buffer, int size)
{
    const char* name = thread_name();
    if (name[0])
        snprintf(buffer, (size_t)size, "%s", name);
    else
        buffer[0] = 0;
}

void setCurrentThreadName(const char* name) { strcpy_s(thread_name(), MAX_THREAD_NAME_LENGTH + 1, name); }

bool isMainThread() { return getCurrentThreadID() == mainThreadID; }

typedef int(__cdecl* SETTHREADDESCFUNC)(HANDLE, PCWSTR);

unsigned WINAPI ThreadFunctionStatic(void* data)
{
    ThreadDesc item = *((ThreadDesc*)(data));
    tf_free(data);

    if (item.mThreadName[0] != 0)
    {
        // Local TheForge thread name, used for logging
        setCurrentThreadName(item.mThreadName);

#ifdef _WINDOWS
        HINSTANCE hinstLib = GetModuleHandle(TEXT("KernelBase.dll"));
        if (hinstLib != NULL)
        {
            SETTHREADDESCFUNC ProcAdd = (SETTHREADDESCFUNC)GetProcAddress(hinstLib, "SetThreadDescription");
            if (ProcAdd != NULL)
            {
                WCHAR windowsThreadName[sizeof(item.mThreadName)] = { 0 };
                mbstowcs(windowsThreadName, item.mThreadName, strlen(item.mThreadName) + 1);
                HRESULT res = ProcAdd(GetCurrentThread(), windowsThreadName);
                ASSERT(!FAILED(res));
            }
        }
#endif
    }

    if (item.setAffinityMask)
    {
        uint32_t nCores = getNumCPUCores();

        for (uint32_t groupId = 0; groupId < nCores / 64; ++groupId)
        {
            GROUP_AFFINITY groupAffinity = { 0 };

            groupAffinity.Mask = item.affinityMask[groupId];
            groupAffinity.Group = (WORD)groupId;

            BOOL res = SetThreadGroupAffinity(GetCurrentThread(), &groupAffinity, NULL);
            if (res != 0)
            {
                LOGF(eERROR, "Failed to set affinity for thread %s for CPU group %u: 0x%x", item.mThreadName, groupId, res);
            }
        }
    }

    item.pFunc(item.pData);
    return 0;
}

void threadSleep(unsigned mSec) { Sleep(mSec); }

bool initThread(ThreadDesc* pDesc, ThreadHandle* pHandle)
{
    ASSERT(pHandle);
    *pHandle = NULL;

    // Copy the contents of ThreadDesc because if the variable is in the stack we might access corrupted data.
    ThreadDesc* pDescCopy = (ThreadDesc*)tf_malloc(sizeof(ThreadDesc));
    *pDescCopy = *pDesc;

    *pHandle = (ThreadHandle)_beginthreadex(0, 0, ThreadFunctionStatic, pDescCopy, 0, 0);
    return *pHandle != NULL;
}

void joinThread(ThreadHandle handle)
{
    if (!handle)
        return;
    WaitForSingleObject((HANDLE)handle, INFINITE);
    CloseHandle((HANDLE)handle);
}

void detachThread(ThreadHandle handle)
{
    if (!handle)
        return;
    CloseHandle((HANDLE)handle);
}

unsigned int getNumCPUCores(void)
{
    struct _SYSTEM_INFO systemInfo;
    GetSystemInfo(&systemInfo);
    return systemInfo.dwNumberOfProcessors;
}

// Performance stats functions
#if defined(ENABLE_THREAD_PERFORMANCE_STATS)

int initPerformanceStats(PerformanceStatsFlags flags)
{
    UNREF_PARAM(flags);
#if defined(XBOX)
    for (uint32_t i = 0; i < getNumCPUCores(); ++i)
    {
        gThreadIds[i] = -1;
        gThreadWorkTimes[i] = 0;
        gPrevThreadWorkTimes[i] = 0;
    }

    FILETIME create, exit, user, kernel;
    GetProcessTimes(GetCurrentProcess(), &create, &exit, &kernel, &user);
    uint64_t lowPart = create.dwLowDateTime;
    uint64_t highPart = create.dwHighDateTime;
    uint64_t creationTime = lowPart | (highPart << 32uLL);

    gLastSamplingTime = creationTime;

    initMutex(&gThreadTimesMutex);

#else
    HRESULT hres;

    CoInitializeEx(0, COINIT_MULTITHREADED);
    CoInitializeSecurity(NULL, -1, NULL, NULL, RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE, NULL);

    FILETIME currSysTime;
    GetSystemTimeAsFileTime(&currSysTime);
    ULARGE_INTEGER uli;
    uli.LowPart = currSysTime.dwLowDateTime;
    uli.HighPart = currSysTime.dwHighDateTime;

    for (uint32_t i = 0; i < getNumCPUCores(); i++)
    {
        pOldTimeStamp[i] = uli.QuadPart;
        pOldPprocUsage[i] = 0;
    }

    hres = CoCreateInstance(&CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER, &IID_IWbemLocator, (LPVOID*)&gWbemLocator);
    if (FAILED(hres))
    {
        CoUninitialize();
        return -1;
    }

    BSTR resString = SysAllocString(L"ROOT\\CIMV2");
    hres = gWbemLocator->lpVtbl->ConnectServer(gWbemLocator, resString, NULL, NULL, NULL, 0, NULL, NULL, &gWbemService);
    SysFreeString(resString);

    if (FAILED(hres))
    {
        gWbemLocator->lpVtbl->Release(gWbemLocator);
        CoUninitialize();
        return -1;
    }

#endif
    return 1;
}

void updatePerformanceStats()
{
#if defined(XBOX)
    // Update current thread times
    HANDLE currThread = GetCurrentThread();
    DWORD  threadId = GetCurrentThreadId();
    DWORD  coreId = GetCurrentProcessorNumber();

    FILETIME creation, exit, user, kernel;
    GetThreadTimes(currThread, &creation, &exit, &kernel, &user);

    uint32_t threadIdx = (uint32_t)-1;
    for (uint32_t i = 0; i < getNumCPUCores(); i++)
    {
        if (gThreadIds[i] == (int)threadId)
        {
            threadIdx = i;
            break;
        }
        else if (gThreadIds[i] == -1)
        {
            gThreadIds[i] = threadId;
            threadIdx = i;
            break;
        }
    }
    ASSERT(threadIdx != -1);

    {
        acquireMutex(&gThreadTimesMutex);

        uint64_t lowPart = (uint64_t)user.dwLowDateTime;
        uint64_t highPart = (uint64_t)user.dwHighDateTime;
        uint64_t userTime = (lowPart | (highPart << 32uLL));

        lowPart = (uint64_t)kernel.dwLowDateTime;
        highPart = (uint64_t)kernel.dwHighDateTime;
        uint64_t kernelTime = (lowPart | (highPart << 32uLL));
        // Add time elapsed since last sampling
        gThreadWorkTimes[coreId] += (kernelTime + userTime) - gPrevThreadWorkTimes[threadIdx];

        // Update total user / kernel times
        gPrevThreadWorkTimes[threadIdx] = userTime + kernelTime;

        releaseMutex(&gThreadTimesMutex);
    }
#endif
}

PerformanceStats getPerformanceStats()
{
    PerformanceStats ret = { { 0 } };
    for (uint32_t i = 0; i < getNumCPUCores(); i++)
        ret.mCoreUsagePercentage[i] = -1;

#if defined(XBOX)
    FILETIME currTime;
    GetSystemTimeAsFileTime(&currTime);

    uint64_t lowPart = currTime.dwLowDateTime;
    uint64_t highPart = currTime.dwHighDateTime;
    uint64_t processTime = lowPart | (highPart << 32uLL);

    // Time elapsed since last sampling
    uint64_t elapsedTime = processTime - gLastSamplingTime;
    {
        acquireMutex(&gThreadTimesMutex);

        for (uint32_t i = 0; i < getNumCPUCores(); ++i)
        {
            ret.mCoreUsagePercentage[i] = (100.0f * ((float)gThreadWorkTimes[i] / elapsedTime));
            gThreadWorkTimes[i] = 0;
        }

        releaseMutex(&gThreadTimesMutex);
    }

    gLastSamplingTime = processTime;
#else
    IEnumWbemClassObject* pEnumerator;
    BSTR                  wql = SysAllocString(L"WQL");
    BSTR                  query =
        SysAllocString(L"SELECT TimeStamp_Sys100NS, PercentProcessorTime, Frequency_PerfTime FROM Win32_PerfRawData_PerfOS_Processor");

    HRESULT hres = gWbemService->lpVtbl->ExecQuery(gWbemService, wql, query, WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL,
                                                   &pEnumerator);
    SysFreeString(wql);
    SysFreeString(query);

    if (FAILED(hres))
    {
        gWbemService->lpVtbl->Release(gWbemService);
        gWbemLocator->lpVtbl->Release(gWbemLocator);
        CoUninitialize();

        return ret;
    }

    IWbemClassObject* pclsObj = NULL;

    ULONG i = 0;
    ULONG retVal;

    while (pEnumerator)
    {
        // Waiting for inifinite blocks resources and app.
        // Waiting for 15 ms (arbitrary) instead works much better
        pEnumerator->lpVtbl->Next(pEnumerator, 15, 1, &pclsObj, &retVal);
        if (!retVal)
        {
            break;
        }

        VARIANT vtPropTime;
        VARIANT vtPropClock;
        VariantInit(&vtPropTime);
        VariantInit(&vtPropClock);

        BSTR timeStamp = SysAllocString(L"TimeStamp_Sys100NS");
        BSTR procTime = SysAllocString(L"PercentProcessorTime");

        pclsObj->lpVtbl->Get(pclsObj, timeStamp, 0, &vtPropTime, 0, 0);
        UINT64 newTimeStamp = _wtoi64(vtPropTime.bstrVal);

        pclsObj->lpVtbl->Get(pclsObj, procTime, 0, &vtPropClock, 0, 0);
        UINT64 newPProcUsage = _wtoi64(vtPropClock.bstrVal);

        SysFreeString(timeStamp);
        SysFreeString(procTime);

        ret.mCoreUsagePercentage[i] =
            (float)(1.0 - (((double)newPProcUsage - (double)pOldPprocUsage[i]) / ((double)newTimeStamp - (double)pOldTimeStamp[i]))) *
            100.0f;

        pOldPprocUsage[i] = newPProcUsage;
        pOldTimeStamp[i] = newTimeStamp;

        VariantClear(&vtPropTime);
        VariantClear(&vtPropClock);

        pclsObj->lpVtbl->Release(pclsObj);
        i++;
    }
#endif

    return ret;
}

void exitPerformanceStats()
{
#if !defined(XBOX)
    gWbemService->lpVtbl->Release(gWbemService);
    gWbemLocator->lpVtbl->Release(gWbemLocator);
    CoUninitialize();
#endif
}
#endif // ENABLE_THREAD_PERFORMANCE_STATS
#endif
