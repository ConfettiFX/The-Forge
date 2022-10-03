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

#ifndef __MT_ATOMIC__
#define __MT_ATOMIC__

#include "MTConfig.h"

#include <type_traits>

#if MT_SSE_INTRINSICS_SUPPORTED
#include <xmmintrin.h>
#else
#include <unistd.h>
#endif



#define MT_ATOMIC_COMPILE_TIME_CHECK \
	static_assert(std::is_pod< Atomic32Base<T> >::value == true, "Atomic32Base must be a POD (plain old data type)"); \
	static_assert(sizeof(T) == sizeof(int32), "Atomic32Base, type T must be equal size as int32"); \

#define MT_ATOMICPTR_COMPILE_TIME_CHECK \
	static_assert(std::is_pod< AtomicPtrBase<T> >::value == true, "AtomicPtrBase must be a POD (plain old data type)");


namespace MT
{
	//
	// Full memory barrier
	//
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	inline void HardwareFullMemoryBarrier()
	{
		__sync_synchronize();
	}

	//
	// Signals to the processor to give resources to threads that are waiting for them.
	//
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	inline void YieldProcessor()
	{
#if MT_SSE_INTRINSICS_SUPPORTED
		_mm_pause();
#else
		usleep(0);
#endif
	}


	//
	// Atomic int (pod type)
	// The operation is ordered in a sequentially consistent manner except for functions marked as relaxed.
	//
	// Note: You must use this type when you need to declare static variable instead of Atomic32
	//
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	template<typename T>
	struct Atomic32Base
	{
		T _value;

		// The function returns the resulting added value.
		T AddFetch(T sum)
		{ MT_ATOMIC_COMPILE_TIME_CHECK

			mt_release_fence();
			T tmp = __sync_add_and_fetch(&_value, sum);
			mt_acquire_fence();
			return tmp;
		}

		// The function returns the resulting incremented value.
		T IncFetch()
		{ MT_ATOMIC_COMPILE_TIME_CHECK

			mt_release_fence();
			T tmp = __sync_add_and_fetch(&_value, 1);
			mt_acquire_fence();
			return tmp;
		}

		// The function returns the resulting decremented value.
		T DecFetch()
		{ MT_ATOMIC_COMPILE_TIME_CHECK

			mt_release_fence();
			T tmp = __sync_sub_and_fetch(&_value, 1);
			mt_acquire_fence();
			return tmp;
		}

		T Load() const
		{ MT_ATOMIC_COMPILE_TIME_CHECK

			T tmp = LoadRelaxed();
			mt_acquire_fence();
			return tmp;
		}

		void Store(T val)
		{ MT_ATOMIC_COMPILE_TIME_CHECK

			mt_release_fence();
			StoreRelaxed(val);
		}

		// The function returns the initial value.
		T Exchange(T val)
		{ MT_ATOMIC_COMPILE_TIME_CHECK

			mt_release_fence();
			T tmp = __sync_lock_test_and_set(&_value, val);
			mt_acquire_fence();
			return tmp;
		}

		// The function returns the initial value.
		T CompareAndSwap(T compareValue, T newValue)
		{ MT_ATOMIC_COMPILE_TIME_CHECK

			mt_release_fence();
			T tmp = __sync_val_compare_and_swap(&_value, compareValue, newValue);
			mt_acquire_fence();
			return tmp;
		}

		// Relaxed operation: there are no synchronization or ordering constraints
		T LoadRelaxed() const
		{ MT_ATOMIC_COMPILE_TIME_CHECK

			return _value;
		}

		// Relaxed operation: there are no synchronization or ordering constraints
		void StoreRelaxed(T val)
		{ MT_ATOMIC_COMPILE_TIME_CHECK

			_value = val;
		}

	};





	//
	// Atomic pointer (pod type)
	//
	// You must use this type when you need to declare static variable instead of AtomicPtr
	//
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	template<typename T>
	struct AtomicPtrBase
	{
		T* _value;
	
		T* Load() const
		{ MT_ATOMICPTR_COMPILE_TIME_CHECK

			T* tmp = LoadRelaxed();
			mt_acquire_fence();
			return tmp;
		}

		void Store(const T* val)
		{ MT_ATOMICPTR_COMPILE_TIME_CHECK

			mt_release_fence();
			StoreRelaxed(val);
		}

		// The function returns the initial value.
		T* Exchange(const T* val)
		{ MT_ATOMICPTR_COMPILE_TIME_CHECK

			mt_release_fence();
			T* tmp = (T*)__sync_lock_test_and_set((void**)&_value, (void*)val);
			mt_acquire_fence();
			return tmp;
		}

		// The function returns the initial value.
		T* CompareAndSwap(const T* compareValue, const T* newValue)
		{ MT_ATOMICPTR_COMPILE_TIME_CHECK

			mt_release_fence();
			T* tmp = (T*)__sync_val_compare_and_swap((void**)&_value, (void*)compareValue, (void*)newValue);
			mt_acquire_fence();
			return tmp;
		}

		// Relaxed operation: there are no synchronization or ordering constraints
		T* LoadRelaxed() const
		{ MT_ATOMICPTR_COMPILE_TIME_CHECK

			return _value;
		}

		// Relaxed operation: there are no synchronization or ordering constraints
		void StoreRelaxed(const T* val)
		{ MT_ATOMICPTR_COMPILE_TIME_CHECK

			_value = (T*)val;
		}

	};



}


#undef MT_ATOMIC_COMPILE_TIME_CHECK


#endif
