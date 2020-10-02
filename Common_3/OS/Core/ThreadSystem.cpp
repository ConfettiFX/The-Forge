/*
 * Copyright (c) 2019 The Forge Interactive Inc.
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

#include "../Interfaces/IThread.h"
#include "../Interfaces/ILog.h"

#include "ThreadSystem.h"
#include "../Interfaces/IMemory.h"

struct ThreadedTask
{
	TaskFunc  mTask;
	void*     mUser;
	uintptr_t mStart;
	uintptr_t mEnd;
};

struct ThreadSystem
{
	ThreadDesc                 mThreadDescs[MAX_LOAD_THREADS];
	ThreadHandle               mThread[MAX_LOAD_THREADS];
	ThreadedTask			   mLoadTask[MAX_SYSTEM_TASKS];
	uint32_t				   mBegin, mEnd;
	ConditionVariable          mQueueCond;
	Mutex                      mQueueMutex;
	ConditionVariable          mIdleCond;
	uint32_t                   mNumLoaders;
	uint32_t                   mNumIdleLoaders;
	volatile bool              mRun;

#if defined(NX64)
	ThreadTypeNX			   mThreadType[MAX_LOAD_THREADS];
#endif
};

bool assistThreadSystemTasks(ThreadSystem* pThreadSystem, uint32_t* pIds, size_t count)
{
	pThreadSystem->mQueueMutex.Acquire();
	if (pThreadSystem->mBegin == pThreadSystem->mEnd)
	{
		pThreadSystem->mQueueMutex.Release();
		return false;
	}

	uint32_t taskSize = pThreadSystem->mBegin >= pThreadSystem->mEnd ? pThreadSystem->mBegin - pThreadSystem->mEnd : MAX_SYSTEM_TASKS - (pThreadSystem->mEnd - pThreadSystem->mBegin);
	ThreadedTask resourceTask;
	bool found = false;

	for (uint32_t i = 0; i < taskSize; ++i)
	{
		uint32_t index = (pThreadSystem->mEnd + i) % MAX_SYSTEM_TASKS;
		resourceTask = pThreadSystem->mLoadTask[index];

		for (size_t j = 0; j < count; ++j)
		{
			if (pIds[j] == resourceTask.mStart)
			{
				found = true;
				break;
			}
		}

		if (found)
		{
			if (resourceTask.mStart + 1 == resourceTask.mEnd)
			{
				pThreadSystem->mLoadTask[index] = pThreadSystem->mLoadTask[pThreadSystem->mEnd];
				++pThreadSystem->mEnd;
				pThreadSystem->mEnd = pThreadSystem->mEnd % MAX_SYSTEM_TASKS;
			}
			else
			{
				++pThreadSystem->mLoadTask[index].mStart;
			}
			break;
		}
	}

	pThreadSystem->mQueueMutex.Release();

	if (!found)
	{
		return false;
	}

	resourceTask.mTask(resourceTask.mUser, resourceTask.mStart);
	return true;
}

bool assistThreadSystem(ThreadSystem* pThreadSystem)
{
	pThreadSystem->mQueueMutex.Acquire();
	if (pThreadSystem->mBegin != pThreadSystem->mEnd)
	{
		ThreadedTask resourceTask = pThreadSystem->mLoadTask[pThreadSystem->mEnd];
		if (resourceTask.mStart + 1 == resourceTask.mEnd)
		{
			++pThreadSystem->mEnd;
			pThreadSystem->mEnd = pThreadSystem->mEnd % MAX_SYSTEM_TASKS;
		}
		else
		{
			++pThreadSystem->mLoadTask[pThreadSystem->mEnd].mStart;
		}
		pThreadSystem->mQueueMutex.Release();
		resourceTask.mTask(resourceTask.mUser, resourceTask.mStart);

		return true;
	}
	else
	{
		pThreadSystem->mQueueMutex.Release();
		return false;
	}
}

static void taskThreadFunc(void* pThreadData)
{
	ThreadSystem* pThreadSystem = (ThreadSystem*)pThreadData;
	while (pThreadSystem->mRun)
	{
		pThreadSystem->mQueueMutex.Acquire();
		++pThreadSystem->mNumIdleLoaders;
		while (pThreadSystem->mRun && pThreadSystem->mBegin == pThreadSystem->mEnd)
		{
			pThreadSystem->mIdleCond.WakeAll();
			pThreadSystem->mQueueCond.Wait(pThreadSystem->mQueueMutex);
		}
		--pThreadSystem->mNumIdleLoaders;
		if (pThreadSystem->mBegin != pThreadSystem->mEnd)
		{
			ThreadedTask resourceTask = pThreadSystem->mLoadTask[pThreadSystem->mEnd];// pThreadSystem->mLoadQueue.front();
			if (resourceTask.mStart + 1 == resourceTask.mEnd)
			{
				++pThreadSystem->mEnd;
				pThreadSystem->mEnd = pThreadSystem->mEnd % MAX_SYSTEM_TASKS;
			}
			else
			{
				++pThreadSystem->mLoadTask[pThreadSystem->mEnd].mStart;
			}
			pThreadSystem->mQueueMutex.Release();
			resourceTask.mTask(resourceTask.mUser, resourceTask.mStart);
		}
		else
		{
			pThreadSystem->mQueueMutex.Release();
		}
	}
	pThreadSystem->mQueueMutex.Acquire();
	++pThreadSystem->mNumIdleLoaders;
	pThreadSystem->mIdleCond.WakeAll();
	pThreadSystem->mQueueMutex.Release();
}

void initThreadSystem(ThreadSystem** ppThreadSystem, uint32_t numRequestedThreads, int preferredCore, bool migrateEnabled, const char* threadName)
{
	ThreadSystem* pThreadSystem = tf_new(ThreadSystem);

	uint32_t numThreads = max<uint32_t>(Thread::GetNumCPUCores() - 1, 1);
	uint32_t numLoaders = min<uint32_t>(numThreads, min<uint32_t>(numRequestedThreads, MAX_LOAD_THREADS));

	pThreadSystem->mQueueMutex.Init();
	pThreadSystem->mQueueCond.Init();
	pThreadSystem->mIdleCond.Init();
	
	pThreadSystem->mRun = true;
	pThreadSystem->mNumIdleLoaders = 0;
	pThreadSystem->mBegin = 0;
	pThreadSystem->mEnd = 0;

	for (unsigned i = 0; i < numLoaders; ++i)
	{
		pThreadSystem->mThreadDescs[i].pFunc = taskThreadFunc;
		pThreadSystem->mThreadDescs[i].pData = pThreadSystem;

#if defined(NX64)
		pThreadSystem->mThreadDescs[i].pThreadStack = aligned_alloc(THREAD_STACK_ALIGNMENT_NX, ALIGNED_THREAD_STACK_SIZE_NX);
		pThreadSystem->mThreadDescs[i].hThread = &pThreadSystem->mThreadType[i];
		pThreadSystem->mThreadDescs[i].preferredCore = preferredCore;
		pThreadSystem->mThreadDescs[i].pThreadName = threadName;
		pThreadSystem->mThreadDescs[i].migrateEnabled = migrateEnabled;
#endif

		pThreadSystem->mThread[i] = create_thread(&pThreadSystem->mThreadDescs[i]);
	}
	pThreadSystem->mNumLoaders = numLoaders;

	*ppThreadSystem = pThreadSystem;
}

void addThreadSystemTask(ThreadSystem* pThreadSystem, TaskFunc task, void* user, uintptr_t index)
{
	pThreadSystem->mQueueMutex.Acquire();
	pThreadSystem->mLoadTask[pThreadSystem->mBegin++] = ThreadedTask{ task, user, index, index + 1 };
	pThreadSystem->mBegin = pThreadSystem->mBegin % MAX_SYSTEM_TASKS;
	LOGF_IF(LogLevel::eERROR, pThreadSystem->mBegin == pThreadSystem->mEnd, "Maximum amount of thread task reached: mBegin (%d), mEnd(%d), Max(%d)", pThreadSystem->mBegin, pThreadSystem->mEnd, MAX_SYSTEM_TASKS);
	ASSERT(pThreadSystem->mBegin != pThreadSystem->mEnd);
	pThreadSystem->mQueueMutex.Release();
	pThreadSystem->mQueueCond.WakeAll();
}

uint32_t getThreadSystemThreadCount(ThreadSystem* pThreadSystem)
{
	return pThreadSystem->mNumLoaders;
}

void addThreadSystemRangeTask(ThreadSystem* pThreadSystem, TaskFunc task, void* user, uintptr_t count)
{
	pThreadSystem->mQueueMutex.Acquire();
	pThreadSystem->mLoadTask[pThreadSystem->mBegin++] = ThreadedTask{ task, user, 0, count };
	pThreadSystem->mBegin = pThreadSystem->mBegin % MAX_SYSTEM_TASKS;
	LOGF_IF(LogLevel::eERROR, pThreadSystem->mBegin == pThreadSystem->mEnd, "Maximum amount of thread task reached: mBegin (%d), mEnd(%d), Max(%d)", pThreadSystem->mBegin, pThreadSystem->mEnd, MAX_SYSTEM_TASKS);
	ASSERT(pThreadSystem->mBegin != pThreadSystem->mEnd);
	pThreadSystem->mQueueMutex.Release();
	pThreadSystem->mQueueCond.WakeAll();
}

void addThreadSystemRangeTask(ThreadSystem* pThreadSystem, TaskFunc task, void* user, uintptr_t start, uintptr_t end)
{
	pThreadSystem->mQueueMutex.Acquire();
	pThreadSystem->mLoadTask[pThreadSystem->mBegin++] = ThreadedTask{ task, user, start, end };
	pThreadSystem->mBegin = pThreadSystem->mBegin % MAX_SYSTEM_TASKS;
	LOGF_IF(LogLevel::eERROR, pThreadSystem->mBegin == pThreadSystem->mEnd, "Maximum amount of thread task reached: mBegin (%d), mEnd(%d), Max(%d)", pThreadSystem->mBegin, pThreadSystem->mEnd, MAX_SYSTEM_TASKS);
	ASSERT(pThreadSystem->mBegin != pThreadSystem->mEnd);
	pThreadSystem->mQueueMutex.Release();
	pThreadSystem->mQueueCond.WakeAll();
}

void shutdownThreadSystem(ThreadSystem* pThreadSystem)
{
	pThreadSystem->mQueueMutex.Acquire();
	pThreadSystem->mRun = false;
	pThreadSystem->mQueueMutex.Release();
	pThreadSystem->mQueueCond.WakeAll();
	pThreadSystem->mBegin = 0;
	pThreadSystem->mEnd = 0;

	uint32_t numLoaders = pThreadSystem->mNumLoaders;
	for (uint32_t i = 0; i < numLoaders; ++i)
	{
		destroy_thread(pThreadSystem->mThread[i]);
	}

	pThreadSystem->mQueueCond.Destroy();
	pThreadSystem->mIdleCond.Destroy();
	pThreadSystem->mQueueMutex.Destroy();
	tf_delete(pThreadSystem);
}

bool isThreadSystemIdle(ThreadSystem* pThreadSystem)
{
	pThreadSystem->mQueueMutex.Acquire();
	bool idle = (pThreadSystem->mBegin == pThreadSystem->mEnd && pThreadSystem->mNumIdleLoaders == pThreadSystem->mNumLoaders) || !pThreadSystem->mRun;
	pThreadSystem->mQueueMutex.Release();
	return idle;
}

void waitThreadSystemIdle(ThreadSystem* pThreadSystem)
{
	pThreadSystem->mQueueMutex.Acquire();
	while ((pThreadSystem->mBegin != pThreadSystem->mEnd || pThreadSystem->mNumIdleLoaders < pThreadSystem->mNumLoaders) && pThreadSystem->mRun)
		pThreadSystem->mIdleCond.Wait(pThreadSystem->mQueueMutex);
	pThreadSystem->mQueueMutex.Release();
}
