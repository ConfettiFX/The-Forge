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
#include <utility>

namespace MT
{
	/// \class ConcurrentQueueLIFO
	/// \brief Lock-Free Multi-Producer Multi-Consumer Queue with fixed capacity.
	///
	/// based on Bounded MPMC queue article by Dmitry Vyukov
	/// http://www.1024cores.net/home/lock-free-algorithms/queues/bounded-mpmc-queue
	///
	template<typename T, uint32 CAPACITY>
	class LockFreeQueueMPMC
	{
		static const int32 ALIGNMENT = 16;
		static const int32 ALIGNMENT_MASK = (ALIGNMENT-1);
		static const uint32 MASK = (CAPACITY - 1);

		struct Cell
		{
			Atomic32<uint32> sequence;
			T data;
		};

		// Raw memory buffer
		byte rawMemory[ sizeof(Cell) * CAPACITY + ALIGNMENT ];

		// Prevent false sharing between threads
		uint8 cacheline0[64];

		Cell* const buffer;

		// Prevent false sharing between threads
		uint8 cacheline1[64];

		Atomic32<uint32> enqueuePos;

		// Prevent false sharing between threads
		uint8 cacheline2[64];

		Atomic32<uint32> dequeuePos;

		inline void MoveCtor(T* element, T && val)
		{
			new(element) T(std::move(val));
		}


	public:

		MT_NOCOPYABLE(LockFreeQueueMPMC);

		LockFreeQueueMPMC()
			: buffer( (Cell*)( ( (uintptr_t)&rawMemory[0] + ALIGNMENT_MASK ) & ~(uintptr_t)ALIGNMENT_MASK ) )
		{
			static_assert( MT::StaticIsPow2<CAPACITY>::result, "LockFreeQueueMPMC capacity must be power of 2");

			for (uint32 i = 0; i < CAPACITY; i++)
			{
				buffer[i].sequence.StoreRelaxed(i);
			}

			enqueuePos.StoreRelaxed(0);
			dequeuePos.StoreRelaxed(0);
		}

		bool TryPush(T && data)
		{
			Cell* cell = nullptr;

			uint32 pos = enqueuePos.LoadRelaxed();
			for(;;)
			{
				cell = &buffer[pos & MASK];

				uint32 seq = cell->sequence.Load();
				int32 dif = (int32)seq - (int32)pos;

				if (dif == 0)
				{
					uint32 nowPos = enqueuePos.CompareAndSwap(pos, pos + 1);
					if (nowPos == pos)
					{
						break;
					} else
					{
						pos = nowPos;
					}
				} else
				{
					if (dif < 0)
					{
						return false;
					} else
					{
						pos = enqueuePos.LoadRelaxed();
					}
				}
			}

			// successfully found a cell
			MoveCtor( &cell->data, std::move(data) );
			cell->sequence.Store(pos + 1);
			return true;
		}


		bool TryPop(T& data)
		{
			Cell* cell = nullptr;
			uint32 pos = dequeuePos.LoadRelaxed();

			for (;;)
			{
				cell = &buffer[pos & MASK];

				uint32 seq = cell->sequence.Load();
				int32 dif = (int32)seq - (int32)(pos + 1);

				if (dif == 0)
				{
					uint32 nowPos = dequeuePos.CompareAndSwap(pos, pos + 1);
					if (nowPos == pos)
					{
						break;
					} else
					{
						pos = nowPos;
					}
				} else
				{
					if (dif < 0)
					{
						return false;
					} else
					{
						pos = dequeuePos.LoadRelaxed();
					}
				}
			}

			// successfully found a cell
			MoveCtor( &data, std::move(cell->data) );
			cell->sequence.Store(pos + MASK + 1);
			return true;
		}

	};


}