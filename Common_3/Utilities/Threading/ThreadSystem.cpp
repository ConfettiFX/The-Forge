/*
 * Copyright (c) 2017-2022 The Forge Interactive Inc.
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

#include "../../Utilities/Interfaces/IThread.h"
#include "../../Utilities/Interfaces/ILog.h"

#include "../../Utilities/Math/MathTypes.h"

#include "ThreadSystem.h"
#include "../../Utilities/Interfaces/IMemory.h"

struct ThreadedTask
{
	TaskFunc  mTask;
	void*     mUser;
	uintptr_t mStart;
	uintptr_t mEnd;
};

struct ThreadSystem
{
	ThreadHandle      mThread[MAX_LOAD_THREADS];
	ThreadedTask      mLoadTask[MAX_SYSTEM_TASKS];
	uint32_t          mBegin, mEnd;
	ConditionVariable mQueueCond;
	Mutex             mQueueMutex;
	ConditionVariable mIdleCond;
	uint32_t          mNumLoaders;
	uint32_t          mNumIdleLoaders;
	volatile bool     mRun;
};

bool assistThreadSystemTasks(ThreadSystem* pThreadSystem, uint32_t* pIds, size_t count)
{
	acquireMutex(&pThreadSystem->mQueueMutex);
	if (pThreadSystem->mBegin == pThreadSystem->mEnd)
	{
		releaseMutex(&pThreadSystem->mQueueMutex);
		return false;
	}

	uint32_t taskSize = pThreadSystem->mBegin >= pThreadSystem->mEnd ? pThreadSystem->mBegin - pThreadSystem->mEnd
																	 : MAX_SYSTEM_TASKS - (pThreadSystem->mEnd - pThreadSystem->mBegin);
	ThreadedTask resourceTask;
	bool         found = false;

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

	releaseMutex(&pThreadSystem->mQueueMutex);

	if (!found)
	{
		return false;
	}

	resourceTask.mTask(resourceTask.mUser, resourceTask.mStart);
	return true;
}

bool assistThreadSystem(ThreadSystem* pThreadSystem)
{
	acquireMutex(&pThreadSystem->mQueueMutex);
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
		releaseMutex(&pThreadSystem->mQueueMutex);
		resourceTask.mTask(resourceTask.mUser, resourceTask.mStart);

		return true;
	}
	else
	{
		releaseMutex(&pThreadSystem->mQueueMutex);
		return false;
	}
}

static void taskThreadFunc(void* pThreadData)
{
	ThreadSystem* pThreadSystem = (ThreadSystem*)pThreadData;
	while (pThreadSystem->mRun)
	{
		acquireMutex(&pThreadSystem->mQueueMutex);
		++pThreadSystem->mNumIdleLoaders;
		while (pThreadSystem->mRun && pThreadSystem->mBegin == pThreadSystem->mEnd)
		{
			wakeAllConditionVariable(&pThreadSystem->mIdleCond);
			waitConditionVariable(&pThreadSystem->mQueueCond, &pThreadSystem->mQueueMutex, TIMEOUT_INFINITE);
		}
		--pThreadSystem->mNumIdleLoaders;
		if (pThreadSystem->mBegin != pThreadSystem->mEnd)
		{
			ThreadedTask resourceTask = pThreadSystem->mLoadTask[pThreadSystem->mEnd];    // pThreadSystem->mLoadQueue.front();
			if (resourceTask.mStart + 1 == resourceTask.mEnd)
			{
				++pThreadSystem->mEnd;
				pThreadSystem->mEnd = pThreadSystem->mEnd % MAX_SYSTEM_TASKS;
			}
			else
			{
				++pThreadSystem->mLoadTask[pThreadSystem->mEnd].mStart;
			}
			releaseMutex(&pThreadSystem->mQueueMutex);
			resourceTask.mTask(resourceTask.mUser, resourceTask.mStart);
		}
		else
		{
			releaseMutex(&pThreadSystem->mQueueMutex);
		}
	}
	acquireMutex(&pThreadSystem->mQueueMutex);
	++pThreadSystem->mNumIdleLoaders;
	wakeAllConditionVariable(&pThreadSystem->mIdleCond);
	releaseMutex(&pThreadSystem->mQueueMutex);
}

void initThreadSystem(
	ThreadSystem** ppThreadSystem, uint32_t numRequestedThreads, uint32_t* affinityMasks, const char* threadName)
{
	ThreadSystem* pThreadSystem = tf_new(ThreadSystem);

	uint32_t numThreads = max<uint32_t>(getNumCPUCores() - 1, 1);
	uint32_t numLoaders = min<uint32_t>(numThreads, min<uint32_t>(numRequestedThreads, MAX_LOAD_THREADS));

	initMutex(&pThreadSystem->mQueueMutex);
	initConditionVariable(&pThreadSystem->mQueueCond);
	initConditionVariable(&pThreadSystem->mIdleCond);

	pThreadSystem->mRun = true;
	pThreadSystem->mNumIdleLoaders = 0;
	pThreadSystem->mBegin = 0;
	pThreadSystem->mEnd = 0;

	if (!threadName || *threadName == 0)
		threadName = "TaskThread";

	ThreadDesc threadDesc = {};
	threadDesc.pFunc = taskThreadFunc;
	threadDesc.pData = pThreadSystem;
	
	for (unsigned i = 0; i < numLoaders; ++i)
	{
		if (affinityMasks)
		{
			threadDesc.mHasAffinityMask = true;
			threadDesc.mAffinityMask = affinityMasks[i];
		}

		snprintf(threadDesc.mThreadName, sizeof(threadDesc.mThreadName), "%s%u", threadName, i);
		initThread(&threadDesc, &pThreadSystem->mThread[i]);
	}
	pThreadSystem->mNumLoaders = numLoaders;

	*ppThreadSystem = pThreadSystem;
}

void addThreadSystemTask(ThreadSystem* pThreadSystem, TaskFunc task, void* user, uintptr_t index)
{
	acquireMutex(&pThreadSystem->mQueueMutex);
	pThreadSystem->mLoadTask[pThreadSystem->mBegin++] = ThreadedTask{ task, user, index, index + 1 };
	pThreadSystem->mBegin = pThreadSystem->mBegin % MAX_SYSTEM_TASKS;
	LOGF_IF(
		LogLevel::eERROR, pThreadSystem->mBegin == pThreadSystem->mEnd,
		"Maximum amount of thread task reached: mBegin (%d), mEnd(%d), Max(%d)", pThreadSystem->mBegin, pThreadSystem->mEnd,
		MAX_SYSTEM_TASKS);
	ASSERT(pThreadSystem->mBegin != pThreadSystem->mEnd);
	releaseMutex(&pThreadSystem->mQueueMutex);
	wakeAllConditionVariable(&pThreadSystem->mQueueCond);
}

uint32_t getThreadSystemThreadCount(ThreadSystem* pThreadSystem) { return pThreadSystem->mNumLoaders; }

void addThreadSystemRangeTask(ThreadSystem* pThreadSystem, TaskFunc task, void* user, uintptr_t count)
{
	acquireMutex(&pThreadSystem->mQueueMutex);
	pThreadSystem->mLoadTask[pThreadSystem->mBegin++] = ThreadedTask{ task, user, 0, count };
	pThreadSystem->mBegin = pThreadSystem->mBegin % MAX_SYSTEM_TASKS;
	LOGF_IF(
		LogLevel::eERROR, pThreadSystem->mBegin == pThreadSystem->mEnd,
		"Maximum amount of thread task reached: mBegin (%d), mEnd(%d), Max(%d)", pThreadSystem->mBegin, pThreadSystem->mEnd,
		MAX_SYSTEM_TASKS);
	ASSERT(pThreadSystem->mBegin != pThreadSystem->mEnd);
	releaseMutex(&pThreadSystem->mQueueMutex);
	wakeAllConditionVariable(&pThreadSystem->mQueueCond);
}

void addThreadSystemRangeTask(ThreadSystem* pThreadSystem, TaskFunc task, void* user, uintptr_t start, uintptr_t end)
{
	acquireMutex(&pThreadSystem->mQueueMutex);
	pThreadSystem->mLoadTask[pThreadSystem->mBegin++] = ThreadedTask{ task, user, start, end };
	pThreadSystem->mBegin = pThreadSystem->mBegin % MAX_SYSTEM_TASKS;
	LOGF_IF(
		LogLevel::eERROR, pThreadSystem->mBegin == pThreadSystem->mEnd,
		"Maximum amount of thread task reached: mBegin (%d), mEnd(%d), Max(%d)", pThreadSystem->mBegin, pThreadSystem->mEnd,
		MAX_SYSTEM_TASKS);
	ASSERT(pThreadSystem->mBegin != pThreadSystem->mEnd);
	releaseMutex(&pThreadSystem->mQueueMutex);
	wakeAllConditionVariable(&pThreadSystem->mQueueCond);
}

void exitThreadSystem(ThreadSystem* pThreadSystem)
{
	acquireMutex(&pThreadSystem->mQueueMutex);
	pThreadSystem->mRun = false;
	pThreadSystem->mBegin = 0;
	pThreadSystem->mEnd = 0;
	releaseMutex(&pThreadSystem->mQueueMutex);
	wakeAllConditionVariable(&pThreadSystem->mQueueCond);

	uint32_t numLoaders = pThreadSystem->mNumLoaders;
	for (uint32_t i = 0; i < numLoaders; ++i)
	{
		joinThread(pThreadSystem->mThread[i]);
	}

	destroyConditionVariable(&pThreadSystem->mQueueCond);
	destroyConditionVariable(&pThreadSystem->mIdleCond);
	destroyMutex(&pThreadSystem->mQueueMutex);
	tf_delete(pThreadSystem);
}

bool isThreadSystemIdle(ThreadSystem* pThreadSystem)
{
	acquireMutex(&pThreadSystem->mQueueMutex);
	bool idle = (pThreadSystem->mBegin == pThreadSystem->mEnd && pThreadSystem->mNumIdleLoaders == pThreadSystem->mNumLoaders) ||
				!pThreadSystem->mRun;
	releaseMutex(&pThreadSystem->mQueueMutex);
	return idle;
}

void waitThreadSystemIdle(ThreadSystem* pThreadSystem)
{
	acquireMutex(&pThreadSystem->mQueueMutex);
	while ((pThreadSystem->mBegin != pThreadSystem->mEnd || pThreadSystem->mNumIdleLoaders < pThreadSystem->mNumLoaders) &&
		   pThreadSystem->mRun)
		waitConditionVariable(&pThreadSystem->mIdleCond, &pThreadSystem->mQueueMutex, TIMEOUT_INFINITE);
	releaseMutex(&pThreadSystem->mQueueMutex);
}
