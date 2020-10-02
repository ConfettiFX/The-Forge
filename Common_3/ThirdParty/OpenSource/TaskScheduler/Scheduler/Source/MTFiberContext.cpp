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

#include "../Include/MTScheduler.h"

namespace MT
{
	FiberContext::FiberContext()
		: threadContext(nullptr)
		, taskStatus(FiberTaskStatus::UNKNOWN)
		, stackRequirements(StackRequirements::INVALID)
		, childrenFibersCount(0)
		, parentFiber(nullptr)
	{
		
	}

	void FiberContext::SetStatus(FiberTaskStatus::Type _taskStatus)
	{
		MT_ASSERT(threadContext, "Sanity check failed");
		MT_ASSERT(threadContext->threadId.IsEqual(ThreadId::Self()), "You can change task status only from owner thread");
		taskStatus = _taskStatus;
	}

	FiberTaskStatus::Type FiberContext::GetStatus() const
	{
		return taskStatus;
	}

	void FiberContext::SetThreadContext(internal::ThreadContext * _threadContext)
	{
		if (_threadContext)
		{
			_threadContext->lastActiveFiberContext = this;
		} 

		threadContext = _threadContext;
	}

	internal::ThreadContext* FiberContext::GetThreadContext()
	{
		return threadContext;
	}

	void FiberContext::Reset()
	{
		MT_ASSERT(childrenFibersCount.Load() == 0, "Can't release fiber with active children fibers");
		currentTask = internal::TaskDesc();
		parentFiber = nullptr;
		threadContext = nullptr;
		stackRequirements = StackRequirements::INVALID;
	}

	void FiberContext::Yield()
	{
		taskStatus = FiberTaskStatus::YIELDED;

		Fiber & schedulerFiber = threadContext->schedulerFiber;

#ifdef MT_INSTRUMENTED_BUILD		
		threadContext->NotifyTaskExecuteStateChanged( currentTask.debugColor, currentTask.debugID, TaskExecuteState::SUSPEND, (int32_t)fiberIndex);
#endif

		// Yielding, so reset thread context
		threadContext = nullptr;

		//switch to scheduler
		Fiber::SwitchTo(fiber, schedulerFiber);

#ifdef MT_INSTRUMENTED_BUILD
		threadContext->NotifyTaskExecuteStateChanged( currentTask.debugColor, currentTask.debugID, TaskExecuteState::RESUME, (int32_t)fiberIndex);
#endif
	}

	void FiberContext::RunSubtasksAndYieldImpl(ArrayView<internal::TaskBucket>& buckets)
	{
		MT_ASSERT(threadContext, "Sanity check failed!");
		MT_ASSERT(threadContext->taskScheduler, "Sanity check failed!");
		MT_ASSERT(threadContext->taskScheduler->IsWorkerThread(), "Can't use RunSubtasksAndYield outside Task. Use TaskScheduler.WaitGroup() instead.");
		MT_ASSERT(threadContext->threadId.IsEqual(ThreadId::Self()), "Thread context sanity check failed");

		// add to scheduler
		threadContext->taskScheduler->RunTasksImpl(buckets, this, false);

		//
		MT_ASSERT(threadContext->threadId.IsEqual(ThreadId::Self()), "Thread context sanity check failed");

		// Change status
		taskStatus = FiberTaskStatus::AWAITING_CHILD;

		Fiber & schedulerFiber = threadContext->schedulerFiber;

#ifdef MT_INSTRUMENTED_BUILD
		threadContext->NotifyTaskExecuteStateChanged( currentTask.debugColor, currentTask.debugID, TaskExecuteState::SUSPEND, (int32_t)fiberIndex);
#endif

		// Yielding, so reset thread context
		threadContext = nullptr;

		//switch to scheduler
		Fiber::SwitchTo(fiber, schedulerFiber);

#ifdef MT_INSTRUMENTED_BUILD
		threadContext->NotifyTaskExecuteStateChanged( currentTask.debugColor, currentTask.debugID, TaskExecuteState::RESUME, (int32_t)fiberIndex);
#endif

	}


	void FiberContext::RunAsync(TaskGroup taskGroup, const TaskHandle* taskHandleArray, uint32_t taskHandleCount)
	{
		MT_ASSERT(taskHandleCount < (internal::TASK_BUFFER_CAPACITY - 1), "Too many tasks per one Run.");
		MT_ASSERT(threadContext, "ThreadContext is nullptr");
		MT_ASSERT(threadContext->taskScheduler, "Sanity check failed!");
		MT_ASSERT(threadContext->taskScheduler->IsWorkerThread(), "Can't use RunAsync outside Task. Use TaskScheduler.RunAsync() instead.");

		TaskScheduler& scheduler = *(threadContext->taskScheduler);

		ArrayView<internal::GroupedTask> buffer(threadContext->descBuffer, taskHandleCount);

		uint32_t bucketCount = MT::Min((uint32_t)scheduler.GetWorkersCount(), taskHandleCount);
		ArrayView<internal::TaskBucket>	buckets(MT_ALLOCATE_ON_STACK(sizeof(internal::TaskBucket) * bucketCount), bucketCount);

		internal::DistibuteDescriptions(taskGroup, taskHandleArray, buffer, buckets);
		scheduler.RunTasksImpl(buckets, nullptr, false);
	}


	void FiberContext::RunSubtasksAndYield(TaskGroup taskGroup, const TaskHandle* taskHandleArray, uint32_t taskHandleCount)
	{
		MT_ASSERT(taskHandleCount < (internal::TASK_BUFFER_CAPACITY - 1), "Too many tasks per one Run.");
		MT_ASSERT(threadContext, "ThreadContext is nullptr");
		MT_ASSERT(threadContext->taskScheduler, "TaskScheduler is nullptr");

		TaskScheduler& scheduler = *(threadContext->taskScheduler);

		ArrayView<internal::GroupedTask> buffer(threadContext->descBuffer, taskHandleCount);

		uint32_t bucketCount = MT::Min((uint32_t)scheduler.GetWorkersCount(), taskHandleCount);
		ArrayView<internal::TaskBucket> buckets(MT_ALLOCATE_ON_STACK(sizeof(internal::TaskBucket) * bucketCount), bucketCount);

		internal::DistibuteDescriptions(taskGroup, taskHandleArray, buffer, buckets);
		RunSubtasksAndYieldImpl(buckets);
	}



}
