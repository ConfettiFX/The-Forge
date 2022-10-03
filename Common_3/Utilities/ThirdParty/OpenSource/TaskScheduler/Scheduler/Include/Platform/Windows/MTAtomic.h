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
#include <intrin.h>
#include <cstdint>
#include <type_traits>
#include <xmmintrin.h>

#define MT_ATOMIC_COMPILE_TIME_CHECK \
	static_assert(std::is_pod< Atomic32Base<T> >::value == true, "Atomic32Base must be a POD (plain old data type)"); \
	static_assert(sizeof(T) == sizeof(int32), "Atomic32Base, type T must be equal size as int32"); \
	static_assert(sizeof(int32) == sizeof(long), "Incompatible types, Interlocked* will fail.");

#define MT_ATOMICPTR_COMPILE_TIME_CHECK \
	static_assert(std::is_pod< AtomicPtrBase<T> >::value == true, "AtomicPtrBase must be a POD (plain old data type)");

#ifdef YieldProcessor
	#undef YieldProcessor
#endif


namespace MT
{
	//
	// Full memory barrier
	//
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	inline void HardwareFullMemoryBarrier()
	{
		_mm_mfence();
	}

	//
	// Signals to the processor to give resources to threads that are waiting for them.
	//
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	inline void YieldProcessor()
	{
		_mm_pause();
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
			T tmp = _InterlockedExchangeAdd((volatile long*)&_value, sum) + sum;
			mt_acquire_fence();
			return tmp;
		}

		// The function returns the resulting incremented value.
		T IncFetch()
		{ MT_ATOMIC_COMPILE_TIME_CHECK

			mt_release_fence();
			T tmp = _InterlockedIncrement((volatile long*)&_value);
			mt_acquire_fence();
			return tmp;
		}

		// The function returns the resulting decremented value.
		T DecFetch()
		{ MT_ATOMIC_COMPILE_TIME_CHECK

			mt_release_fence();
			T tmp = _InterlockedDecrement((volatile long*)&_value);
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
			T tmp = _InterlockedExchange((volatile long*)&_value, val); 
			mt_acquire_fence();
			return tmp;
		}

		// The function returns the initial value.
		T CompareAndSwap(T compareValue, T newValue)
		{ MT_ATOMIC_COMPILE_TIME_CHECK

			mt_release_fence();
			T tmp = _InterlockedCompareExchange((volatile long*)&_value, newValue, compareValue);
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
#ifndef MT_PTR64
			static_assert(sizeof(long) == sizeof(void*), "Incompatible types, _InterlockedExchange will fail");
			T* tmp = (T*)_InterlockedExchange((volatile long*)&_value, (long)val); 
#else
			T* tmp = (T*)_InterlockedExchangePointer((void* volatile*)&_value, (void*)val); 
#endif
			mt_acquire_fence();
			return tmp;
		}

		// The function returns the initial value.
		T* CompareAndSwap(const T* compareValue, const T* newValue)
		{ MT_ATOMICPTR_COMPILE_TIME_CHECK

			mt_release_fence();
#ifndef MT_PTR64
			static_assert(sizeof(long) == sizeof(void*), "Incompatible types, _InterlockedCompareExchange will fail");
			T* tmp = (T*)_InterlockedCompareExchange((volatile long*)&_value, (long)newValue, (long)compareValue);
#else
			T* tmp = (T*)_InterlockedCompareExchangePointer((void* volatile*)&_value, (void*)newValue, (void*)compareValue);
#endif
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
