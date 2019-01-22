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

#include <algorithm>

#include "../Interfaces/IThread.h"
#include "../Interfaces/ILogManager.h"
#include "../Interfaces/IMemoryManager.h"

MutexLock::MutexLock(Mutex& rhs): mMutex(rhs) { rhs.Acquire(); }

MutexLock::~MutexLock() { mMutex.Release(); }

Thread::Thread(ThreadPool* pThreadSystem)
{
	pItem = (WorkItem*)conf_calloc(1, sizeof(WorkItem));
	pItem->pData = pThreadSystem;
	pItem->pFunc = ThreadPool::ProcessItems;
	pItem->mCompleted = false;

	pHandle = create_thread(pItem);
}

Thread::~Thread()
{
	if (pHandle != 0)
	{
		destroy_thread(pHandle);
		conf_free(pItem);
	}
}

ThreadPool::ThreadPool(): mShutDown(false), mPausing(false), mPaused(false), mCompleting(false) { Thread::SetMainThread(); }

ThreadPool::~ThreadPool()
{
	// Stop the worker threads. First make sure they are not waiting for work items
	mShutDown = true;
	Resume();

	for (unsigned i = 0; i < mThreads.size(); ++i)
	{
		mThreads[i]->~Thread();
		conf_free(mThreads[i]);
	}
}

void ThreadPool::CreateThreads(unsigned numThreads)
{
	// Only allow creation of threads once during lifetime of a threadpool instance
	if (!mThreads.empty())
		return;

	// Start threads in paused mode
	Pause();

	for (unsigned i = 0; i < numThreads; ++i)
	{
		Thread* thread(conf_placement_new<Thread>(conf_calloc(1, sizeof(Thread)), this));
		mThreads.emplace_back(thread);
	}
}

void ThreadPool::AddWorkItem(WorkItem* item)
{
	// Check for duplicate / invalid items.
	ASSERT(item && "Null work item submitted to thread pool");
	ASSERT(mWorkItems.find(item) == mWorkItems.end());

	// Push to the main thread list to keep item alive
	// Clear completed flag in case item is reused
	mWorkItems.push_back(item);
	item->mCompleted = false;

	if (mThreads.size() && !mPaused)
		mQueueMutex.Acquire();

	// Find position for new item
	if (mWorkQueue.empty())
		mWorkQueue.push_back(item);
	else
	{
		for (WorkItem** i = mWorkQueue.begin(); i != mWorkQueue.end(); ++i)
		{
			if ((*i)->mPriority <= item->mPriority)
			{
				mWorkQueue.insert(i, item);
				break;
			}
		}
	}

	if (mThreads.size())
	{
		mQueueMutex.Release();
		mPaused = false;
	}
}

bool ThreadPool::RemoveWorkItem(WorkItem*& item)
{
	if (!item)
		return false;

	MutexLock lock(mQueueMutex);

	tinystl::vector<WorkItem*>::iterator i = mWorkQueue.find(item);
	if (i != mWorkQueue.end())
	{
		WorkItem** j = mWorkItems.find(item);
		if (j != mWorkItems.end())
		{
			mWorkQueue.erase(i);
			mWorkItems.erase(j);
			return true;
		}
	}

	return false;
}

unsigned ThreadPool::RemoveWorkItems(const tinystl::vector<WorkItem*>& items)
{
	MutexLock lock(mQueueMutex);
	unsigned  removed = 0;

	for (WorkItem* const& i : items)
	{
		WorkItem** j = mWorkQueue.find(i);
		if (j != mWorkQueue.end())
		{
			WorkItem** k = mWorkItems.find(i);
			if (k != mWorkItems.end())
			{
				mWorkQueue.erase(j);
				mWorkItems.erase(k);
				++removed;
			}
		}
	}

	return removed;
}

void ThreadPool::Pause()
{
	if (!mPaused)
	{
		mPausing = true;

		mQueueMutex.Acquire();
		mPaused = true;

		mPausing = false;
	}
}

void ThreadPool::Resume()
{
	if (mPaused)
	{
		mQueueMutex.Release();
		mPaused = false;
	}
}

void ThreadPool::Complete(unsigned priority)
{
	mCompleting = true;

	if (mThreads.size())
	{
		Resume();

		while (!mWorkQueue.empty())
		{
			mQueueMutex.Acquire();
			if (!mWorkQueue.empty() && mWorkQueue.front()->mPriority >= priority)
			{
				WorkItem* item = mWorkQueue.front();
				mWorkQueue.erase(mWorkQueue.begin());
				mQueueMutex.Release();
				item->pFunc(item->pData);
				item->mCompleted = true;
			}
			else
			{
				mQueueMutex.Release();
				break;
			}
		}

		// Wait for threads to complete work
		while (!IsCompleted(priority)) {}

		// Pause worker threads
		if (mWorkQueue.empty())
			Pause();
	}
	else
	{
		// Single threaded systems
		while (!mWorkQueue.empty() && mWorkQueue.front()->mPriority >= priority)
		{
			WorkItem* item = mWorkQueue.front();
			mWorkQueue.erase(mWorkQueue.begin());
			item->pFunc(item->pData);
			item->mCompleted = true;
		}
	}

	Cleanup(priority);
	mCompleting = false;
}

bool ThreadPool::IsCompleted(unsigned priority) const
{
	for (WorkItem* const* i = mWorkItems.begin(); i != mWorkItems.end(); ++i)
	{
		if ((*i)->mPriority >= priority && !(*i)->mCompleted)
			return false;
	}

	return true;
}

void ThreadPool::ProcessItems(void* pData)
{
	bool wasActive = false;

	ThreadPool* pSystem = (ThreadPool*)pData;

	for (;;)
	{
		if (pSystem->mShutDown)
			return;

		if (pSystem->mPausing && !wasActive)
			Thread::Sleep(0);
		else
		{
			pSystem->mQueueMutex.Acquire();
			if (!pSystem->mWorkQueue.empty())
			{
				wasActive = true;

				WorkItem* item = pSystem->mWorkQueue.front();
				pSystem->mWorkQueue.erase(pSystem->mWorkQueue.begin());
				pSystem->mQueueMutex.Release();
				item->pFunc(item->pData);
				item->mCompleted = true;
			}
			else
			{
				wasActive = false;

				pSystem->mQueueMutex.Release();
				Thread::Sleep(0);
			}
		}
	}
}

void ThreadPool::Cleanup(unsigned priority)
{
	for (WorkItem** i = mWorkItems.begin(); i != mWorkItems.end();)
	{
		if ((*i)->mCompleted && (*i)->mPriority >= priority)
		{
			i = mWorkItems.erase(i);
		}
		else
			++i;
	}
}
