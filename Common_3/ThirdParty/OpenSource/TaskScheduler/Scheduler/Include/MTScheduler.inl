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

	namespace internal
	{
		//generic template
		template<class T>
		inline internal::GroupedTask GetGroupedTask(TaskGroup group, const T * src)
		{
			internal::TaskDesc desc(T::TaskEntryPoint, src, T::GetStackRequirements(), T::GetTaskPriority() );
#ifdef MT_INSTRUMENTED_BUILD
			desc.debugID = T::GetDebugID();
			desc.debugColor = T::GetDebugColor();
#endif
			return internal::GroupedTask( desc, group );
		}

		//template specialization for FiberContext*
		template<>
		inline internal::GroupedTask GetGroupedTask(TaskGroup group, FiberContext* const * src)
		{
			MT_USED_IN_ASSERT(group);
			MT_ASSERT(group == TaskGroup::ASSIGN_FROM_CONTEXT, "Group must be assigned from context");
			FiberContext* fiberContext = *src;
			MT_ASSERT(fiberContext->currentTask.stackRequirements == fiberContext->stackRequirements, "Sanity check failed");
			internal::GroupedTask groupedTask( fiberContext->currentTask, fiberContext->currentGroup );
			groupedTask.awaitingFiber = fiberContext;
			return groupedTask;
		}

		//template specialization for TaskHandle
		template<>
		inline internal::GroupedTask GetGroupedTask(TaskGroup group, const MT::TaskHandle * src)
		{
			MT_ASSERT(src->IsValid(), "Invalid task handle!");
			const internal::TaskDesc & desc = src->GetDesc();
			return internal::GroupedTask( desc, group );
		}



		////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		// Distributes task to threads:
		// | Task1 | Task2 | Task3 | Task4 | Task5 | Task6 |
		// ThreadCount = 4
		// Thread0: Task1, Task5
		// Thread1: Task2, Task6
		// Thread2: Task3
		// Thread3: Task4
		////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		template<class TTask>
		inline bool DistibuteDescriptions(TaskGroup group, TTask* taskArray, ArrayView<internal::GroupedTask>& descriptions, ArrayView<internal::TaskBucket>& buckets)
		{
			size_t index = 0;

			for (size_t bucketIndex = 0; (bucketIndex < buckets.Size()) && (index < descriptions.Size()); ++bucketIndex)
			{
				size_t bucketStartIndex = index;

				for (size_t i = bucketIndex; i < descriptions.Size(); i += buckets.Size())
				{
					descriptions[index] = GetGroupedTask(group, &taskArray[i]);
					index++;
				}

				buckets[bucketIndex] = internal::TaskBucket(&descriptions[bucketStartIndex], index - bucketStartIndex);
			}

			MT_ASSERT(index == descriptions.Size(), "Sanity check");
			return index > 0;
		}

	}





	template<class TTask>
	void TaskScheduler::RunAsync(TaskGroup group, const TTask* taskArray, uint32 taskCount)
	{
		MT_ASSERT(taskCount < (internal::TASK_BUFFER_CAPACITY - 1), "Too many tasks per one Run.");
		MT_ASSERT(!IsWorkerThread(), "Can't use RunAsync inside Task. Use FiberContext.RunAsync() instead.");

		uint32 bytesCountForGroupedTasks = sizeof(internal::GroupedTask) * taskCount;
		ArrayView<internal::GroupedTask> buffer( MT_ALLOCATE_ON_STACK( bytesCountForGroupedTasks ), taskCount );

		uint32 bucketCount = MT::Min((uint32)GetWorkersCount(), taskCount);
		uint32 bytesCountForTaskBuckets = sizeof(internal::TaskBucket) * bucketCount;
		ArrayView<internal::TaskBucket> buckets( MT_ALLOCATE_ON_STACK( bytesCountForTaskBuckets ), bucketCount );

		internal::DistibuteDescriptions(group, taskArray, buffer, buckets);
		RunTasksImpl(buckets, nullptr, false);
	}

}
