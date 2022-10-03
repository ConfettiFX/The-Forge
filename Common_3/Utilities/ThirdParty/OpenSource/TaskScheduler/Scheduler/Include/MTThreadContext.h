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

#include "MTTools.h"
#include "MTPlatform.h"
#include "MTTaskQueue.h"
#include "MTConcurrentRingBuffer.h"
#include "MTGroupedTask.h"


#ifdef MT_INSTRUMENTED_BUILD

#define MT_SYSTEM_TASK_COLOR (MT::Color::Yellow)
#define MT_SYSTEM_TASK_NAME "SchedulerTask"
#define MT_SYSTEM_TASK_FIBER_NAME "IdleFiber"
#define MT_SYSTEM_FIBER_INDEX (int32)(-1)

#endif


namespace MT
{
	class FiberContext;
	class TaskScheduler;


#ifdef MT_INSTRUMENTED_BUILD
	namespace TaskExecuteState
	{
		enum Type
		{
			START = 0,
			STOP = 1,
			RESUME = 2,
			SUSPEND = 3,
		};
	}

#endif

	namespace internal
	{
		static const size_t TASK_BUFFER_CAPACITY = 4096;


		namespace ThreadState
		{
			const uint32 ALIVE = 0;
			const uint32 EXIT = 1;
		};

		////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		// Thread (Scheduler fiber) context
		////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		struct ThreadContext
		{
			FiberContext* lastActiveFiberContext;

			// pointer to task manager
			TaskScheduler* taskScheduler;

			// thread
			Thread thread;

			// thread Id
			ThreadId threadId;

			// scheduler fiber
			Fiber schedulerFiber;

			// task queue awaiting execution
			TaskQueue<internal::GroupedTask, TASK_BUFFER_CAPACITY> queue;

			// new task has arrived to queue event
			Event hasNewTasksEvent;

			// thread is alive or not
			Atomic32<int32> state;

			// Temporary buffer, fixed size = TASK_BUFFER_CAPACITY
			void* descBuffer;

			// Thread index
			uint32 workerIndex;

			// Thread random number generator
			LcgRandom random;

			bool isExternalDescBuffer;

			// prevent false cache sharing between threads
			uint8 cacheline[64];

			ThreadContext();
			ThreadContext(void* externalDescBuffer);
			~ThreadContext();

			void SetThreadIndex(uint32 threadIndex);

#ifdef MT_INSTRUMENTED_BUILD
			
			void NotifyThreadCreated(uint32 threadIndex);
			void NotifyThreadStarted(uint32 threadIndex);
			void NotifyThreadStoped(uint32 threadIndex);

			void NotifyTaskExecuteStateChanged(MT::Color::Type debugColor, const mt_char* debugID, TaskExecuteState::Type type, int32 fiberIndex);

			void NotifyThreadIdleStarted(uint32 threadIndex);
			void NotifyThreadIdleFinished(uint32 threadIndex);

			void NotifyWaitStarted();
			void NotifyWaitFinished();

			void NotifyTemporaryWorkerThreadJoin();
			void NotifyTemporaryWorkerThreadLeave();

#endif

			static size_t GetMemoryRequrementInBytesForDescBuffer();
		};

	}

}
