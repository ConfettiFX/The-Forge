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

#include "MTConfig.h"
#include "MTColorTable.h"
#include "MTTools.h"
#include "MTPlatform.h"
#include "MTQueueMPMC.h"
#include "MTArrayView.h"
#include "MTThreadContext.h"
#include "MTFiberContext.h"
#include "MTAppInterop.h"
#include "MTTaskPool.h"
#include "MTStackRequirements.h"
#include "Scopes/MTScopes.h"

/*
	You can inject some profiler code right into the task scope using this macro.
*/
#ifndef MT_SCHEDULER_PROFILER_TASK_SCOPE_CODE_INJECTION
#define MT_SCHEDULER_PROFILER_TASK_SCOPE_CODE_INJECTION( TYPE, DEBUG_COLOR, SRC_FILE, SRC_LINE)
#endif

namespace MT
{

	template<typename CLASS_TYPE, typename MACRO_TYPE>
	struct CheckType
	{
		static_assert(std::is_same<CLASS_TYPE, MACRO_TYPE>::value, "Invalid type in MT_DECLARE_TASK macro. See CheckType template instantiation params to details.");
	};

	struct TypeChecker
	{
		template <typename T>
		static T QueryThisType(T thisPtr)
		{
			MT_UNUSED(thisPtr);
			return (T)nullptr;
		}
	};


	template <typename T>
	inline void CallDtor(T* p)
	{
		MT_UNUSED(p);
		p->~T();
	}

}

#if MT_MSVC_COMPILER_FAMILY

// Visual Studio compile time check
#define MT_COMPILE_TIME_TYPE_CHECK(TYPE) \
	void CompileTimeCheckMethod() \
	{ \
		MT::CheckType< typename std::remove_pointer< decltype(MT::TypeChecker::QueryThisType(this)) >::type, typename TYPE > compileTypeTypesCheck; \
		compileTypeTypesCheck; \
	}

#elif MT_GCC_COMPILER_FAMILY

// GCC, Clang and other compilers compile time check
#define MT_COMPILE_TIME_TYPE_CHECK(TYPE) \
	void CompileTimeCheckMethod() \
	{ \
		/* query this pointer type */ \
		typedef decltype(MT::TypeChecker::QueryThisType(this)) THIS_PTR_TYPE; \
		/* query class type from this pointer type */ \
		typedef typename std::remove_pointer<THIS_PTR_TYPE>::type CPP_TYPE; \
		/* define macro type */ \
		typedef TYPE MACRO_TYPE; \
		/* compile time checking that is same types */ \
		MT::CheckType< CPP_TYPE, MACRO_TYPE > compileTypeTypesCheck; \
		/* remove unused variable warning */ \
		MT_UNUSED(compileTypeTypesCheck); \
	}

#else

#error Platform is not supported.

#endif


#define MT_DECLARE_TASK_IMPL(TYPE, STACK_REQUIREMENTS, TASK_PRIORITY, DEBUG_COLOR) \
	\
	MT_COMPILE_TIME_TYPE_CHECK(TYPE) \
	\
	static void TaskEntryPoint(MT::FiberContext& fiberContext, const void* userData) \
	{ \
		MT_SCHEDULER_PROFILER_TASK_SCOPE_CODE_INJECTION(TYPE, DEBUG_COLOR, __FILE__, __LINE__); \
		/* C style cast */ \
		TYPE * task = (TYPE *)(userData); \
		task->Do(fiberContext); \
	} \
	\
	static void PoolTaskDestroy(const void* userData) \
	{ \
		/* C style cast */ \
		TYPE * task = (TYPE *)(userData); \
		MT::CallDtor( task ); \
		/* Find task pool header */ \
		MT::PoolElementHeader * poolHeader = (MT::PoolElementHeader *)((char*)userData - sizeof(MT::PoolElementHeader)); \
		/* Fixup pool header, mark task as unused */ \
		poolHeader->id.Store(MT::TaskID::UNUSED); \
	} \
	\
	static MT::StackRequirements::Type GetStackRequirements() \
	{ \
		return STACK_REQUIREMENTS; \
	} \
	static MT::TaskPriority::Type GetTaskPriority() \
	{ \
		return TASK_PRIORITY; \
	} \



#ifdef MT_INSTRUMENTED_BUILD
#include "MTProfilerEventListener.h"

#define MT_DECLARE_TASK(TYPE, STACK_REQUIREMENTS, TASK_PRIORITY, DEBUG_COLOR) \
	static const mt_char* GetDebugID() \
	{ \
		return MT_TEXT( #TYPE ); \
	} \
	\
	static MT::Color::Type GetDebugColor() \
	{ \
		return DEBUG_COLOR; \
	} \
	\
	MT_DECLARE_TASK_IMPL(TYPE, STACK_REQUIREMENTS, TASK_PRIORITY, DEBUG_COLOR);

#else

#define MT_DECLARE_TASK(TYPE, STACK_REQUIREMENTS, TASK_PRIORITY, DEBUG_COLOR) \
	MT_DECLARE_TASK_IMPL(TYPE, STACK_REQUIREMENTS, TASK_PRIORITY, DEBUG_COLOR);

#endif




#if defined(MT_DEBUG) || defined(MT_INSTRUMENTED_BUILD)
#define MT_GROUP_DEBUG (1)
#endif



namespace MT
{
	const uint32 MT_MAX_THREAD_COUNT = 64;
	const uint32 MT_SCHEDULER_STACK_SIZE = 1048576; // 1Mb

	const uint32 MT_MAX_STANDART_FIBERS_COUNT = 256;
	const uint32 MT_STANDART_FIBER_STACK_SIZE = 32768; //32Kb

	const uint32 MT_MAX_EXTENDED_FIBERS_COUNT = 8;
	const uint32 MT_EXTENDED_FIBER_STACK_SIZE = 1048576; // 1Mb

	namespace internal
	{
		struct ThreadContext;
	}

	namespace TaskStealingMode
	{
		enum Type
		{
			DISABLED = 0,
			ENABLED = 1,
		};
	}

	struct WorkerThreadParams
	{
		uint32 core;
		ThreadPriority::Type priority;

		WorkerThreadParams()
			: core(MT_CPUCORE_ANY)
			, priority(ThreadPriority::DEFAULT)
		{
		}
	};

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Task scheduler
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	class TaskScheduler
	{
		friend class FiberContext;
		friend struct internal::ThreadContext;



		////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		// Task group description
		////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		// Application can assign task group to task and later wait until group was finished.
		class TaskGroupDescription
		{
			Atomic32<int32> inProgressTaskCount;

#if MT_GROUP_DEBUG
			bool debugIsFree;
#endif

		public:

			MT_NOCOPYABLE(TaskGroupDescription);

			TaskGroupDescription()
			{
				inProgressTaskCount.Store(0);
#if MT_GROUP_DEBUG
				debugIsFree = true;
#endif
			}

			int32 GetTaskCount() const
			{
				return inProgressTaskCount.Load();
			}

			int32 Dec()
			{
				return inProgressTaskCount.DecFetch();
			}

			int32 Inc()
			{
				return inProgressTaskCount.IncFetch();
			}

			int32 Add(int sum)
			{
				return inProgressTaskCount.AddFetch(sum);
			}

			Atomic32<int32>* GetWaitCounter()
			{
				return &inProgressTaskCount;
			}

#if MT_GROUP_DEBUG
			void SetDebugIsFree(bool _debugIsFree)
			{
				debugIsFree = _debugIsFree;
			}

			bool GetDebugIsFree() const
			{
				return debugIsFree;
			}
#endif
		};


		struct WaitContext
		{
			Atomic32<int32>* waitCounter;
			internal::ThreadContext* threadContext;
			uint32 waitTimeMs;
			uint32 exitCode;
		};


		// Thread index for new task
		Atomic32<int32> roundRobinThreadIndex;

		// Started threads count
		Atomic32<int32> startedThreadsCount;

		std::array<ThreadId, 4 > waitingThreads;
		Atomic32<int32> nextWaitingThreadSlotIndex;

		// Threads created by task manager
		Atomic32<int32> threadsCount;

		internal::ThreadContext threadContext[MT_MAX_THREAD_COUNT];

		// All groups task statistic
		TaskGroupDescription allGroups;

		// Groups pool
		LockFreeQueueMPMC<TaskGroup, TaskGroup::MT_MAX_GROUPS_COUNT * 2> availableGroups;

		//
		TaskGroupDescription groupStats[TaskGroup::MT_MAX_GROUPS_COUNT];

		// Fibers context
		FiberContext standartFiberContexts[MT_MAX_STANDART_FIBERS_COUNT];
		FiberContext extendedFiberContexts[MT_MAX_EXTENDED_FIBERS_COUNT];

		// Fibers pool
		LockFreeQueueMPMC<FiberContext*, MT_MAX_STANDART_FIBERS_COUNT * 2> standartFibersAvailable;
		LockFreeQueueMPMC<FiberContext*, MT_MAX_EXTENDED_FIBERS_COUNT * 2> extendedFibersAvailable;

#ifdef MT_INSTRUMENTED_BUILD
		IProfilerEventListener * profilerEventListener;
#endif

		bool taskStealingDisabled;

		FiberContext* RequestFiberContext(internal::GroupedTask& task);
		void ReleaseFiberContext(FiberContext*&& fiberExecutionContext);
		void RunTasksImpl(ArrayView<internal::TaskBucket>& buckets, FiberContext * parentFiber, bool restoredFromAwaitState);
		TaskGroupDescription & GetGroupDesc(TaskGroup group);

		static void WorkerThreadMain( void* userData );
		static void SchedulerFiberMain( void* userData );
		static void SchedulerFiberWait( void* userData );
		static bool SchedulerFiberStep( internal::ThreadContext& context, bool disableTaskStealing);
		static void SchedulerFiberProcessTask( internal::ThreadContext& context, internal::GroupedTask& task );
		static void FiberMain( void* userData );
		static bool TryStealTask(internal::ThreadContext& threadContext, internal::GroupedTask & task);

		static FiberContext* ExecuteTask (internal::ThreadContext& threadContext, FiberContext* fiberContext);

	public:

		/// \brief Initializes a new instance of the TaskScheduler class.
		/// \param workerThreadsCount Worker threads count. Automatically determines the required number of threads if workerThreadsCount set to 0
#ifdef MT_INSTRUMENTED_BUILD
		TaskScheduler(uint32 workerThreadsCount = 0, WorkerThreadParams* workerParameters = nullptr, IProfilerEventListener* listener = nullptr, TaskStealingMode::Type stealMode = TaskStealingMode::ENABLED);
#else
		TaskScheduler(uint32 workerThreadsCount = 0, WorkerThreadParams* workerParameters = nullptr, TaskStealingMode::Type stealMode = TaskStealingMode::ENABLED);
#endif


		~TaskScheduler();

		void JoinWorkerThreads();

		template<class TTask>
		void RunAsync(TaskGroup group, const TTask* taskArray, uint32 taskCount);

		void RunAsync(TaskGroup group, const TaskHandle* taskHandleArray, uint32 taskHandleCount);

		/// \brief Wait while no more tasks in specific group.
		/// \return true - if no more tasks in specific group. false - if timeout in milliseconds has reached and group still has some tasks.
		bool WaitGroup(TaskGroup group, uint32 milliseconds);

		bool WaitAll(uint32 milliseconds);

		TaskGroup CreateGroup();
		void ReleaseGroup(TaskGroup group);

		int32 GetWorkersCount() const;

		bool IsTaskStealingDisabled(uint32 minWorkersCount = 1) const;

		bool IsWorkerThread() const;

#ifdef MT_INSTRUMENTED_BUILD

		inline IProfilerEventListener* GetProfilerEventListener()
		{
			return profilerEventListener;
		}		

		void NotifyFibersCreated(uint32 fibersCount);
		void NotifyThreadsCreated(uint32 threadsCount);


#endif
	};
}

#include "MTScheduler.inl"
#include "MTFiberContext.inl"
