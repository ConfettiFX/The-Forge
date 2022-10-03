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

namespace MT
{

	template<class TTask>
	void FiberContext::RunSubtasksAndYield(TaskGroup taskGroup, const TTask* taskArray, size_t taskCount)
	{
		MT_ASSERT(taskCount < (internal::TASK_BUFFER_CAPACITY - 1), "Too many tasks per one Run.");
		MT_ASSERT(threadContext, "ThreadContext is nullptr");

		TaskScheduler& scheduler = *(threadContext->taskScheduler);

		ArrayView<internal::GroupedTask> buffer(threadContext->descBuffer, taskCount);

		size_t bucketCount = MT::Min((size_t)scheduler.GetWorkersCount(), taskCount);
		ArrayView<internal::TaskBucket> buckets(MT_ALLOCATE_ON_STACK(sizeof(internal::TaskBucket) * bucketCount), bucketCount);

		internal::DistibuteDescriptions(taskGroup, taskArray, buffer, buckets);
		RunSubtasksAndYieldImpl(buckets);
	}

	template<class TTask>
	void FiberContext::RunAsync(TaskGroup taskGroup, const TTask* taskArray, size_t taskCount)
	{
		MT_ASSERT(taskCount < (internal::TASK_BUFFER_CAPACITY - 1), "Too many tasks per one Run.");

		MT_ASSERT(threadContext, "ThreadContext is nullptr");
		MT_ASSERT(threadContext->taskScheduler->IsWorkerThread(), "Can't use RunAsync outside Task. Use TaskScheduler.RunAsync() instead.");

		TaskScheduler& scheduler = *(threadContext->taskScheduler);

		ArrayView<internal::GroupedTask> buffer(threadContext->descBuffer, taskCount);

		size_t bucketCount = MT::Min((size_t)scheduler.GetWorkersCount(), taskCount);
		ArrayView<internal::TaskBucket>	buckets(MT_ALLOCATE_ON_STACK(sizeof(internal::TaskBucket) * bucketCount), bucketCount);

		internal::DistibuteDescriptions(taskGroup, taskArray, buffer, buckets);
		scheduler.RunTasksImpl(buckets, nullptr, false);
	}




}
