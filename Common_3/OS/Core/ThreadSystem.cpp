/*
 * Copyright (c) 2019 Confetti Interactive Inc.
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

#include "../../ThirdParty/OpenSource/EASTL/deque.h"

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

enum
{
	MAX_LOAD_THREADS = 16
};

struct ThreadSystem
{
	ThreadDesc                 mThreadDescs[MAX_LOAD_THREADS];
	ThreadHandle               mThread[MAX_LOAD_THREADS];
	eastl::deque<ThreadedTask> mLoadQueue;
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

bool assistThreadSystem(ThreadSystem* pThreadSystem)
{
	pThreadSystem->mQueueMutex.Acquire();
	if (!pThreadSystem->mLoadQueue.empty())
	{
		ThreadedTask resourceTask = pThreadSystem->mLoadQueue.front();
		if (resourceTask.mStart + 1 == resourceTask.mEnd)
			pThreadSystem->mLoadQueue.pop_front();
		else
			++pThreadSystem->mLoadQueue.front().mStart;
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
		while (pThreadSystem->mRun && pThreadSystem->mLoadQueue.empty())
		{
			pThreadSystem->mIdleCond.WakeAll();
			pThreadSystem->mQueueCond.Wait(pThreadSystem->mQueueMutex);
		}
		--pThreadSystem->mNumIdleLoaders;
		if (!pThreadSystem->mLoadQueue.empty())
		{
			ThreadedTask resourceTask = pThreadSystem->mLoadQueue.front();
			if (resourceTask.mStart + 1 == resourceTask.mEnd)
				pThreadSystem->mLoadQueue.pop_front();
			else
				++pThreadSystem->mLoadQueue.front().mStart;
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

void initThreadSystem(ThreadSystem** ppThreadSystem)
{
	ThreadSystem* pThreadSystem = conf_new(ThreadSystem);

	uint32_t numThreads = max<uint32_t>(Thread::GetNumCPUCores() - 1, 1);
	uint32_t numLoaders = min<uint32_t>(numThreads, MAX_LOAD_THREADS);

	pThreadSystem->mQueueMutex.Init();
	pThreadSystem->mQueueCond.Init();
	pThreadSystem->mIdleCond.Init();
	
	pThreadSystem->mRun = true;
	pThreadSystem->mNumIdleLoaders = 0;

	for (unsigned i = 0; i < numLoaders; ++i)
	{
		pThreadSystem->mThreadDescs[i].pFunc = taskThreadFunc;
		pThreadSystem->mThreadDescs[i].pData = pThreadSystem;

#if defined(NX64)
		pThreadSystem->mThreadDescs[i].pThreadStack = aligned_alloc(THREAD_STACK_ALIGNMENT_NX, ALIGNED_THREAD_STACK_SIZE_NX);
		pThreadSystem->mThreadDescs[i].hThread = &pThreadSystem->mThreadType[i];
#endif

		pThreadSystem->mThread[i] = create_thread(&pThreadSystem->mThreadDescs[i]);
	}
	pThreadSystem->mNumLoaders = numLoaders;

	*ppThreadSystem = pThreadSystem;
}

void addThreadSystemTask(ThreadSystem* pThreadSystem, TaskFunc task, void* user, uintptr_t index)
{
	pThreadSystem->mQueueMutex.Acquire();
	pThreadSystem->mLoadQueue.emplace_back(ThreadedTask{ task, user, index, index+1 });
	pThreadSystem->mQueueMutex.Release();
	pThreadSystem->mQueueCond.WakeAll();
}

void addThreadSystemRangeTask(ThreadSystem* pThreadSystem, TaskFunc task, void* user, uintptr_t count)
{
	pThreadSystem->mQueueMutex.Acquire();
	pThreadSystem->mLoadQueue.emplace_back(ThreadedTask{ task, user, 0, count });
	pThreadSystem->mQueueMutex.Release();
	pThreadSystem->mQueueCond.WakeAll();
}

void addThreadSystemRangeTask(ThreadSystem* pThreadSystem, TaskFunc task, void* user, uintptr_t start, uintptr_t end)
{
	pThreadSystem->mQueueMutex.Acquire();
	pThreadSystem->mLoadQueue.emplace_back(ThreadedTask{ task, user, start, end });
	pThreadSystem->mQueueMutex.Release();
	pThreadSystem->mQueueCond.WakeAll();
}

void shutdownThreadSystem(ThreadSystem* pThreadSystem)
{
	pThreadSystem->mQueueMutex.Acquire();
	pThreadSystem->mRun = false;
	pThreadSystem->mQueueMutex.Release();
	pThreadSystem->mQueueCond.WakeAll();

	uint32_t numLoaders = pThreadSystem->mNumLoaders;
	for (uint32_t i = 0; i < numLoaders; ++i)
	{
		destroy_thread(pThreadSystem->mThread[i]);
	}

	pThreadSystem->mQueueCond.Destroy();
	pThreadSystem->mIdleCond.Destroy();
	pThreadSystem->mQueueMutex.Destroy();
	conf_delete(pThreadSystem);
}

bool isThreadSystemIdle(ThreadSystem* pThreadSystem)
{
	pThreadSystem->mQueueMutex.Acquire();
	bool idle = (pThreadSystem->mLoadQueue.empty() && pThreadSystem->mNumIdleLoaders == pThreadSystem->mNumLoaders) || !pThreadSystem->mRun;
	pThreadSystem->mQueueMutex.Release();
	return idle;
}

void waitThreadSystemIdle(ThreadSystem* pThreadSystem)
{
	pThreadSystem->mQueueMutex.Acquire();
	while ((!pThreadSystem->mLoadQueue.empty() || pThreadSystem->mNumIdleLoaders < pThreadSystem->mNumLoaders) && pThreadSystem->mRun)
		pThreadSystem->mIdleCond.Wait(pThreadSystem->mQueueMutex);
	pThreadSystem->mQueueMutex.Release();
}
