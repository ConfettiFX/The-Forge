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
#include "MTTaskBucket.h"

#ifdef Yield
	#undef Yield
#endif


namespace MT
{
	class TaskHandle;

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Fiber task status
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Task can be completed for several reasons.
	// For example task was done or someone call Yield from the Task body.
	namespace FiberTaskStatus
	{
		enum Type
		{
			UNKNOWN = 0,
			RUNNED = 1,
			FINISHED = 2,
			YIELDED = 3,
			AWAITING_CHILD = 4,
		};
	}

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Fiber context
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Context passed to fiber main function
	class FiberContext
	{
	private:

		void RunSubtasksAndYieldImpl(ArrayView<internal::TaskBucket>& buckets);

	public:

		FiberContext();

		template<class TTask>
		void RunSubtasksAndYield(TaskGroup taskGroup, const TTask* taskArray, size_t taskCount);

		template<class TTask>
		void RunAsync(TaskGroup taskGroup, const TTask* taskArray, size_t taskCount);

		//
		void RunAsync(TaskGroup taskGroup, const TaskHandle* taskHandleArray, uint32 taskHandleCount);
		void RunSubtasksAndYield(TaskGroup taskGroup, const TaskHandle* taskHandleArray, uint32 taskHandleCount);

		//
		void Yield();

		void Reset();

		void SetThreadContext(internal::ThreadContext * _threadContext);
		internal::ThreadContext* GetThreadContext();

		void SetStatus(FiberTaskStatus::Type _taskStatus);
		FiberTaskStatus::Type GetStatus() const;

	private:

		// Active thread context (null if fiber context is not executing now)
		internal::ThreadContext * threadContext;

		// Active task status
		FiberTaskStatus::Type taskStatus;

	public:

		// Active task attached to this fiber
		internal::TaskDesc currentTask;

		// Active task group
		TaskGroup currentGroup;

		// Requirements for stack
		StackRequirements::Type stackRequirements;

		// Number of children fibers
		Atomic32<int32> childrenFibersCount;

		// Parent fiber
		FiberContext* parentFiber;

		// System fiber
		Fiber fiber;

		//Fiber index in pool
		uint32 fiberIndex;

		// Prevent false sharing between threads
		uint8 cacheline[64];
	};


}
