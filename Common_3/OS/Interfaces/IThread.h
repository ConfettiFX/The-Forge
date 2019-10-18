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

#include "../Interfaces/IOperatingSystem.h"
#include "../Math/MathTypes.h"

#ifndef _THREAD_H_
#define _THREAD_H_

#ifndef _WIN32
#include <pthread.h>
#endif

#ifndef _WIN32
#define ThreadID pthread_t
#else
typedef unsigned ThreadID;
#endif

#define TIMEOUT_INFINITE UINT32_MAX

/// Operating system mutual exclusion primitive.
struct Mutex
{
	static const uint32_t kDefaultSpinCount = 1500;
	
	bool Init(uint32_t spinCount = kDefaultSpinCount, const char* name = NULL);
	void Destroy();

	void Acquire();
	bool TryAcquire();
	void Release();

#ifdef _WIN32
	CRITICAL_SECTION mHandle;
#else
	pthread_mutex_t pHandle;
	uint32_t mSpinCount;
#endif
};

struct MutexLock
{
	MutexLock(Mutex& rhs) : mMutex(rhs) { rhs.Acquire(); }
	~MutexLock() { mMutex.Release(); }

	/// Prevent copy construction.
	MutexLock(const MutexLock& rhs) = delete;
	/// Prevent assignment.
	MutexLock& operator=(const MutexLock& rhs) = delete;

	Mutex& mMutex;
};

struct ConditionVariable
{
	bool Init(const char* name = NULL);
	void Destroy();

	void Wait(const Mutex& mutex, uint32_t md = TIMEOUT_INFINITE);
	void WakeOne();
	void WakeAll();

#ifdef _WIN32
	void* pHandle;
#else
	pthread_cond_t  pHandle;
#endif
};

typedef void (*ThreadFunction)(void*);

/// Work queue item.
struct ThreadDesc
{
	/// Work item description and thread index (Main thread => 0)
	ThreadFunction pFunc;
	void*          pData;
};

#ifdef _WIN32
typedef void* ThreadHandle;
#else
typedef pthread_t ThreadHandle;
#endif

ThreadHandle create_thread(ThreadDesc* pItem);
void         destroy_thread(ThreadHandle handle);
void         join_thread(ThreadHandle handle);

struct Thread
{
	static ThreadID     mainThreadID;
	static void         SetMainThread();
	static ThreadID     GetCurrentThreadID();
	static void         GetCurrentThreadName(char * buffer, int buffer_size);
	static void         SetCurrentThreadName(const char * name);
	static bool         IsMainThread();
	static void         Sleep(unsigned mSec);
	static unsigned int GetNumCPUCores(void);
};

// Max thread name should be 15 + null character
#ifndef MAX_THREAD_NAME_LENGTH
#define MAX_THREAD_NAME_LENGTH 15
#endif

#ifdef _WIN32
void sleep(unsigned mSec);
#endif

#endif
