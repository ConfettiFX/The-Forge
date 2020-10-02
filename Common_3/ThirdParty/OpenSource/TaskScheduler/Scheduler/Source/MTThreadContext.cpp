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
	namespace internal
	{
		// Prime numbers for linear congruential generator seed
		static const uint32_t primeNumbers[] = {
			128473, 135349, 159499, 173839, 209213, 241603, 292709, 314723,
			343943, 389299, 419473, 465169, 518327, 649921, 748271, 851087,
			862171, 974551, 1002973, 1034639, 1096289, 1153123, 1251037, 1299269,
			1272941, 1252151, 1231091, 1206761, 1185469, 1169933, 1141351, 1011583 };

		uint32_t GetPrimeNumber(uint32_t index)
		{
			return primeNumbers[index % MT_ARRAY_SIZE(primeNumbers)];
		}



		ThreadContext::ThreadContext()
			: lastActiveFiberContext(nullptr)
			, taskScheduler(nullptr)
			, hasNewTasksEvent(EventReset::AUTOMATIC, true)
			, state(ThreadState::ALIVE)
			, workerIndex(0)
			, isExternalDescBuffer(false)
		{
			 descBuffer = Memory::Alloc( GetMemoryRequrementInBytesForDescBuffer() );
		}

		ThreadContext::ThreadContext(void* externalDescBuffer)
			: lastActiveFiberContext(nullptr)
			, taskScheduler(nullptr)
			, queue(DummyQueueFlag::IS_DUMMY_QUEUE)
			, state(ThreadState::ALIVE)
			, workerIndex(0)
			, isExternalDescBuffer(true)
		{
			descBuffer = externalDescBuffer;
		}

		ThreadContext::~ThreadContext()
		{
			if (isExternalDescBuffer == false)
			{
				Memory::Free(descBuffer);
			}
			descBuffer = nullptr;
		}

		size_t ThreadContext::GetMemoryRequrementInBytesForDescBuffer()
		{
			return sizeof(internal::GroupedTask) * TASK_BUFFER_CAPACITY;
		}

		void ThreadContext::SetThreadIndex(uint32_t threadIndex)
		{
			workerIndex = threadIndex;
			random.SetSeed( GetPrimeNumber(threadIndex) );
		}

#ifdef MT_INSTRUMENTED_BUILD

		void ThreadContext::NotifyWaitStarted()
		{
			if (IProfilerEventListener* eventListener = taskScheduler->GetProfilerEventListener())
			{
				eventListener->OnThreadWaitStarted();
			}
		}

		void ThreadContext::NotifyWaitFinished()
		{
			if (IProfilerEventListener* eventListener = taskScheduler->GetProfilerEventListener())
			{
				eventListener->OnThreadWaitFinished();
			}
		}

		void ThreadContext::NotifyTemporaryWorkerThreadJoin()
		{
			if (IProfilerEventListener* eventListener = taskScheduler->GetProfilerEventListener())
			{
				eventListener->OnTemporaryWorkerThreadJoin();
			}
		}

		void ThreadContext::NotifyTemporaryWorkerThreadLeave()
		{
			if (IProfilerEventListener* eventListener = taskScheduler->GetProfilerEventListener())
			{
				eventListener->OnTemporaryWorkerThreadLeave();
			}
		}

		void ThreadContext::NotifyTaskExecuteStateChanged(MT::Color::Type debugColor, const mt_char* debugID, TaskExecuteState::Type type, int32_t fiberIndex)
		{
			if (IProfilerEventListener* eventListener = taskScheduler->GetProfilerEventListener())
			{
				eventListener->OnTaskExecuteStateChanged(debugColor, debugID, type, fiberIndex);
			}
		}

		void ThreadContext::NotifyThreadCreated(uint32_t threadIndex)
		{
			if (IProfilerEventListener* eventListener = taskScheduler->GetProfilerEventListener())
			{
				eventListener->OnThreadCreated(threadIndex);
			}
		}

		void ThreadContext::NotifyThreadStarted(uint32_t threadIndex)
		{
			if (IProfilerEventListener* eventListener = taskScheduler->GetProfilerEventListener())
			{
				eventListener->OnThreadStarted(threadIndex);
			}
		}

		void ThreadContext::NotifyThreadStoped(uint32_t threadIndex)
		{
			if (IProfilerEventListener* eventListener = taskScheduler->GetProfilerEventListener())
			{
				eventListener->OnThreadStoped(threadIndex);
			}
		}

		void ThreadContext::NotifyThreadIdleStarted(uint32_t threadIndex)
		{
			if (IProfilerEventListener* eventListener = taskScheduler->GetProfilerEventListener())
			{
				eventListener->OnThreadIdleStarted(threadIndex);
			}
		}

		void ThreadContext::NotifyThreadIdleFinished(uint32_t threadIndex)
		{
			if (IProfilerEventListener* eventListener = taskScheduler->GetProfilerEventListener())
			{
				eventListener->OnThreadIdleFinished(threadIndex);
			}
		}

#endif

	}

}
