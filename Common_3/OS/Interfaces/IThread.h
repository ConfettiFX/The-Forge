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
#include "../../ThirdParty/OpenSource/TinySTL/vector.h"

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

/// Operating system mutual exclusion primitive.
struct Mutex
{
	Mutex();
	~Mutex();

	void Acquire();
	void Release();

#ifdef _WIN32
	void* pHandle;
#else
	pthread_mutex_t pHandle;
#endif
};

struct MutexLock
{
	MutexLock(Mutex& mutex);
	~MutexLock();

	/// Prevent copy construction.
	MutexLock(const MutexLock& rhs) = delete;
	/// Prevent assignment.
	MutexLock& operator=(const MutexLock& rhs) = delete;

	Mutex& mMutex;
};

struct ConditionVariable
{
	ConditionVariable();
	~ConditionVariable();

	void Wait(const Mutex& mutex, unsigned md);
	void Set();

#ifdef _WIN32
	void* pHandle;
#else
	pthread_cond_t  pHandle;
#endif
};

typedef void (*JobFunction)(void*);

/// Work queue item.
struct WorkItem
{
	// Construct
	WorkItem(): pFunc(0), pData(0), mPriority(0), mCompleted(false) {}

	/// Work item description and thread index (Main thread => 0)
	JobFunction   pFunc;
	void*         pData;
	unsigned      mPriority;
	volatile bool mCompleted;
};

#ifndef _WIN32
/// Forward declaration
struct Thread;
#endif

/// Work queue subsystem for multithreading.
class ThreadPool
{
	public:
	/// Construct.
	ThreadPool();
	/// Destruct.
	~ThreadPool();

	/// Can only be called once during lifetime of program
	void     CreateThreads(unsigned numThreads);
	void     AddWorkItem(WorkItem* item);
	bool     RemoveWorkItem(WorkItem*& item);
	unsigned RemoveWorkItems(const tinystl::vector<WorkItem*>& items);
	void     Pause();
	void     Resume();
	void     Shutdown() { mShutDown = true; }
	void     Complete(unsigned priority);

	unsigned GetNumThreads() const { return (uint32_t)mThreads.size(); }
	bool     IsCompleted(unsigned priority) const;
	bool     IsCompleting() const { return mCompleting; }

	static void ProcessItems(void* pThreadSystem);

	private:
	void Cleanup(unsigned priority);

	tinystl::vector<struct Thread*> mThreads;
	tinystl::vector<WorkItem*>      mWorkItems;
	tinystl::vector<WorkItem*>      mWorkQueue;
	Mutex                           mQueueMutex;
	ConditionVariable               mWaitConditionVar;
	Mutex                           mWaitMutex;
	volatile bool                   mShutDown;
	volatile bool                   mPausing;
	bool                            mPaused;
	bool                            mCompleting;
};

#ifdef _WIN32
typedef void* ThreadHandle;
#else
typedef pthread_t ThreadHandle;
#endif

ThreadHandle create_thread(WorkItem* pItem);
void         destroy_thread(ThreadHandle handle);
void         join_thread(ThreadHandle handle);

struct Thread
{
	Thread(ThreadPool* threadSystem);
	~Thread();

	ThreadHandle pHandle;
	WorkItem*    pItem;

	static ThreadID mainThreadID;

	static void         SetMainThread();
	static ThreadID     GetCurrentThreadID();
	static bool         IsMainThread();
	static void         Sleep(unsigned mSec);
	static unsigned int GetNumCPUCores(void);
};

#endif
