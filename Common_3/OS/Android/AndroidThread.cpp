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
#ifdef __ANDROID__

#include <assert.h>
#include "../Interfaces/IThread.h"
#include "../Interfaces/IOperatingSystem.h"
#include "../Interfaces/ILog.h"

#include <pthread.h>
#include <unistd.h>
#include "../Interfaces/IMemory.h"

Mutex::Mutex()
{
	pHandle = PTHREAD_MUTEX_INITIALIZER;
	pthread_mutex_init(&pHandle, NULL);
}

Mutex::~Mutex() { pthread_mutex_destroy(&pHandle); }

void Mutex::Acquire() { pthread_mutex_lock(&pHandle); }

void Mutex::Release() { pthread_mutex_unlock(&pHandle); }

void* ThreadFunctionStatic(void* data)
{
	ThreadDesc* pItem = static_cast<ThreadDesc*>(data);
	pItem->pFunc(pItem->pData);
	return 0;
}

ConditionVariable::ConditionVariable()
{
	pHandle = PTHREAD_COND_INITIALIZER;
	int res = pthread_cond_init(&pHandle, NULL);
	assert(res == 0);
}

ConditionVariable::~ConditionVariable() { pthread_cond_destroy(&pHandle); }

void ConditionVariable::Wait(const Mutex& mutex, uint32_t ms)
{
    pthread_mutex_t* mutexHandle = (pthread_mutex_t*)&mutex.pHandle;
    if (ms == TIMEOUT_INFINITE)
    {
        pthread_cond_wait(&pHandle, mutexHandle);
    }
    else
    {
        timespec ts;
        ts.tv_sec = ms / 1000;
        ts.tv_nsec = (ms % 1000) * 1000;
        pthread_cond_timedwait(&pHandle, mutexHandle, &ts);
    }
}

void ConditionVariable::WakeOne() { pthread_cond_signal(&pHandle); }

void ConditionVariable::WakeAll() { pthread_cond_broadcast(&pHandle); }

ThreadID Thread::mainThreadID;

void Thread::SetMainThread() { mainThreadID = GetCurrentThreadID(); }

ThreadID Thread::GetCurrentThreadID() { return pthread_self(); }

void Thread::GetCurrentThreadName(char * buffer, int buffer_size) {	pthread_getname_np(pthread_self(), buffer, buffer_size); }

void Thread::SetCurrentThreadName(const char * name) { pthread_setname_np(pthread_self(), name); }

bool Thread::IsMainThread() { return GetCurrentThreadID() == mainThreadID; }

ThreadHandle create_thread(ThreadDesc* pData)
{
    pthread_t handle;
	int res = pthread_create(&handle, NULL, ThreadFunctionStatic, pData);
	assert(res == 0);
    return (ThreadHandle)handle;
}

void destroy_thread(ThreadHandle handle)
{
    ASSERT(handle!= (long)NULL);
    // Wait for thread to join, need to make sure thread
    //stops running otherwise it is not properly destroyed
    pthread_join(handle, NULL);
	handle = NULL;
}

void join_thread(ThreadHandle handle) { pthread_join(handle, NULL); }

void Thread::Sleep(unsigned mSec) { usleep(mSec * 1000); }

// threading class (Static functions)
unsigned int Thread::GetNumCPUCores(void)
{
    unsigned int ncpu;
    ncpu = sysconf(_SC_NPROCESSORS_ONLN);
    return ncpu;
}
#endif    //if __ANDROID__
