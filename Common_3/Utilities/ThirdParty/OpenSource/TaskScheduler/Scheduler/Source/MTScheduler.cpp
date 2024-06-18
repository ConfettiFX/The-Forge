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
#include "../Include/MTStaticVector.h"
#include <string.h> // for memset


//  Enable low latency experimental wait code path.
//  Look like low latency hybrid wait is work better for PS4/X1, but a little worse on PC
//#define MT_LOW_LATENCY_EXPERIMENTAL_WAIT (1)

#if defined(MT_PLATFORM_DURANGO) || defined(MT_PLATFORM_ORBIS)
#define MT_LOW_LATENCY_EXPERIMENTAL_WAIT (1)
#endif

namespace MT
{
#ifdef MT_INSTRUMENTED_BUILD
	TaskScheduler::TaskScheduler(uint32 workerThreadsCount, WorkerThreadParams* workerParameters, IProfilerEventListener* listener, TaskStealingMode::Type stealMode)
#else
	TaskScheduler::TaskScheduler(uint32 workerThreadsCount, WorkerThreadParams* workerParameters, TaskStealingMode::Type stealMode)
#endif
		: roundRobinThreadIndex(0)
		, startedThreadsCount(0)
		, taskStealingDisabled(stealMode == TaskStealingMode::DISABLED)
	{

#ifdef MT_INSTRUMENTED_BUILD
		profilerEventListener = listener;
#endif
        
		if (workerThreadsCount != 0)
		{
			threadsCount.StoreRelaxed( MT::Clamp(workerThreadsCount, (uint32)1, (uint32)MT_MAX_THREAD_COUNT) );
		} else
		{
			//query number of processor
			threadsCount.StoreRelaxed( (uint32)MT::Clamp(Thread::GetNumberOfHardwareThreads() - 1, 1, (int)MT_MAX_THREAD_COUNT) );
		}

		uint32 fiberIndex = 0;

		// create fiber pool (fibers with standard stack size)
		for (uint32 i = 0; i < MT_MAX_STANDARD_FIBERS_COUNT; i++)
		{
			FiberContext& context = standardFiberContexts[i];
			context.fiber.Create(MT_STANDARD_FIBER_STACK_SIZE, FiberMain, &context);
			context.fiberIndex = fiberIndex;
			bool res = standardFibersAvailable.TryPush( &context );
			MT_USED_IN_ASSERT(res);
			MT_ASSERT(res == true, "Can't add fiber to storage");
			fiberIndex++;
		}

		// create fiber pool (fibers with extended stack size)
		for (uint32 i = 0; i < MT_MAX_EXTENDED_FIBERS_COUNT; i++)
		{
			FiberContext& context = extendedFiberContexts[i];
			context.fiber.Create(MT_EXTENDED_FIBER_STACK_SIZE, FiberMain, &context);
			context.fiberIndex = fiberIndex;
			bool res = extendedFibersAvailable.TryPush( &context );
			MT_USED_IN_ASSERT(res);
			MT_ASSERT(res == true, "Can't add fiber to storage");
			fiberIndex++;
		}

#ifdef MT_INSTRUMENTED_BUILD
		NotifyFibersCreated(MT_MAX_STANDARD_FIBERS_COUNT + MT_MAX_EXTENDED_FIBERS_COUNT);
#endif

		for (int16 i = 0; i < TaskGroup::MT_MAX_GROUPS_COUNT; i++)
		{
			if (i != TaskGroup::DEFAULT)
			{
				bool res = availableGroups.TryPush( TaskGroup(i) );
				MT_USED_IN_ASSERT(res);
				MT_ASSERT(res == true, "Can't add group to storage");
			}
		}

#if MT_GROUP_DEBUG
		groupStats[TaskGroup::DEFAULT].SetDebugIsFree(false);
#endif

		// create worker thread pool
		int32 totalThreadsCount = GetWorkersCount();

#ifdef MT_INSTRUMENTED_BUILD
		NotifyThreadsCreated(totalThreadsCount);
#endif

		for (int32 i = 0; i < totalThreadsCount; i++)
		{
			threadContext[i].SetThreadIndex(i);
			threadContext[i].taskScheduler = this;

			uint32 threadCore = i;
			ThreadPriority::Type priority = ThreadPriority::DEFAULT;
			if (workerParameters != nullptr)
			{
				const WorkerThreadParams& params = workerParameters[i];

				threadCore = params.core;
				priority = params.priority;
			}

			threadContext[i].thread.Start( MT_SCHEDULER_STACK_SIZE, WorkerThreadMain, &threadContext[i], threadCore, priority);
		}
	}

	void TaskScheduler::JoinWorkerThreads()
	{
		int32 totalThreadsCount = GetWorkersCount();
		for (int32 i = 0; i < totalThreadsCount; i++)
		{
			threadContext[i].state.Store(internal::ThreadState::EXIT);
			threadContext[i].hasNewTasksEvent.Signal();
		}

		for (int32 i = 0; i < totalThreadsCount; i++)
		{
			threadContext[i].thread.Join();
		}
		threadsCount.Store(0);
	}

	TaskScheduler::~TaskScheduler()
	{
		if (GetWorkersCount() > 0)
		{
			JoinWorkerThreads();
		}
	}

	FiberContext* TaskScheduler::RequestFiberContext(internal::GroupedTask& task)
	{
		FiberContext *fiberContext = task.awaitingFiber;
		if (fiberContext)
		{
			task.awaitingFiber = nullptr;
			return fiberContext;
		}

		MT::StackRequirements::Type stackRequirements = task.desc.stackRequirements;

		fiberContext = nullptr;
		bool res = false;
		MT_USED_IN_ASSERT(res);
		switch(stackRequirements)
		{
		case MT::StackRequirements::STANDARD:
			res = standardFibersAvailable.TryPop(fiberContext);
            MT_USED_IN_ASSERT(res);
			MT_ASSERT(res, "Can't get more standard fibers!");
			break;
		case MT::StackRequirements::EXTENDED:
			res = extendedFibersAvailable.TryPop(fiberContext);
            MT_USED_IN_ASSERT(res);
			MT_ASSERT(res, "Can't get more extended fibers!");
			break;
		default:
			MT_REPORT_ASSERT("Unknown stack requrements");
		}

		MT_ASSERT(fiberContext != nullptr, "Can't get more fibers. Too many tasks in flight simultaneously?");

		fiberContext->currentTask = task.desc;
		fiberContext->currentGroup = task.group;
		fiberContext->parentFiber = task.parentFiber;
		fiberContext->stackRequirements = stackRequirements;
		return fiberContext;
	}

	void TaskScheduler::ReleaseFiberContext(FiberContext*&& fiberContext)
	{
		MT_ASSERT(fiberContext, "Can't release nullptr Fiber. fiberContext is nullptr");

		MT::StackRequirements::Type stackRequirements = fiberContext->stackRequirements;
		fiberContext->Reset();

		MT_ASSERT(fiberContext != nullptr, "Fiber context can't be nullptr");

		bool res = false;
		MT_USED_IN_ASSERT(res);
		switch(stackRequirements)
		{
		case MT::StackRequirements::STANDARD:
			res = standardFibersAvailable.TryPush(std::move(fiberContext));
			break;
		case MT::StackRequirements::EXTENDED:
			res = extendedFibersAvailable.TryPush(std::move(fiberContext));
			break;
		default:
			MT_REPORT_ASSERT("Unknown stack requrements");
		}

		MT_USED_IN_ASSERT(res);
		MT_ASSERT(res != false, "Can't return fiber to storage");
	}

	FiberContext* TaskScheduler::ExecuteTask(internal::ThreadContext& threadContext, FiberContext* fiberContext)
	{
		MT_ASSERT(threadContext.threadId.IsEqual(ThreadId::Self()), "Thread context sanity check failed");

		MT_ASSERT(fiberContext, "Invalid fiber context");
		MT_ASSERT(fiberContext->currentTask.IsValid(), "Invalid task");

		// Set actual thread context to fiber
		fiberContext->SetThreadContext(&threadContext);

		// Update task status
		fiberContext->SetStatus(FiberTaskStatus::RUNNED);

		MT_ASSERT(fiberContext->GetThreadContext()->threadId.IsEqual(ThreadId::Self()), "Thread context sanity check failed");

		const void* poolUserData = fiberContext->currentTask.userData;
		TPoolTaskDestroy poolDestroyFunc = fiberContext->currentTask.poolDestroyFunc;

#ifdef MT_INSTRUMENTED_BUILD
		threadContext.NotifyTaskExecuteStateChanged( MT_SYSTEM_TASK_COLOR, MT_SYSTEM_TASK_NAME, TaskExecuteState::STOP, MT_SYSTEM_FIBER_INDEX);
#endif

		// Run current task code
		Fiber::SwitchTo(threadContext.schedulerFiber, fiberContext->fiber);

#ifdef MT_INSTRUMENTED_BUILD
		threadContext.NotifyTaskExecuteStateChanged( MT_SYSTEM_TASK_COLOR, MT_SYSTEM_TASK_NAME, TaskExecuteState::START, MT_SYSTEM_FIBER_INDEX);
#endif

		// If task was done
		FiberTaskStatus::Type taskStatus = fiberContext->GetStatus();
		if (taskStatus == FiberTaskStatus::FINISHED)
		{
			//destroy task (call dtor) for "fire and forget" type of task from TaskPool
			if (poolDestroyFunc != nullptr)
			{
				poolDestroyFunc(poolUserData);
			}

			TaskGroup taskGroup = fiberContext->currentGroup;

			TaskScheduler::TaskGroupDescription  & groupDesc = threadContext.taskScheduler->GetGroupDesc(taskGroup);

			// Update group status
			int groupTaskCount = groupDesc.Dec();
			MT_ASSERT(groupTaskCount >= 0, "Sanity check failed!");
			if (groupTaskCount == 0)
			{
				fiberContext->currentGroup = TaskGroup::INVALID;
			}

			// Update total task count
			int allGroupTaskCount = threadContext.taskScheduler->allGroups.Dec();
			MT_USED_IN_ASSERT(allGroupTaskCount);
			MT_ASSERT(allGroupTaskCount >= 0, "Sanity check failed!");

			FiberContext* parentFiberContext = fiberContext->parentFiber;
			if (parentFiberContext != nullptr)
			{
				int childrenFibersCount = parentFiberContext->childrenFibersCount.DecFetch();
				MT_ASSERT(childrenFibersCount >= 0, "Sanity check failed!");

				if (childrenFibersCount == 0)
				{
					// This is a last subtask. Restore parent task
					MT_ASSERT(threadContext.threadId.IsEqual(ThreadId::Self()), "Thread context sanity check failed");
					MT_ASSERT(parentFiberContext->GetThreadContext() == nullptr, "Inactive parent should not have a valid thread context");

					// WARNING!! Thread context can changed here! Set actual current thread context.
					parentFiberContext->SetThreadContext(&threadContext);

					MT_ASSERT(parentFiberContext->GetThreadContext()->threadId.IsEqual(ThreadId::Self()), "Thread context sanity check failed");

					// All subtasks is done.
					// Exiting and return parent fiber to scheduler
					return parentFiberContext;
				} else
				{
					// Other subtasks still exist
					// Exiting
					return nullptr;
				}
			} else
			{
				// Task is finished and no parent task
				// Exiting
				return nullptr;
			}
		}

		MT_ASSERT(taskStatus != FiberTaskStatus::RUNNED, "Incorrect task status")
		return nullptr;
	}


	void TaskScheduler::FiberMain(void* userData)
	{
		FiberContext& fiberContext = *(FiberContext*)(userData);
		for(;;)
		{
			MT_ASSERT(fiberContext.currentTask.IsValid(), "Invalid task in fiber context");
			MT_ASSERT(fiberContext.GetThreadContext(), "Invalid thread context");
			MT_ASSERT(fiberContext.GetThreadContext()->threadId.IsEqual(ThreadId::Self()), "Thread context sanity check failed");

#ifdef MT_INSTRUMENTED_BUILD
			fiberContext.fiber.SetName( MT_SYSTEM_TASK_FIBER_NAME );
			fiberContext.GetThreadContext()->NotifyTaskExecuteStateChanged( fiberContext.currentTask.debugColor, fiberContext.currentTask.debugID, TaskExecuteState::START, (int32)fiberContext.fiberIndex);
#endif

			fiberContext.currentTask.taskFunc( fiberContext, fiberContext.currentTask.userData );
			fiberContext.SetStatus(FiberTaskStatus::FINISHED);

#ifdef MT_INSTRUMENTED_BUILD
			fiberContext.fiber.SetName( MT_SYSTEM_TASK_FIBER_NAME );
			fiberContext.GetThreadContext()->NotifyTaskExecuteStateChanged( fiberContext.currentTask.debugColor, fiberContext.currentTask.debugID, TaskExecuteState::STOP, (int32)fiberContext.fiberIndex);
#endif

			Fiber::SwitchTo(fiberContext.fiber, fiberContext.GetThreadContext()->schedulerFiber);
		}

	}


	bool TaskScheduler::TryStealTask(internal::ThreadContext& threadContext, internal::GroupedTask & task)
	{
		uint32 workersCount = threadContext.taskScheduler->GetWorkersCount();

		uint32 victimIndex = threadContext.random.Get();

		for (uint32 attempt = 0; attempt < workersCount; attempt++)
		{
			uint32 index = victimIndex % workersCount;
			if (index == threadContext.workerIndex)
			{
				victimIndex++;
				index = victimIndex % workersCount;
			}

			internal::ThreadContext& victimContext = threadContext.taskScheduler->threadContext[index];
			if (victimContext.queue.TryPopNewest(task))
			{
				return true;
			}

			victimIndex++;
		}
		return false;
	}

	void TaskScheduler::WorkerThreadMain( void* userData )
	{
		internal::ThreadContext& context = *(internal::ThreadContext*)(userData);
		MT_ASSERT(context.taskScheduler, "Task scheduler must be not null!");

		context.threadId = ThreadId::Self();

#ifdef MT_INSTRUMENTED_BUILD
		const char* threadNames[] = {"worker0","worker1","worker2","worker3","worker4","worker5","worker6","worker7","worker8","worker9","worker10","worker11","worker12"};
		if (context.workerIndex < MT_ARRAY_SIZE(threadNames))
		{
			Thread::SetThreadName(threadNames[context.workerIndex]);
		} else
		{
			Thread::SetThreadName("worker_thread");
		}
#endif

		context.schedulerFiber.CreateFromCurrentThreadAndRun(SchedulerFiberMain, userData);
	}


	void TaskScheduler::SchedulerFiberWait( void* userData )
	{
		WaitContext& waitContext = *(WaitContext*)(userData);
		internal::ThreadContext& context = *waitContext.threadContext;
		MT_ASSERT(context.taskScheduler, "Task scheduler must be not null!");
		MT_ASSERT(waitContext.waitCounter, "Wait counter must be not null!");

#ifdef MT_INSTRUMENTED_BUILD
		context.NotifyTemporaryWorkerThreadJoin();

		context.NotifyWaitStarted();
		context.NotifyTaskExecuteStateChanged( MT_SYSTEM_TASK_COLOR, MT_SYSTEM_TASK_NAME, TaskExecuteState::START, MT_SYSTEM_FIBER_INDEX);
#endif

		bool isTaskStealingDisabled = context.taskScheduler->IsTaskStealingDisabled(0);

		int64 timeOut = GetTimeMicroSeconds() + (waitContext.waitTimeMs * 1000);

		SpinWait spinWait;
		
		for(;;)
		{
			if ( SchedulerFiberStep(context, isTaskStealingDisabled) == false )
			{
				spinWait.SpinOnce();
			} else
			{
				spinWait.Reset();
			}

			int32 groupTaskCount = waitContext.waitCounter->Load();
			if (groupTaskCount == 0)
			{
				waitContext.exitCode = 0;
				break;
			}

			int64 timeNow = GetTimeMicroSeconds();
			if (timeNow >= timeOut)
			{
				waitContext.exitCode = 1;
				break;
			}
		}

#ifdef MT_INSTRUMENTED_BUILD
		context.NotifyTaskExecuteStateChanged( MT_SYSTEM_TASK_COLOR, MT_SYSTEM_TASK_NAME, TaskExecuteState::STOP, MT_SYSTEM_FIBER_INDEX);
		context.NotifyWaitFinished();

		context.NotifyTemporaryWorkerThreadLeave();
#endif
	}

	void TaskScheduler::SchedulerFiberMain( void* userData )
	{
		internal::ThreadContext& context = *(internal::ThreadContext*)(userData);
		MT_ASSERT(context.taskScheduler, "Task scheduler must be not null!");

#ifdef MT_INSTRUMENTED_BUILD
		context.NotifyThreadCreated(context.workerIndex);
#endif

		int32 totalThreadsCount = context.taskScheduler->threadsCount.LoadRelaxed();
		context.taskScheduler->startedThreadsCount.IncFetch();

		//Simple spinlock until all threads is started and initialized
		for(;;)
		{
			int32 initializedThreadsCount = context.taskScheduler->startedThreadsCount.Load();
			if (initializedThreadsCount == totalThreadsCount)
			{
				break;
			}

			// sleep some time until all other thread initialized
			Thread::Sleep(1);
		}

		HardwareFullMemoryBarrier();

#ifdef MT_INSTRUMENTED_BUILD
		context.NotifyThreadStarted(context.workerIndex);
		context.NotifyTaskExecuteStateChanged( MT_SYSTEM_TASK_COLOR, MT_SYSTEM_TASK_NAME, TaskExecuteState::START, MT_SYSTEM_FIBER_INDEX);
#endif
		bool isTaskStealingDisabled = context.taskScheduler->IsTaskStealingDisabled();

		while(context.state.Load() != internal::ThreadState::EXIT)
		{
			if ( SchedulerFiberStep(context, isTaskStealingDisabled) == false)
			{
#ifdef MT_INSTRUMENTED_BUILD
				context.NotifyThreadIdleStarted(context.workerIndex);
#endif

#if MT_LOW_LATENCY_EXPERIMENTAL_WAIT

				SpinWait spinWait;

				for(;;)
				{
					// Queue is empty and stealing attempt has failed.
					// Fast Spin Wait for new tasks
					if (spinWait.SpinOnce() >= SpinWait::YIELD_SLEEP0_THRESHOLD)
					{
						// Fast Spin wait for new tasks has failed.
						// Wait for new events using events
						context.hasNewTasksEvent.Wait(20000);

						spinWait.Reset();

#ifdef MT_INSTRUMENTED_BUILD
						context.NotifyThreadIdleFinished(context.workerIndex);
#endif

						break;
					}

					internal::GroupedTask task;
					if ( context.queue.TryPopOldest(task) )
					{
#ifdef MT_INSTRUMENTED_BUILD
						context.NotifyThreadIdleFinished(context.workerIndex);
#endif

						SchedulerFiberProcessTask(context, task);

						break;
					}

				}
#else
				// Queue is empty and stealing attempt has failed.
				// Wait for new events using events
				context.hasNewTasksEvent.Wait(20000);

#ifdef MT_INSTRUMENTED_BUILD
				context.NotifyThreadIdleFinished(context.workerIndex);
#endif

#endif

			}

		} // main thread loop

#ifdef MT_INSTRUMENTED_BUILD
		context.NotifyTaskExecuteStateChanged( MT_SYSTEM_TASK_COLOR, MT_SYSTEM_TASK_NAME, TaskExecuteState::STOP, MT_SYSTEM_FIBER_INDEX);
		context.NotifyThreadStoped(context.workerIndex);
#endif

	}

	void TaskScheduler::SchedulerFiberProcessTask( internal::ThreadContext& context, internal::GroupedTask& task )
	{
#ifdef MT_INSTRUMENTED_BUILD
		bool isNewTask = (task.awaitingFiber == nullptr);
#endif

		// There is a new task
		FiberContext* fiberContext = context.taskScheduler->RequestFiberContext(task);
		MT_ASSERT(fiberContext, "Can't get execution context from pool");
		MT_ASSERT(fiberContext->currentTask.IsValid(), "Sanity check failed");
		MT_ASSERT(fiberContext->stackRequirements == task.desc.stackRequirements, "Sanity check failed");

		while(fiberContext)
		{
#ifdef MT_INSTRUMENTED_BUILD
			if (isNewTask)
			{
				//TODO:
				isNewTask = false;
			}
#endif
			// prevent invalid fiber resume from child tasks, before ExecuteTask is done
			fiberContext->childrenFibersCount.IncFetch();

			FiberContext* parentFiber = ExecuteTask(context, fiberContext);

			FiberTaskStatus::Type taskStatus = fiberContext->GetStatus();

			//release guard
			int childrenFibersCount = fiberContext->childrenFibersCount.DecFetch();

			// Can drop fiber context - task is finished
			if (taskStatus == FiberTaskStatus::FINISHED)
			{
				MT_ASSERT( childrenFibersCount == 0, "Sanity check failed");
				context.taskScheduler->ReleaseFiberContext(std::move(fiberContext));

				// If parent fiber is exist transfer flow control to parent fiber, if parent fiber is null, exit
				fiberContext = parentFiber;
			} else
			{
				MT_ASSERT( childrenFibersCount >= 0, "Sanity check failed");

				// No subtasks here and status is not finished, this mean all subtasks already finished before parent return from ExecuteTask
				if (childrenFibersCount == 0)
				{
					MT_ASSERT(parentFiber == nullptr, "Sanity check failed");
				} else
				{
					// If subtasks still exist, drop current task execution. task will be resumed when last subtask finished
					break;
				}

				// If task is yielded execution, get another task from queue.
				if (taskStatus == FiberTaskStatus::YIELDED)
				{
					// Task is yielded, add to tasks queue
					ArrayView<internal::GroupedTask> buffer(context.descBuffer, 1);
					ArrayView<internal::TaskBucket> buckets( MT_ALLOCATE_ON_STACK(sizeof(internal::TaskBucket)), 1 );

					FiberContext* yieldedTask = fiberContext;
					StaticVector<FiberContext*, 1> yieldedTasksQueue(1, yieldedTask);
					internal::DistibuteDescriptions( TaskGroup(TaskGroup::ASSIGN_FROM_CONTEXT), yieldedTasksQueue.Begin(), buffer, buckets );

					// add yielded task to scheduler
					context.taskScheduler->RunTasksImpl(buckets, nullptr, true);

					// ATENTION! yielded task can be already completed at this point

					break;
				}
			}
		} //while(fiberContext)
	}

	bool TaskScheduler::SchedulerFiberStep( internal::ThreadContext& context, bool disableTaskStealing)
	{
		internal::GroupedTask task;
		if ( context.queue.TryPopOldest(task) || (disableTaskStealing == false && TryStealTask(context, task) ) )
		{
			SchedulerFiberProcessTask(context, task);
			return true;
		}

		return false;
	}

	void TaskScheduler::RunTasksImpl(ArrayView<internal::TaskBucket>& buckets, FiberContext * parentFiber, bool restoredFromAwaitState)
	{

#if MT_LOW_LATENCY_EXPERIMENTAL_WAIT
		// Early wakeup worker threads (worker thread spin wait for some time before sleep)
		int32 roundRobinIndex = roundRobinThreadIndex.LoadRelaxed();
		for (size_t i = 0; i < buckets.Size(); ++i)
		{
			int bucketIndex = ((roundRobinIndex + i) % threadsCount.LoadRelaxed());
			internal::ThreadContext & context = threadContext[bucketIndex];
			context.hasNewTasksEvent.Signal();
		}
#endif


		// This storage is necessary to calculate how many tasks we add to different groups
		int newTaskCountInGroup[TaskGroup::MT_MAX_GROUPS_COUNT];

		// Default value is 0
		memset(&newTaskCountInGroup[0], 0, sizeof(newTaskCountInGroup));

		// Set parent fiber pointer
		// Calculate the number of tasks per group
		// Calculate total number of tasks
		size_t count = 0;
		for (size_t i = 0; i < buckets.Size(); ++i)
		{
			internal::TaskBucket& bucket = buckets[i];
			for (size_t taskIndex = 0; taskIndex < bucket.count; taskIndex++)
			{
				internal::GroupedTask & task = bucket.tasks[taskIndex];

				task.parentFiber = parentFiber;

				int idx = task.group.GetValidIndex();
				MT_ASSERT(idx >= 0 && idx < TaskGroup::MT_MAX_GROUPS_COUNT, "Invalid index");
				newTaskCountInGroup[idx]++;
			}

			count += bucket.count;
		}

		// Increments child fibers count on parent fiber
		if (parentFiber)
		{
			parentFiber->childrenFibersCount.AddFetch((int)count);
		}

		if (restoredFromAwaitState == false)
		{
			// Increase the number of active tasks in the group using data from temporary storage
			for (size_t i = 0; i < TaskGroup::MT_MAX_GROUPS_COUNT; i++)
			{
				int groupNewTaskCount = newTaskCountInGroup[i];
				if (groupNewTaskCount > 0)
				{
					groupStats[i].Add((uint32)groupNewTaskCount);
				}
			}

			// Increments all task in progress counter
			allGroups.Add((uint32)count);
		} else
		{
			// If task's restored from await state, counters already in correct state
		}

		// Add to thread queue
		for (size_t i = 0; i < buckets.Size(); ++i)
		{
			int bucketIndex = roundRobinThreadIndex.IncFetch() % threadsCount.LoadRelaxed();
			internal::ThreadContext & context = threadContext[bucketIndex];

			internal::TaskBucket& bucket = buckets[i];

			for(;;)
			{
				MT_ASSERT(bucket.count < (internal::TASK_BUFFER_CAPACITY - 1), "Sanity check failed. Too many tasks per one bucket.");
				
				bool res = context.queue.Add(bucket.tasks, bucket.count);
				if (res == true)
				{
					break;
				}

				//Can't add new tasks onto the queue. Look like the job system is overloaded. Wait some time and try again.
				//TODO: implement waiting until workers done using events.
				Thread::Sleep(10);
			}
			
			context.hasNewTasksEvent.Signal();
		}
	}

	void TaskScheduler::RunAsync(TaskGroup group, const TaskHandle* taskHandleArray, uint32 taskHandleCount)
	{
		MT_ASSERT(!IsWorkerThread(), "Can't use RunAsync inside Task. Use FiberContext.RunAsync() instead.");

		ArrayView<internal::GroupedTask> buffer(MT_ALLOCATE_ON_STACK(sizeof(internal::GroupedTask) * taskHandleCount), taskHandleCount);

		uint32 bucketCount = MT::Min((uint32)GetWorkersCount(), taskHandleCount);
		ArrayView<internal::TaskBucket> buckets(MT_ALLOCATE_ON_STACK(sizeof(internal::TaskBucket) * bucketCount), bucketCount);

		internal::DistibuteDescriptions(group, taskHandleArray, buffer, buckets);
		RunTasksImpl(buckets, nullptr, false);
	}

	bool TaskScheduler::WaitGroup(TaskGroup group, uint32 milliseconds)
	{
		MT_VERIFY(IsWorkerThread() == false, "Can't use WaitGroup inside Task. Use FiberContext.WaitGroupAndYield() instead.", return false);

		TaskScheduler::TaskGroupDescription& groupDesc = GetGroupDesc(group);

		// Early exit if not tasks in group
		int32 taskCount = groupDesc.GetTaskCount();
		if (taskCount == 0)
		{
			return true;
		}

		size_t bytesCountForDescBuffer = internal::ThreadContext::GetMemoryRequrementInBytesForDescBuffer();
		void* descBuffer = MT_ALLOCATE_ON_STACK(bytesCountForDescBuffer);
		
		internal::ThreadContext context(descBuffer);
		context.taskScheduler = this;
		context.SetThreadIndex(0xFFFFFFFF);
		context.threadId = ThreadId::Self();

		WaitContext waitContext;
		waitContext.threadContext = &context;
		waitContext.waitCounter = groupDesc.GetWaitCounter();
		waitContext.waitTimeMs = milliseconds;
		waitContext.exitCode = 0;

		int32 waitingSlotIndex = nextWaitingThreadSlotIndex.IncFetch();
		waitingThreads[waitingSlotIndex % waitingThreads.size()] = ThreadId::Self();
		context.schedulerFiber.CreateFromCurrentThreadAndRun(SchedulerFiberWait, &waitContext);
		
		MT_ASSERT( waitingThreads[waitingSlotIndex % waitingThreads.size()].IsEqual(ThreadId::Self()), "waitingThreads array overflow");
		waitingThreads[waitingSlotIndex % waitingThreads.size()] = ThreadId();

		return (waitContext.exitCode == 0);
	}

	bool TaskScheduler::WaitAll(uint32 milliseconds)
	{
		MT_VERIFY(IsWorkerThread() == false, "Can't use WaitAll inside Task.", return false);

		// Early exit if not tasks in group
		int32 taskCount = allGroups.GetTaskCount();
		if (taskCount == 0)
		{
			return true;
		}

		size_t bytesCountForDescBuffer = internal::ThreadContext::GetMemoryRequrementInBytesForDescBuffer();
		void* descBuffer = MT_ALLOCATE_ON_STACK(bytesCountForDescBuffer);

		internal::ThreadContext context(descBuffer);
		context.taskScheduler = this;
		context.SetThreadIndex(0xFFFFFFFF);
		context.threadId = ThreadId::Self();

		WaitContext waitContext;
		waitContext.threadContext = &context;
		waitContext.waitCounter = allGroups.GetWaitCounter();
		waitContext.waitTimeMs = milliseconds;
		waitContext.exitCode = 0;

		int32 waitingSlotIndex = nextWaitingThreadSlotIndex.IncFetch();
		waitingThreads[waitingSlotIndex % waitingThreads.size()] = ThreadId::Self();

		context.schedulerFiber.CreateFromCurrentThreadAndRun(SchedulerFiberWait, &waitContext);

		MT_ASSERT( waitingThreads[waitingSlotIndex % waitingThreads.size()].IsEqual(ThreadId::Self()), "waitingThreads array overflow");
		waitingThreads[waitingSlotIndex % waitingThreads.size()] = ThreadId();

		return (waitContext.exitCode == 0);
	}

	bool TaskScheduler::IsTaskStealingDisabled(uint32 minWorkersCount) const
	{
		if (threadsCount.LoadRelaxed() <= (int32)minWorkersCount)
		{
			return true;
		}

		return taskStealingDisabled;
	}

	int32 TaskScheduler::GetWorkersCount() const
	{
		return threadsCount.LoadRelaxed();
	}


	bool TaskScheduler::IsWorkerThread() const
	{
		int32 workersCount = GetWorkersCount();
		for (int32 i = 0; i < workersCount ; i++)
		{
			if (threadContext[i].threadId.IsEqual(ThreadId::Self()))
			{
				return true;
			}
		}
		for (uint32 i = 0; i < waitingThreads.size(); i++)
		{
			if (waitingThreads[i].IsEqual(ThreadId::Self()))
				return true;
		}

		return false;
	}

	TaskGroup TaskScheduler::CreateGroup()
	{
		MT_ASSERT(IsWorkerThread() == false, "Can't use CreateGroup inside Task.");

		TaskGroup group;
		if (!availableGroups.TryPop(group))
		{
			MT_REPORT_ASSERT("Group pool is empty");
		}

		int idx = group.GetValidIndex();
		MT_USED_IN_ASSERT(idx);
		MT_ASSERT(groupStats[idx].GetDebugIsFree() == true, "Bad logic!");
#if MT_GROUP_DEBUG
		groupStats[idx].SetDebugIsFree(false);
#endif

		return group;
	}

	void TaskScheduler::ReleaseGroup(TaskGroup group)
	{
		MT_ASSERT(IsWorkerThread() == false, "Can't use ReleaseGroup inside Task.");
		MT_ASSERT(group.IsValid(), "Invalid group ID");

		int idx = group.GetValidIndex();
		MT_USED_IN_ASSERT(idx);
		MT_ASSERT(groupStats[idx].GetDebugIsFree() == false, "Group already released");
#if MT_GROUP_DEBUG
		groupStats[idx].SetDebugIsFree(true);
#endif

		bool res = availableGroups.TryPush(std::move(group));
		MT_USED_IN_ASSERT(res);
		MT_ASSERT(res, "Can't return group to pool");
	}

	TaskScheduler::TaskGroupDescription& TaskScheduler::GetGroupDesc(TaskGroup group)
	{
		MT_ASSERT(group.IsValid(), "Invalid group ID");

		int idx = group.GetValidIndex();
		TaskScheduler::TaskGroupDescription & groupDesc = groupStats[idx];

		MT_ASSERT(groupDesc.GetDebugIsFree() == false, "Invalid group");
		return groupDesc;
	}


#ifdef MT_INSTRUMENTED_BUILD

	void TaskScheduler::NotifyFibersCreated(uint32 fibersCount)
	{
		if (IProfilerEventListener* eventListener = GetProfilerEventListener())
		{
			eventListener->OnFibersCreated(fibersCount);
		}
	}

	void TaskScheduler::NotifyThreadsCreated(uint32 threadsCount)
	{
		if (IProfilerEventListener* eventListener = GetProfilerEventListener())
		{
			eventListener->OnThreadsCreated(threadsCount);
		}
	}


#endif

}


