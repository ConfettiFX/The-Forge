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


namespace MT
{

	namespace TaskID
	{
		//unused_id is any odd number, valid_id should be always only even numbers
		static const int UNUSED = 1;
	}


	//forward declaration
	class TaskHandle;


	/// \class PoolElementHeader
	/// \brief 
	//////////////////////////////////////////////////////////////////////////
	struct PoolElementHeader
	{
		//Task id (timestamp)
		Atomic32<int32> id;

		internal::TaskDesc desc;

	public:

		PoolElementHeader(int _id)
			: id(_id)
		{
		}

		static bool DestoryByHandle(const MT::TaskHandle & handle);
	};


	/// \class TaskPoolElement
	/// \brief 
	//////////////////////////////////////////////////////////////////////////
	template<typename T>
	class PoolElement : public PoolElementHeader
	{
	public:

		// Storage for task
		T task;

		PoolElement(int _id, T && _task)
			: PoolElementHeader(_id)
			, task( std::move(_task) )
		{

#if MT_CLANG_COMPILER_FAMILY
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
#endif
			static_assert( offsetof(PoolElement<T>, task) == sizeof(PoolElementHeader), "Invalid offset for task in PoolElement");
#if MT_CLANG_COMPILER_FAMILY
#pragma clang diagnostic pop
#endif
			
			desc.poolDestroyFunc = T::PoolTaskDestroy;
			desc.taskFunc = T::TaskEntryPoint;
			desc.stackRequirements = T::GetStackRequirements();
			desc.priority = T::GetTaskPriority();
			desc.userData = &task;

#ifdef MT_INSTRUMENTED_BUILD
			desc.debugID = T::GetDebugID();
			desc.debugColor = T::GetDebugColor();
#endif
		}

	};


	/// \class TaskHandle
	/// \brief 
	//////////////////////////////////////////////////////////////////////////
	class TaskHandle
	{
		int32 check_id;

	protected:

		friend struct PoolElementHeader;

		PoolElementHeader* task;

	public:

		//default ctor
		TaskHandle()
			: check_id(TaskID::UNUSED)
			, task(nullptr)
		{

		}

		//ctor
		TaskHandle(int _id, PoolElementHeader* _task)
			: check_id(_id)
			, task(_task)
		{
		}

		//copy ctor
		TaskHandle(const TaskHandle & other)
			: check_id(other.check_id)
			, task(other.task)
		{
		}

		//move ctor
		TaskHandle(TaskHandle && other)
			: check_id(other.check_id)
			, task(other.task)
		{
			other.check_id = TaskID::UNUSED;
			other.task = nullptr;
		}

		~TaskHandle()
		{
		}

		bool IsValid() const
		{
			if (task == nullptr)
			{
				return false;
			}

			if (check_id != task->id.Load())
			{
				return false;
			}

			return true;
		}


		// assignment operator
		TaskHandle & operator= (const TaskHandle & other)
		{
			check_id = other.check_id;
			task = other.task;

			return *this;
		}

		// move assignment operator
		TaskHandle & operator= (TaskHandle && other)
		{
			check_id = other.check_id;
			task = other.task;

			other.check_id = TaskID::UNUSED;
			other.task = nullptr;

			return *this;
		}

		const internal::TaskDesc & GetDesc() const
		{
			MT_ASSERT(IsValid(), "Task handle is invalid");
			return task->desc;
		}


	};




	//////////////////////////////////////////////////////////////////////////
	inline bool PoolElementHeader::DestoryByHandle(const MT::TaskHandle & handle)
	{
		if (!handle.IsValid())
		{
			return false;
		}

		if (handle.task->desc.poolDestroyFunc == nullptr)
		{
			return false;
		}

		if (handle.task->desc.userData == nullptr)
		{
			return false;
		}

		//call destroy func
		handle.task->desc.poolDestroyFunc(handle.task->desc.userData);
		return true;
	}

	



	/// \class TaskPool
	/// \brief 
	//////////////////////////////////////////////////////////////////////////
	template<typename T, size_t N>
	class TaskPool
	{
		static const int32 ALIGNMENT = 16;

		typedef PoolElement<T> PoolItem;

		//
		static const size_t MASK = (N - 1);

		void* data;
		Atomic32<int32> idGenerator;
		Atomic32<int32> index;

		inline PoolItem* Buffer()
		{
			return (PoolItem*)(data);
		}

		inline void MoveCtor(PoolItem* element, int id, T && val)
		{
			new(element) PoolItem(id, std::move(val));
		}

	public:

		MT_NOCOPYABLE(TaskPool);

		TaskPool()
			: idGenerator(0)
			, index(0)
		{
			static_assert( MT::StaticIsPow2<N>::result, "Task pool capacity must be power of 2");

			size_t bytesCount = sizeof(PoolItem) * N;
			data = Memory::Alloc(bytesCount, ALIGNMENT);

			for(size_t idx = 0; idx < N; idx++)
			{
				PoolItem* pElement = Buffer() + idx;
				pElement->id.Store(TaskID::UNUSED);
			}
		}

		~TaskPool()
		{
			if (data != nullptr)
			{

				for(size_t idx = 0; idx < N; idx++)
				{
					PoolItem* pElement = Buffer() + idx;

					int preValue = pElement->id.Exchange(TaskID::UNUSED);
					if (preValue != TaskID::UNUSED)
					{
						pElement->task.~T();
					}
				}

				Memory::Free(data);
				data = nullptr;
			}
		}

		TaskHandle TryAlloc(T && task)
		{
			int idx = index.IncFetch() - 1;

			int clampedIdx = (idx & MASK);

			PoolItem* pElement = Buffer() + clampedIdx;

			bool isUnused = ((pElement->id.Load() & 1 ) != 0);

			if (isUnused == false)
			{
				//Can't allocate more, next element in circular buffer is already used
				return TaskHandle();
			}

			//generate next even number for id
			int id = idGenerator.AddFetch(2);
			MoveCtor( pElement, id, std::move(task) );
			return TaskHandle(id, pElement);
		}


		TaskHandle Alloc(T && task)
		{
			TaskHandle res = TryAlloc(std::move(task));
			MT_ASSERT(res.IsValid(), "Pool allocation failed");
			return res;
		}

	};

}
