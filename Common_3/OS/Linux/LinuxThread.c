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

#ifdef __linux__

#define _GNU_SOURCE // sched_setaffinity

#include "../../Application/Config.h"

#include "../../Utilities/Interfaces/ILog.h"
#include "../../Utilities/Interfaces/IThread.h"
#include "../Interfaces/IOperatingSystem.h"

#include "../../Utilities/Threading/UnixThreadID.h"

#include "../../Utilities/Interfaces/IMemory.h"

#if defined(ENABLE_THREAD_PERFORMANCE_STATS)
#include <unistd.h>

uint64_t* pOldTimeStamp;
uint64_t* pOldProcUsage;

enum CPUStates
{
    S_USER = 0,
    S_NICE,
    S_SYSTEM,
    S_IDLE,
    S_IOWAIT,
    S_IRQ,
    S_SOFTIRQ,
    S_STEAL,
    S_GUEST,
    S_GUEST_NICE,
    NUM_CPU_STATES
};
typedef struct CPUData
{
    char   cpu[64];
    size_t times[NUM_CPU_STATES];
} CPUData;

size_t GetIdleTime(const CPUData* e) { return e->times[S_IDLE] + e->times[S_IOWAIT]; }

size_t GetActiveTime(const CPUData* e)
{
    return e->times[S_USER] + e->times[S_NICE] + e->times[S_SYSTEM] + e->times[S_IRQ] + e->times[S_SOFTIRQ] + e->times[S_STEAL] +
           e->times[S_GUEST] + e->times[S_GUEST_NICE];
}

#endif

#include <sys/time.h>

#define NSEC_PER_USEC 1000ull
#define USEC_PER_SEC  1000000ull
#define NSEC_PER_SEC  1000000000ull
#define NSEC_PER_MSEC 1000000ull

void callOnce(CallOnceGuard* pGuard, CallOnceFn pFn) { pthread_once(pGuard, pFn); }

bool initMutex(Mutex* pMutex)
{
    pMutex->mSpinCount = MUTEX_DEFAULT_SPIN_COUNT;
    pMutex->pHandle = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
    pthread_mutexattr_t attr;
    int                 status = pthread_mutexattr_init(&attr);
    status |= pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    status |= pthread_mutex_init(&pMutex->pHandle, &attr);
    status |= pthread_mutexattr_destroy(&attr);
    return status == 0;
}

void exitMutex(Mutex* pMutex) { pthread_mutex_destroy(&pMutex->pHandle); }

void acquireMutex(Mutex* pMutex)
{
    uint32_t count = 0;

    while (count < pMutex->mSpinCount && pthread_mutex_trylock(&pMutex->pHandle) != 0)
        ++count;

    if (count == pMutex->mSpinCount)
    {
        int r = pthread_mutex_lock(&pMutex->pHandle);
        ASSERT(r == 0 && "Mutex::Acquire failed to take the lock");
    }
}

bool tryAcquireMutex(Mutex* pMutex) { return pthread_mutex_trylock(&pMutex->pHandle) == 0; }

void releaseMutex(Mutex* pMutex) { pthread_mutex_unlock(&pMutex->pHandle); }

bool initConditionVariable(ConditionVariable* pCv)
{
    pCv->pHandle = (pthread_cond_t)PTHREAD_COND_INITIALIZER;
    int res = pthread_cond_init(&pCv->pHandle, NULL);
    ASSERT(res == 0);
    return res == 0;
}

void exitConditionVariable(ConditionVariable* pCv) { pthread_cond_destroy(&pCv->pHandle); }

void waitConditionVariable(ConditionVariable* pCv, Mutex* mutex, uint32_t ms)
{
    pthread_mutex_t* mutexHandle = (pthread_mutex_t*)&mutex->pHandle;
    if (ms == TIMEOUT_INFINITE)
    {
        pthread_cond_wait(&pCv->pHandle, mutexHandle);
        return;
    }

    // pthread_cond_timedwait won't do because it doesn't work as you expected!
    // Read docs please!
    releaseMutex(mutex);
    usleep(200);
    acquireMutex(mutex);
}

void wakeOneConditionVariable(ConditionVariable* pCv) { pthread_cond_signal(&pCv->pHandle); }

void wakeAllConditionVariable(ConditionVariable* pCv) { pthread_cond_broadcast(&pCv->pHandle); }

static ThreadID mainThreadID;

/*  void Thread::SetPriority(int priority)
{
      sched_param param;
      param.sched_priority = priority;
      pthread_setschedparam(pHandle, SCHED_OTHER, &param);
}*/

void setMainThread() { mainThreadID = getCurrentThreadID(); }

ThreadID getCurrentThreadID() { return getCurrentPthreadID(); }

int pthread_setname_np(pthread_t thread, const char* name);
int pthread_getname_np(pthread_t thread, char* name, size_t len);

void getCurrentThreadName(char* buffer, int buffer_size) { pthread_getname_np(pthread_self(), buffer, buffer_size); }

void setCurrentThreadName(const char* name) { pthread_setname_np(pthread_self(), name); }

bool isMainThread() { return getCurrentThreadID() == mainThreadID; }

void threadSleep(unsigned mSec) { usleep(mSec * 1000); }

static void* ThreadFunctionStatic(void* data)
{
    ThreadDesc item = *((ThreadDesc*)(data));
    tf_free(data);

    if (item.mThreadName[0] != 0)
        setCurrentThreadName(item.mThreadName);

    if (item.setAffinityMask)
    {
        size_t    affinityMaskSize = sizeof item.affinityMask;
        // We can't memcpy here, even when cpuset is exact copy of affinityMask.
        // By specs we should use CPU_SET macro family
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        for (uint64_t si = 0; si < affinityMaskSize; ++si)
        {
            uint8_t mask = *((uint8_t*)item.affinityMask + si);
            for (size_t bi = 0; bi < 8; ++bi)
            {
                if (mask & 1)
                    CPU_SET(si * 8 + bi, &cpuset);
                mask >>= 1;
            }
        }

        const int res = sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);
        ASSERT(res == 0);
    }

    item.pFunc(item.pData);
    return 0;
}

bool initThread(ThreadDesc* pData, ThreadHandle* pHandle)
{
    // Copy the contents of ThreadDesc because if the variable is in the stack we might access corrupted data.
    ThreadDesc* pDataCopy = (ThreadDesc*)tf_malloc(sizeof(ThreadDesc));
    if (pDataCopy == NULL)
        return false;

    *pDataCopy = *pData;

    int res = pthread_create(pHandle, NULL, ThreadFunctionStatic, pDataCopy);
    if (res)
        tf_free(pDataCopy);
    return res == 0;
}

void joinThread(ThreadHandle handle)
{
    pthread_join(handle, NULL);
    handle = (ThreadHandle)NULL;
}

void detachThread(ThreadHandle handle) { pthread_detach(handle); }

// Performance stats
unsigned int getNumCPUCores(void)
{
    unsigned int ncpu;
    ncpu = sysconf(_SC_NPROCESSORS_ONLN);
    return ncpu;
}

#if defined(ENABLE_THREAD_PERFORMANCE_STATS)

int initPerformanceStats(PerformanceStatsFlags flags)
{
    pOldTimeStamp = (uint64_t*)tf_malloc(sizeof(uint64_t) * getNumCPUCores());
    pOldProcUsage = (uint64_t*)tf_malloc(sizeof(uint64_t) * getNumCPUCores());

    return 0;
}

void updatePerformanceStats() {}

PerformanceStats getPerformanceStats()
{
    uint32_t         nCpuCores = getNumCPUCores();
    PerformanceStats ret = { { 0 } };
    for (uint32_t i = 0; i < nCpuCores; i++)
        ret.mCoreUsagePercentage[i] = -1;

#ifndef __ANDROID__
    // Open cpu stat file
    FILE* fh = fopen("/proc/stat", "rb");

    if (!fh)
    {
        return ret;
    }

    for (uint32_t i = 0; i < nCpuCores; i++)
    {
        // While eof not detected, keep parsing the stat file
        CPUData entry = {};
        if (!feof(fh))
        {
            char dummyCpuName[256]; // dummy cpu name, not used.
            int  _ret = fscanf(fh, "%s %zu %zu %zu %zu %zu %zu %zu %zu %zu %zu", &dummyCpuName[0], &entry.times[0], &entry.times[1],
                              &entry.times[2], &entry.times[3], &entry.times[4], &entry.times[5], &entry.times[6], &entry.times[7],
                              &entry.times[8], &entry.times[9]);
            (void)_ret;
        }
        float ACTIVE_TIME = (float)GetActiveTime(&entry);
        float IDLE_TIME = (float)GetIdleTime(&entry);

        ret.mCoreUsagePercentage[i] = (ACTIVE_TIME - pOldProcUsage[i]) / ((float)(IDLE_TIME + ACTIVE_TIME) - pOldTimeStamp[i]) * 100.0f;

        pOldProcUsage[i] = ACTIVE_TIME;
        pOldTimeStamp[i] = IDLE_TIME + ACTIVE_TIME;
    }

    // Close the cpu stat file
    fclose(fh);
#endif
    return ret;
}

void exitPerformanceStats()
{
    tf_free(pOldTimeStamp);
    tf_free(pOldProcUsage);
}

#endif // ENABLE_THREAD_PERFORMANCE_STATS
#endif // if __linux__
