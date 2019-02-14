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

#include "../../../../Common_3/OS/Interfaces/ILogManager.h"
#include "../../../../Common_3/OS/Interfaces/IThread.h"
#include "../../../../Common_3/OS/Interfaces/IMemoryManager.h"

#include "IOResourceLoader.h"

struct IOResourceTask
{
	TaskFunc mTask;
	void*    mUser;
	size_t   mArg;
};

enum
{
	MAX_LOAD_THREADS = 3
};

struct IOResourceLoader
{
	WorkItem                        mWorkItems[MAX_LOAD_THREADS];
	ThreadHandle                    mThread[MAX_LOAD_THREADS];
	tinystl::vector<IOResourceTask> mLoadQueue;
	ConditionVariable               mQueueCond;
	Mutex                           mQueueMutex;
	uint32_t                        mNumLoaders;
	volatile bool                   mRun;
};

static void loadThreadFunc(void* pThreadData)
{
	IOResourceLoader* pLoader = (IOResourceLoader*)pThreadData;
	while (pLoader->mRun)
	{
		pLoader->mQueueMutex.Acquire();
		while (pLoader->mRun && pLoader->mLoadQueue.empty())
		{
			pLoader->mQueueCond.Wait(pLoader->mQueueMutex);
		}
		if (!pLoader->mLoadQueue.empty())
		{
			IOResourceTask resourceTask = pLoader->mLoadQueue.front();
			pLoader->mLoadQueue.erase(pLoader->mLoadQueue.begin());
			pLoader->mQueueMutex.Release();
			resourceTask.mTask(resourceTask.mUser, resourceTask.mArg);
		}
		else
		{
			pLoader->mQueueMutex.Release();
		}
	}
}

void initializeIOResourceLoader(IOResourceLoader** ppLoader)
{
	IOResourceLoader* pLoader = conf_new<IOResourceLoader>();

	uint32_t numThreads = Thread::GetNumCPUCores() - 1;
	uint32_t numLoaders = MAX_LOAD_THREADS < numThreads ? MAX_LOAD_THREADS : numThreads;

	pLoader->mRun = true;

	for (unsigned i = 0; i < numLoaders; ++i)
	{
		pLoader->mWorkItems[i].pFunc = loadThreadFunc;
		pLoader->mWorkItems[i].pData = pLoader;
		pLoader->mWorkItems[i].mPriority = 0;
		pLoader->mWorkItems[i].mCompleted = false;

		pLoader->mThread[i] = create_thread(&pLoader->mWorkItems[i]);
	}
	pLoader->mNumLoaders = numLoaders;

	*ppLoader = pLoader;
}

void addIOResourceTask(IOResourceLoader* pLoader, TaskFunc task, void* user, size_t arg)
{
	pLoader->mQueueMutex.Acquire();
	pLoader->mLoadQueue.emplace_back(IOResourceTask{ task, user, arg });
	pLoader->mQueueMutex.Release();
	pLoader->mQueueCond.Set();
}

void shutdownIOResourceLoader(IOResourceLoader* pLoader)
{
	pLoader->mQueueMutex.Acquire();
	pLoader->mRun = false;
	pLoader->mQueueMutex.Release();
	pLoader->mQueueCond.SetAll();

	uint32_t numLoaders = pLoader->mNumLoaders;
	for (uint32_t i = 0; i < numLoaders; ++i)
	{
		destroy_thread(pLoader->mThread[i]);
	}

	conf_delete(pLoader);
}
