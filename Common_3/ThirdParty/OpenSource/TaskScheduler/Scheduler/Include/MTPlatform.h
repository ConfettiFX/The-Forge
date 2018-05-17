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
#include "MTTypes.h"
#include "MTDebug.h"

typedef void (*TThreadEntryPoint)(void* userData);

#define MT_ARRAY_SIZE( arr ) ( sizeof( arr ) / sizeof( (arr)[0] ) )

namespace MT
{
	namespace EventReset
	{
		enum Type
		{
			AUTOMATIC = 0,
			MANUAL = 1,
		};
	}
}



#if MT_PLATFORM_WINDOWS 
	#include "Platform/Windows/MTCommon.h"
#elif MT_PLATFORM_POSIX || MT_PLATFORM_OSX
	#include "Platform/Posix/MTCommon.h"
#else
	#error Platfrom is not supported
#endif

#include "Platform/Common/MTAtomic.h"
#include "Platform/Common/MTSpinWait.h"

namespace MT
{
	//
	//
	//
	class ScopedGuard
	{
		MT::Mutex & mutex;

		ScopedGuard( const ScopedGuard & );
		void operator=( const ScopedGuard &);

	public:

		ScopedGuard(MT::Mutex & _mutex) : mutex(_mutex)
		{
			mutex.Lock();
		}

		~ScopedGuard()
		{
			mutex.Unlock();
		}
	};

	//
	// Simple Linear congruential generator
	//
	class LcgRandom
	{
		uint32 state;

	public:

		LcgRandom()
			: state(2578432553)
		{
		}

		void SetSeed(uint32 seed)
		{
			state = seed;
		}

		uint16 Get()
		{
			state = 214013 * state + 2531011;
			uint16 rnd = (state >> 16);
			return rnd;
		}


	};

}


#if MT_CPP11_SUPPORTED
#define mt_thread_local _Thread_local
#elif MT_GCC_COMPILER_FAMILY
#define mt_thread_local __thread
#elif MT_MSVC_COMPILER_FAMILY
#define mt_thread_local __declspec(thread)
#else
#error Can not define mt_thread_local. Unknown platform.
#endif


