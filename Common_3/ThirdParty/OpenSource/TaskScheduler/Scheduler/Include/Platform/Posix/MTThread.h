// The MIT License (MIT)
//
// 	Copyright (c) 2015 Sergey Makeev, Vadim Slyusarev
//
// 	Permission is hereby granted, free of charge, to any person obtaining a copy
// 	of this software and associated documentation files (the "Software"), to deal
// 	in the Software without restriction, including without limitation the rights
// 	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// 	copies of the Software, and to permit persons to whom the Software is
// 	furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in
// 	all copies or substantial portions of the Software.
//
// 	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// 	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// 	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// 	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// 	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// 	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// 	THE SOFTWARE.

#pragma once

#ifndef __MT_THREAD__
#define __MT_THREAD__

#include "MTConfig.h"
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <limits.h>
#include <stdlib.h>
#include <sched.h>

#if MT_PLATFORM_OSX
#include <thread>
#endif

#include <sys/mman.h>

#ifndef MAP_ANONYMOUS
    #define MAP_ANONYMOUS MAP_ANON
#endif

#ifndef MAP_STACK
    #define MAP_STACK (0)
#endif

#include "Platform/Common/MTThread.h"
#include "MTAppInterop.h"

namespace MT
{
	//
	// Signals the calling thread to yield execution to another thread that is ready to run.
	//
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	inline void YieldThread()
	{
		int err = sched_yield();
		MT_USED_IN_ASSERT(err);
		MT_ASSERT(err == 0, "pthread_yield - error");
	}


	class ThreadId
	{
	protected:
		pthread_t id;
		Atomic32<uint32> isInitialized;

		void Assign(const ThreadId& other)
		{
			id = other.id;
			isInitialized.Store(other.isInitialized.Load());
		}

	public:

		ThreadId()
		{
			isInitialized.Store(0);
		}

		mt_forceinline ThreadId(const ThreadId& other)
		{
			Assign(other);
		}

		mt_forceinline ThreadId& operator=(const ThreadId& other)
		{
			Assign(other);
			return *this;
		}

		mt_forceinline static ThreadId Self()
		{
			ThreadId selfThread;
			selfThread.id = pthread_self();
			selfThread.isInitialized.Store(1);
			return selfThread;
		}

		mt_forceinline bool IsValid() const
		{
			return (isInitialized.Load() != 0);
		}

		mt_forceinline bool IsEqual(const ThreadId& other) const
		{
			if (isInitialized.Load() != other.isInitialized.Load())
			{
				return false;
			}
			if (pthread_equal(id, other.id) == false)
			{
				return false;
			}
			return true;
		}

		mt_forceinline uint64 AsUInt64() const
		{
			if (isInitialized.Load() == 0)
			{
				return (uint64)-1;
			}

			return (uint64)id;
		}
	};



	class Thread : public ThreadBase
	{
		pthread_t thread;
		pthread_attr_t threadAttr;

		Memory::StackDesc stackDesc;

		size_t stackSize;

		bool isStarted;

		static void* ThreadFuncInternal(void* pThread)
		{
			Thread* self = (Thread *)pThread;
			self->func(self->funcData);
			return nullptr;
		}

#if MT_PLATFORM_OSX
		//TODO: support OSX priority and bind to processors
#else
		static void GetAffinityMask(cpu_set_t & cpu_mask, uint32 cpuCore)
		{
			CPU_ZERO(&cpu_mask);

			if (cpuCore == MT_CPUCORE_ANY)
			{
				uint32 threadsCount = (uint32)GetNumberOfHardwareThreads();
				for(uint32 i = 0; i < threadsCount; i++)
				{
					CPU_SET(i, &cpu_mask);
				}
			} else
			{
				CPU_SET(cpuCore, &cpu_mask);
			}
		}


		static int GetPriority(ThreadPriority::Type priority)
		{
			int min_prio = sched_get_priority_min (SCHED_FIFO);
			int max_prio = sched_get_priority_max (SCHED_FIFO);
			int default_prio = (max_prio - min_prio) / 2;

			switch(priority)
			{
			case ThreadPriority::DEFAULT:
				return default_prio;
			case ThreadPriority::HIGH:
				return max_prio;
			case ThreadPriority::LOW:
				return min_prio;
			default:
				MT_REPORT_ASSERT("Invalid thread priority");
			}

			return default_prio;
		}
#endif


	public:

		Thread()
			: stackSize(0)
			, isStarted(false)
		{
		}

		void* GetStackBottom()
		{
			return stackDesc.stackBottom;
		}

		size_t GetStackSize()
		{
			return stackSize;
		}


		void Start(size_t _stackSize, TThreadEntryPoint entryPoint, void* userData, uint32 cpuCore = MT_CPUCORE_ANY, ThreadPriority::Type priority = ThreadPriority::DEFAULT)
		{
			MT_ASSERT(!isStarted, "Thread already stared");

			MT_ASSERT(func == nullptr, "Thread already started");

			func = entryPoint;
			funcData = userData;

			stackDesc = Memory::AllocStack(_stackSize);
			stackSize = stackDesc.GetStackSize();

			MT_ASSERT(stackSize >= PTHREAD_STACK_MIN, "Thread stack to small");

			int err = pthread_attr_init(&threadAttr);
			MT_USED_IN_ASSERT(err);
			MT_ASSERT(err == 0, "pthread_attr_init - error");

			err = pthread_attr_setstack(&threadAttr, stackDesc.stackBottom, stackSize);
			MT_USED_IN_ASSERT(err);
			MT_ASSERT(err == 0, "pthread_attr_setstack - error");

			err = pthread_attr_setdetachstate(&threadAttr, PTHREAD_CREATE_JOINABLE);
			MT_USED_IN_ASSERT(err);
			MT_ASSERT(err == 0, "pthread_attr_setdetachstate - error");

#if MT_PLATFORM_OSX
			MT_UNUSED(cpuCore);
			MT_UNUSED(priority);

			//TODO: support OSX priority and bind to processors
#else
			err = pthread_attr_setinheritsched(&threadAttr, PTHREAD_EXPLICIT_SCHED);
			MT_USED_IN_ASSERT(err);
			MT_ASSERT(err == 0, "pthread_attr_setinheritsched - error");

			cpu_set_t cpu_mask;
			GetAffinityMask(cpu_mask, cpuCore);
			err = pthread_attr_setaffinity_np(&threadAttr, sizeof(cpu_mask), &cpu_mask);
			MT_USED_IN_ASSERT(err);
			MT_ASSERT(err == 0, "pthread_attr_setaffinity_np - error");

			struct sched_param params;
			params.sched_priority = GetPriority(priority);
			err = pthread_attr_setschedparam(&threadAttr, &params);
			MT_USED_IN_ASSERT(err);
			MT_ASSERT(err == 0, "pthread_attr_setschedparam - error");
#endif

			isStarted = true;

			err = pthread_create(&thread, &threadAttr, ThreadFuncInternal, this);
			MT_USED_IN_ASSERT(err);
			MT_ASSERT(err == 0, "pthread_create - error");
		}

		void Join()
		{
			MT_ASSERT(isStarted, "Thread is not started");

			if (func == nullptr)
			{
				return;
			}

			void *threadStatus = nullptr;
			int err = pthread_join(thread, &threadStatus);
			MT_USED_IN_ASSERT(err);
			MT_ASSERT(err == 0, "pthread_join - error");

			err = pthread_attr_destroy(&threadAttr);
			MT_USED_IN_ASSERT(err);
			MT_ASSERT(err == 0, "pthread_attr_destroy - error");

			func = nullptr;
			funcData = nullptr;

			if (stackDesc.stackMemory != nullptr)
			{
				Memory::FreeStack(stackDesc);
			}

			stackSize = 0;
			isStarted = false;
		}


		static int GetNumberOfHardwareThreads()
		{
#if MT_PLATFORM_OSX
            return std::thread::hardware_concurrency();
#else
			long numberOfProcessors = sysconf( _SC_NPROCESSORS_ONLN );
			return (int)numberOfProcessors;
#endif
		}

#ifdef MT_INSTRUMENTED_BUILD
		static void SetThreadName(const char* threadName)
		{
			pthread_t callThread = pthread_self();
			pthread_setname_np(callThread, threadName);
		}
#endif

		static void SetThreadSchedulingPolicy(uint32 cpuCore, ThreadPriority::Type priority = ThreadPriority::DEFAULT)
		{
#if MT_PLATFORM_OSX
			MT_UNUSED(cpuCore);
			MT_UNUSED(priority);

			//TODO: support OSX priority and bind to processors
#else
			pthread_t callThread = pthread_self();

			int sched_priority = GetPriority(priority);
			int err = pthread_setschedprio(callThread, sched_priority);
			MT_USED_IN_ASSERT(err);
			MT_ASSERT(err == 0, "pthread_setschedprio - error");

			cpu_set_t cpu_mask;
			GetAffinityMask(cpu_mask, cpuCore);
			err = pthread_setaffinity_np(callThread, sizeof(cpu_mask), &cpu_mask);
			MT_USED_IN_ASSERT(err);
			MT_ASSERT(err == 0, "pthread_setaffinity_np - error");
#endif
		}


		static void Sleep(uint32 milliseconds)
		{
			struct timespec req;
			int sec = (int)(milliseconds / 1000);
			milliseconds = milliseconds - (sec*1000);
			req.tv_sec = sec;
			req.tv_nsec = milliseconds * 1000000L;
			while (nanosleep(&req,&req) == -1 )
			{
				continue;
			}
		}

	};


}


#endif
