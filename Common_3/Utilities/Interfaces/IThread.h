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

#include "../../OS/Interfaces/IOperatingSystem.h"

#ifndef _THREAD_H_
#define _THREAD_H_

#if defined(_WINDOWS) || defined(XBOX)
typedef unsigned long ThreadID;
#define THREAD_ID_MAX ULONG_MAX
#define THREAD_ID_MIN ((unsigned long)0)
#else
#include <pthread.h>
#if !defined(NX64)
#if !defined(__APPLE__) || defined(TARGET_IOS)
typedef uint32_t       ThreadID;
#endif
#define THREAD_ID_MAX UINT32_MAX
#define THREAD_ID_MIN ((uint32_t)0)
#endif // !NX64

#endif

#define INVALID_THREAD_ID 0

#if defined(_WINDOWS) || defined(XBOX)
#if !defined(THREAD_LOCAL)
#define THREAD_LOCAL __declspec(thread)
#endif
#define INIT_CALL_ONCE_GUARD INIT_ONCE_STATIC_INIT
typedef INIT_ONCE CallOnceGuard;
#else
#if !defined(THREAD_LOCAL)
#define THREAD_LOCAL __thread
#endif
#define INIT_CALL_ONCE_GUARD PTHREAD_ONCE_INIT
typedef pthread_once_t CallOnceGuard;
#endif

// Max thread name should be 15 + null character
#ifndef MAX_THREAD_NAME_LENGTH
#define MAX_THREAD_NAME_LENGTH 31
#endif

#define TIMEOUT_INFINITE UINT32_MAX

#ifdef __cplusplus
extern "C"
{
#endif
    typedef void (*CallOnceFn)(void);
    /*
     * Brief:
     *   Guaranties that CallOnceFn will be called once in a thread-safe way.
     * Notes:
     *   CallOnceGuard has to be a pointer to a global variable initialized with INIT_CALL_ONCE_GUARD
     */
    void callOnce(CallOnceGuard* pGuard, CallOnceFn pFn);

    /// Operating system mutual exclusion primitive.
    typedef struct Mutex
    {
#if defined(_WINDOWS) || defined(XBOX)
        CRITICAL_SECTION mHandle;
#elif defined(NX64)
    MutexTypeNX             mMutexPlatformNX;
    uint32_t                mSpinCount;
#else
    pthread_mutex_t pHandle;
    uint32_t        mSpinCount;
#endif
    } Mutex;

#define MUTEX_DEFAULT_SPIN_COUNT 1500

    FORGE_API bool initMutex(Mutex* pMutex);
    FORGE_API void exitMutex(Mutex* pMutex);

    FORGE_API void acquireMutex(Mutex* pMutex);
    FORGE_API bool tryAcquireMutex(Mutex* pMutex);
    FORGE_API void releaseMutex(Mutex* pMutex);

    typedef struct ConditionVariable
    {
#if defined(_WINDOWS) || defined(XBOX)
        void* pHandle;
#elif defined(NX64)
    ConditionVariableTypeNX mCondPlatformNX;
#else
    pthread_cond_t  pHandle;
#endif
    } ConditionVariable;

    FORGE_API bool initConditionVariable(ConditionVariable* cv);
    FORGE_API void exitConditionVariable(ConditionVariable* cv);

    FORGE_API void waitConditionVariable(ConditionVariable* cv, Mutex* pMutex, uint32_t msTimeout);
    FORGE_API void wakeOneConditionVariable(ConditionVariable* cv);
    FORGE_API void wakeAllConditionVariable(ConditionVariable* cv);

    typedef void (*ThreadFunction)(void*);

    /// Work queue item.
    typedef struct ThreadDesc
    {
        /// Work item description and thread index (Main thread => 0)
        ThreadFunction pFunc;
        void*          pData;
        char           mThreadName[MAX_THREAD_NAME_LENGTH];

        // Affinity is a bit mask. It tells on which CPU thread can run.
        // Maximum supported number of bits is 1024.
        // Use 'setCpuAffinity()' helper function repeatedly for convenience
        bool     setAffinityMask;
        uint64_t affinityMask[16];
    } ThreadDesc;

    static inline void setCpuAffinity(struct ThreadDesc* desc, uint64_t cpuId)
    {
        if (cpuId >= 1024)
            return;

        uint64_t vId = cpuId / 64;
        uint64_t bit = cpuId - vId * 64;

        desc->affinityMask[vId] |= ((uint64_t)1) << bit;

        desc->setAffinityMask = true;
    }

#if defined(_WINDOWS) || defined(XBOX)
    typedef void* ThreadHandle;
#elif !defined(NX64)
typedef pthread_t ThreadHandle;
#endif

    FORGE_API bool initThread(ThreadDesc* pItem, ThreadHandle* pHandle);
    FORGE_API void joinThread(ThreadHandle handle);
    FORGE_API void detachThread(ThreadHandle handle);

    FORGE_API void         setMainThread(void);
    FORGE_API ThreadID     getCurrentThreadID(void);
    FORGE_API void         getCurrentThreadName(char* buffer, int buffer_size);
    FORGE_API void         setCurrentThreadName(const char* name);
    FORGE_API bool         isMainThread(void);
    FORGE_API void         threadSleep(unsigned mSec);
    FORGE_API unsigned int getNumCPUCores(void);

    // Performance metrics functions
#if defined(ENABLE_THREAD_PERFORMANCE_STATS)
#define MAX_PERFORMANCE_STATS_CORES 64

    typedef struct PerformanceStats
    {
        float mCoreUsagePercentage[MAX_PERFORMANCE_STATS_CORES];
    } PerformanceStats;

    typedef enum PerformanceStatsFlags
    {
        PERFORMANCE_STATS_FLAG_CORE_USAGE_PERCENTAGE = 0x0,
    } PerformanceStatsFlags;

    FORGE_API int              initPerformanceStats(PerformanceStatsFlags flags);
    FORGE_API void             updatePerformanceStats(void);
    FORGE_API PerformanceStats getPerformanceStats(void);
    FORGE_API void             exitPerformanceStats(void);
#endif

#ifdef __cplusplus
} // extern "C"

struct MutexLock
{
    MutexLock(Mutex& rhs): mMutex(rhs) { acquireMutex(&rhs); }
    ~MutexLock() { releaseMutex(&mMutex); }

    /// Prevent copy construction.
    MutexLock(const MutexLock& rhs) = delete;
    /// Prevent assignment.
    MutexLock& operator=(const MutexLock& rhs) = delete;

    Mutex& mMutex;
};
#endif

#endif
