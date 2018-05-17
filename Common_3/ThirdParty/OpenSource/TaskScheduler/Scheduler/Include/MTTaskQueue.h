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

#include <vector>

#include "MTPlatform.h"
#include "MTTools.h"
#include "MTAppInterop.h"


namespace MT
{
	namespace TaskPriority
	{
		enum Type
		{
			HIGH = 0,
			NORMAL = 1,
			LOW = 2,

			COUNT,

			INVALID
		};
	}

	namespace DummyQueueFlag
	{
		enum Type
		{
			IS_DUMMY_QUEUE = 1
		};
	}


	/// \class TaskQueue
	/// \brief thread safe task queue
	///
	template<typename T, uint32 CAPACITY>
	class TaskQueue
	{
		//////////////////////////////////////////////////////////////////////////
		class Queue
		{
			static const int32 ALIGNMENT = 16;
			static const unsigned int MASK = CAPACITY - 1u;

			void* data;
			size_t begin;
			size_t end;

			inline T* Buffer()
			{
				return (T*)(data);
			}

			inline void CopyCtor(T* element, const T & val)
			{
				new(element) T(val);
			}

			inline void MoveCtor(T* element, T && val)
			{
				new(element) T(std::move(val));
			}

			inline void Dtor(T* element)
			{
				MT_UNUSED(element);
				element->~T();
			}

			inline size_t Size() const
			{
				if (IsEmpty())
				{
					return 0;
				}

				size_t count = ((end & MASK) - (begin & MASK)) & MASK;
				return count;
			}

			inline void Clear()
			{
				size_t queueSize = Size();
				for (size_t i = 0; i < queueSize; i++)
				{
					T* pElement = Buffer() + ((begin + i) & MASK);
					Dtor(pElement);
				}

				begin = 0;
				end = 0;
			}

		public:

			Queue()
				: data(nullptr)
				, begin(0)
				, end(0)
			{
			}

			// Queue is just dummy until you call the Create
			void Create()
			{
				size_t bytesCount = sizeof(T) * CAPACITY;
				data = Memory::Alloc(bytesCount, ALIGNMENT);
			}


			~Queue()
			{
				if (data != nullptr)
				{
					Memory::Free(data);
					data = nullptr;
				}
			}

			inline bool HasSpace(size_t itemCount)
			{
				if ((Size() + itemCount) >= CAPACITY)
				{
					return false;
				}

				return true;
			}

			inline bool Add(const T& item)
			{
				MT_VERIFY(data, "Can't add items to dummy queue", return false; );

				if ((Size() + 1) >= CAPACITY)
				{
					return false;
				}

				size_t index = (end & MASK);
				T* pElement = Buffer() + index;
				CopyCtor( pElement, item );
				end++;

				return true;
			}


			inline bool TryPopOldest(T & item)
			{
				if (IsEmpty())
				{
					return false;
				}

				MT_VERIFY(data, "Can't pop items from dummy queue", return false; );

				size_t index = (begin & MASK);
				T* pElement = Buffer() + index;
				begin++;
				item = *pElement;
				Dtor(pElement);
				return true;
			}

			inline bool TryPopNewest(T & item)
			{
				if (IsEmpty())
				{
					return false;
				}

				MT_VERIFY(data, "Can't pop items from dummy queue", return false; );

				end--;
				size_t index = (end & MASK);
				T* pElement = Buffer() + index;
				item = *pElement;
				Dtor(pElement);
				return true;
			}

			inline bool IsEmpty() const
			{
				return (begin == end);
			}
		};
		//////////////////////////////////////////////////////////////////////////

		MT::Mutex mutex;
		Queue queues[TaskPriority::COUNT];

	public:

		MT_NOCOPYABLE(TaskQueue);

		TaskQueue()
		{
			for(uint32 i = 0; i < MT_ARRAY_SIZE(queues); i++)
			{
				queues[i].Create();
			}
		}

		TaskQueue(DummyQueueFlag::Type)
		{
			// Create dummy queue.
		}

		~TaskQueue()
		{
		}

		bool Add(const T* itemArray, size_t count)
		{
			MT::ScopedGuard guard(mutex);

			// Check for space for all queues.
			// At the moment it is not known exactly in what queue items will be added.
			for(size_t i = 0; i < MT_ARRAY_SIZE(queues); i++)
			{
				Queue& queue = queues[i];
				if (!queue.HasSpace(count))
				{
					return false;
				}
			}

			// Adding the tasks into the appropriate queue
			for(size_t i = 0; i < count; i++)
			{
				const T& item = itemArray[i];

				uint32 queueIndex = (uint32)item.desc.priority;
				MT_ASSERT(queueIndex < MT_ARRAY_SIZE(queues), "Invalid task priority");

				Queue& queue = queues[queueIndex];
				bool res = queue.Add(itemArray[i]);
				MT_USED_IN_ASSERT(res);
				MT_ASSERT(res == true, "Sanity check failed");
			}

			return true;
		}


		bool TryPopOldest(T & item)
		{
			MT::ScopedGuard guard(mutex);
			for(uint32 queueIndex = 0; queueIndex < TaskPriority::COUNT; queueIndex++)
			{
				Queue& queue = queues[queueIndex];
				if (queue.TryPopOldest(item))
				{
					return true;
				}
			}
			return false;
		}

		bool TryPopNewest(T & item)
		{
			MT::ScopedGuard guard(mutex);
			for(uint32 queueIndex = 0; queueIndex < TaskPriority::COUNT; queueIndex++)
			{
				Queue& queue = queues[queueIndex];
				if (queue.TryPopNewest(item))
				{
					return true;
				}
			}
			return false;
		}


	};
}
