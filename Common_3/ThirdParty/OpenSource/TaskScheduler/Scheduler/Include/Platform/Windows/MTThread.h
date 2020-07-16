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

#include "Platform/Common/MTThread.h"

namespace MT
{
	//
	// Signals the calling thread to yield execution to another thread that is ready to run.
	//
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	inline void YieldThread()
	{
		::SwitchToThread ();
	}


	class ThreadId
	{
	protected:
		MW_DWORD id;
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
			selfThread.id = ::GetCurrentThreadId();
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
			if (id != other.id)
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
		MW_HANDLE thread;

		static MW_DWORD __stdcall ThreadFuncInternal(void *pThread)
		{
			Thread* self = (Thread*)pThread;
			self->func(self->funcData);
			return 0;
		}



		static int GetPriority(ThreadPriority::Type priority)
		{
			switch(priority)
			{
			case ThreadPriority::DEFAULT:
				return MW_THREAD_PRIORITY_HIGHEST;
			case ThreadPriority::HIGH:
				return MW_THREAD_PRIORITY_NORMAL;
			case ThreadPriority::LOW:
				return MW_THREAD_PRIORITY_LOWEST;
			default:
				MT_REPORT_ASSERT("Invalid thread priority");
			}

			return MW_THREAD_PRIORITY_NORMAL;
		}


	public:

		Thread()
			: thread(nullptr)
		{
		}

		~Thread()
		{
			MT_ASSERT(thread == nullptr, "Thread is not stopped!");
		}

		void Start(size_t stackSize, TThreadEntryPoint entryPoint, void *userData, uint32 cpuCore = MT_CPUCORE_ANY, ThreadPriority::Type priority = ThreadPriority::DEFAULT)
		{
			MT_ASSERT(thread == nullptr, "Thread already started");

			func = entryPoint;
			funcData = userData;
			thread = ::CreateThread( nullptr, stackSize, ThreadFuncInternal, this, MW_CREATE_SUSPENDED, nullptr );
			MT_ASSERT(thread != nullptr, "Can't create thread");

			if (cpuCore == MT_CPUCORE_ANY)
			{
				cpuCore = MW_MAXIMUM_PROCESSORS;
			}
			MT_VERIFY((cpuCore >= 0 && cpuCore < (uint32)GetNumberOfHardwareThreads()) || cpuCore == MW_MAXIMUM_PROCESSORS, "Invalid cpu core specified", cpuCore=MW_MAXIMUM_PROCESSORS);

			MW_DWORD res;
#if !defined(XBOX)
			res = ::SetThreadIdealProcessor(thread, cpuCore);
			MT_USED_IN_ASSERT(res);
			MT_ASSERT(res != (MW_DWORD)-1, "SetThreadIdealProcessor failed!");
#endif
			int sched_priority = GetPriority(priority);

			MW_BOOL result = ::SetThreadPriority(thread, sched_priority);
			MT_USED_IN_ASSERT(result);
			MT_ASSERT(result != 0, "SetThreadPriority failed!");

			res = ::ResumeThread(thread);
			MT_USED_IN_ASSERT(res);
			MT_ASSERT(res != (MW_DWORD)-1, "ResumeThread failed!");
		}

		void Join()
		{
			if (thread == nullptr)
			{
				return;
			}

			::WaitForSingleObject(thread, MW_INFINITE);
			MW_BOOL res = CloseHandle(thread);
			MT_USED_IN_ASSERT(res);
			MT_ASSERT(res != 0, "Can't close thread handle");
			thread = nullptr;
		}

#ifdef MT_INSTRUMENTED_BUILD
		static void SetThreadName(const char* threadName)
		{
			const int MW_EXCEPTION_EXECUTE_HANDLER = 1;
			const MW_DWORD MW_MSVC_EXCEPTION = 0x406D1388;

#pragma pack(push,8)
			typedef struct tagTHREADNAME_INFO
			{
				MW_DWORD dwType; // Must be 0x1000.
				const char* szName; // Pointer to name (in user addr space).
				MW_DWORD dwThreadID; // Thread ID (-1=caller thread).
				MW_DWORD dwFlags; // Reserved for future use, must be zero.
			} THREADNAME_INFO;
#pragma pack(pop)

			THREADNAME_INFO info;
			info.dwType = 0x1000;
			info.szName = threadName;
			info.dwThreadID = ::GetCurrentThreadId();
			info.dwFlags = 0;

			__try
			{
				RaiseException(MW_MSVC_EXCEPTION, 0, sizeof(info) / sizeof(void*), (MW_ULONG_PTR*)&info);
			}
			__except (MW_EXCEPTION_EXECUTE_HANDLER)
			{
			}
		}
#endif

		static void SetThreadSchedulingPolicy(uint32 cpuCore, ThreadPriority::Type priority = ThreadPriority::DEFAULT)
		{
			if (cpuCore == MT_CPUCORE_ANY)
			{
				cpuCore = MW_MAXIMUM_PROCESSORS;
			}
			MT_VERIFY((cpuCore >= 0 && cpuCore < (uint32)GetNumberOfHardwareThreads()) || cpuCore == MW_MAXIMUM_PROCESSORS, "Invalid cpu core specified", cpuCore=MW_MAXIMUM_PROCESSORS);
			MW_DWORD res = ::SetThreadIdealProcessor( ::GetCurrentThread(), cpuCore);
			MT_USED_IN_ASSERT(res);
			MT_ASSERT(res != (MW_DWORD)-1, "SetThreadIdealProcessor failed!");

			int sched_priority = GetPriority(priority);

			MW_BOOL result = ::SetThreadPriority( ::GetCurrentThread(), sched_priority );
			MT_USED_IN_ASSERT(result);
			MT_ASSERT(result != 0, "SetThreadPriority failed!");
		}

		static int GetNumberOfHardwareThreads()
		{
			MW_SYSTEM_INFO sysinfo;
			::GetSystemInfo( &sysinfo );
			return sysinfo.dwNumberOfProcessors;
		}

		static void Sleep(uint32 milliseconds)
		{
		  ::Sleep(milliseconds);
		}
	};


}


#endif
