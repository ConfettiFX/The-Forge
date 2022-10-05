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

#ifndef __MT_MUTEX__
#define __MT_MUTEX__

#include <pthread.h>


namespace MT
{
	class ScopedGuard;

	//
	//
	//
	class Mutex
	{
		pthread_mutexattr_t mutexAttr;
		pthread_mutex_t mutex;

	public:

		MT_NOCOPYABLE(Mutex);

		Mutex()
		{
			int res = pthread_mutexattr_init(&mutexAttr);
			MT_USED_IN_ASSERT(res);
			MT_ASSERT(res == 0, "pthread_mutexattr_init - failed");

			res = pthread_mutexattr_settype(&mutexAttr, PTHREAD_MUTEX_RECURSIVE);
			MT_USED_IN_ASSERT(res);
			MT_ASSERT(res == 0, "pthread_mutexattr_settype - failed");

			res = pthread_mutex_init(&mutex, &mutexAttr);
			MT_USED_IN_ASSERT(res);
			MT_ASSERT(res == 0, "pthread_mutex_init - failed");
		}

		~Mutex()
		{
			int res = pthread_mutex_destroy(&mutex);
			MT_USED_IN_ASSERT(res);
			MT_ASSERT(res == 0, "pthread_mutex_destroy - failed");

			res = pthread_mutexattr_destroy(&mutexAttr);
			MT_USED_IN_ASSERT(res);
			MT_ASSERT(res == 0, "pthread_mutexattr_destroy - failed");
		}

		friend class MT::ScopedGuard;

	private:

		void Lock()
		{
			int res = pthread_mutex_lock(&mutex);
			MT_USED_IN_ASSERT(res);
			MT_ASSERT(res == 0, "pthread_mutex_lock - failed");
		}
		void Unlock()
		{
			int res = pthread_mutex_unlock(&mutex);
			MT_USED_IN_ASSERT(res);
			MT_ASSERT(res == 0, "pthread_mutex_unlock - failed");
		}

	};


}


#endif
