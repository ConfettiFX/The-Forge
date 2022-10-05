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

#ifndef __MT_EVENT__
#define __MT_EVENT__

#include <sys/time.h>
#include <sched.h>
#include <errno.h>


namespace MT
{
	//
	//
	//
	class Event
	{
		static const int NOT_SIGNALED = 0;
		static const int SIGNALED = 1;


		pthread_mutex_t	mutex;
		pthread_cond_t	condition;

		EventReset::Type resetType;

		volatile uint32 numOfWaitingThreads;
		volatile int32 value;
		volatile bool isInitialized;

	private:

		void AutoResetIfNeed()
		{
			if (resetType == EventReset::MANUAL)
			{
				return;
			}
			value = NOT_SIGNALED;
		}

	public:

		MT_NOCOPYABLE(Event);


		Event()
			: numOfWaitingThreads(0)
			, isInitialized(false)
		{
		}

		Event(EventReset::Type resetType, bool initialState)
			: numOfWaitingThreads(0)
			, isInitialized(false)
		{
			Create(resetType, initialState);
		}

		~Event()
		{
			if (isInitialized)
			{
				int res = pthread_cond_destroy( &condition );
				MT_USED_IN_ASSERT(res);
				MT_ASSERT(res == 0, "pthread_cond_destroy - failed");

				res = pthread_mutex_destroy( &mutex );
				MT_USED_IN_ASSERT(res);
				MT_ASSERT(res == 0, "pthread_mutex_destroy - failed");
			}
		}

		void Create(EventReset::Type _resetType, bool initialState)
		{
			MT_ASSERT (!isInitialized, "Event already initialized");

			resetType = _resetType;

			int res = pthread_mutex_init( &mutex, nullptr );
			MT_USED_IN_ASSERT(res);
			MT_ASSERT(res == 0, "pthread_mutex_init - failed");

			res = pthread_cond_init( &condition, nullptr );
			MT_USED_IN_ASSERT(res);
			MT_ASSERT(res == 0, "pthread_cond_init - failed");

			value = initialState ? SIGNALED : NOT_SIGNALED;
			isInitialized = true;
			numOfWaitingThreads = 0;
		}

		void Signal()
		{
			MT_ASSERT (isInitialized, "Event not initialized");

			int res = pthread_mutex_lock( &mutex );
			MT_USED_IN_ASSERT(res);
			MT_ASSERT(res == 0, "pthread_mutex_lock - failed");

			value = SIGNALED;
			if (numOfWaitingThreads > 0)
			{
				if (resetType == EventReset::MANUAL)
				{
					res = pthread_cond_broadcast( &condition );
					MT_USED_IN_ASSERT(res);
					MT_ASSERT(res == 0, "pthread_cond_broadcast - failed");
				} else
				{
					res = pthread_cond_signal( &condition );
					MT_USED_IN_ASSERT(res);
					MT_ASSERT(res == 0, "pthread_cond_signal - failed");
				}
			}

			res = pthread_mutex_unlock( &mutex );
			MT_USED_IN_ASSERT(res);
			MT_ASSERT(res == 0, "pthread_mutex_unlock - failed");
		}

		void Reset()
		{
			MT_ASSERT (isInitialized, "Event not initialized");
			MT_ASSERT(resetType == EventReset::MANUAL, "Can't reset, auto reset event");

			int res = pthread_mutex_lock( &mutex );
			MT_USED_IN_ASSERT(res);
			MT_ASSERT(res == 0, "pthread_mutex_lock - failed");

			value = NOT_SIGNALED;

			res = pthread_mutex_unlock( &mutex );
			MT_USED_IN_ASSERT(res);
			MT_ASSERT(res == 0, "pthread_mutex_unlock - failed");

		}

		bool Wait(uint32 milliseconds)
		{
			MT_ASSERT (isInitialized, "Event not initialized");

			int res = pthread_mutex_lock( &mutex );
			MT_USED_IN_ASSERT(res);
			MT_ASSERT(res == 0, "pthread_mutex_lock - failed");

			// early exit if event already signaled
			if ( value != NOT_SIGNALED )
			{
				AutoResetIfNeed();
				res = pthread_mutex_unlock( &mutex );
				MT_USED_IN_ASSERT(res);
				MT_ASSERT(res == 0, "pthread_mutex_unlock - failed");
				return true;
			}

			numOfWaitingThreads++;

			//convert milliseconds to global posix time
			struct timespec ts;

			struct timeval tv;
			gettimeofday(&tv, NULL);

			uint64_t nanoseconds = ((uint64_t) tv.tv_sec) * 1000 * 1000 * 1000 + (uint64_t)milliseconds * 1000 * 1000 + ((uint64_t) tv.tv_usec) * 1000;

			ts.tv_sec = (time_t)(nanoseconds / 1000 / 1000 / 1000);
			ts.tv_nsec = (long)(nanoseconds - ((uint64_t) ts.tv_sec) * 1000 * 1000 * 1000);

			int ret = 0;
			while(true)
			{
				ret = pthread_cond_timedwait( &condition, &mutex, &ts );
				MT_ASSERT(ret == 0 || ret == ETIMEDOUT || ret == EINTR, "Unexpected return value");

				/*
				
				http://pubs.opengroup.org/onlinepubs/009695399/functions/pthread_cond_timedwait.html

				It is important to note that when pthread_cond_wait() and pthread_cond_timedwait() return without error, the associated predicate may still be false.
				Similarly, when pthread_cond_timedwait() returns with the timeout error, the associated predicate may be true due to an unavoidable race between
				the expiration of the timeout and the predicate state change.
				
				*/
				if (value == SIGNALED || ret == ETIMEDOUT)
				{
					break;
				}
			}

			numOfWaitingThreads--;
			bool isSignaled = (value == SIGNALED);
			
			if (isSignaled)
			{
				AutoResetIfNeed();
			}

			res = pthread_mutex_unlock( &mutex );
			MT_USED_IN_ASSERT(res);
			MT_ASSERT(res == 0, "pthread_mutex_unlock - failed");

			return isSignaled;
		}

	};

}


#endif
