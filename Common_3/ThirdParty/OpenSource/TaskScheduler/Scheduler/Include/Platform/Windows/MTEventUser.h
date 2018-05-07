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

#ifndef __MT_EVENT_USER__
#define __MT_EVENT_USER__

namespace MT
{
	//
	//
	//
	class Event
	{
		static const int NOT_SIGNALED = 0;
		static const int SIGNALED = 1;

		::MW_CRITICAL_SECTION criticalSection;
		::MW_CONDITION_VARIABLE condition;

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
				::DeleteCriticalSection(&criticalSection);
				isInitialized = false;
			}
		}

		void Create(EventReset::Type _resetType, bool initialState)
		{
			MT_ASSERT (!isInitialized, "Event already initialized");
			resetType = _resetType;

			::InitializeCriticalSectionAndSpinCount( &criticalSection, 16 );
			::InitializeConditionVariable( &condition );

			value = initialState ? SIGNALED : NOT_SIGNALED;
			isInitialized = true;
			numOfWaitingThreads = 0;
		}

		void Signal()
		{
			MT_ASSERT (isInitialized, "Event not initialized");
			::EnterCriticalSection( &criticalSection );
			value = SIGNALED;
			if (numOfWaitingThreads > 0)
			{
				if (resetType == EventReset::MANUAL)
				{
					::WakeAllConditionVariable( &condition );
				} else
				{
					::WakeConditionVariable( &condition );
				}
			}
			::LeaveCriticalSection( &criticalSection );
		}

		void Reset()
		{
			MT_ASSERT (isInitialized, "Event not initialized");
			MT_ASSERT(resetType == EventReset::MANUAL, "Can't reset, auto reset event");

			::EnterCriticalSection( &criticalSection );
			value = NOT_SIGNALED;
			::LeaveCriticalSection( &criticalSection );
		}

		bool Wait(uint32 milliseconds)
		{
			MT_ASSERT (isInitialized, "Event not initialized");

			::EnterCriticalSection( &criticalSection );
			// early exit if event already signaled
			if ( value != NOT_SIGNALED )
			{
				AutoResetIfNeed();
				::LeaveCriticalSection( &criticalSection );
				return true;
			}

			numOfWaitingThreads++;

			for(;;)
			{
				MW_BOOL ret = ::SleepConditionVariableCS(&condition, &criticalSection, (MW_DWORD)milliseconds);

#if defined(MT_DEBUG) || defined(MT_INSTRUMENTED_BUILD)
				if (ret == 0)
				{
						MW_DWORD err = ::GetLastError();
						MT_USED_IN_ASSERT(err);
						MT_ASSERT(err == MW_ERROR_TIMEOUT, "Unexpected return value from SleepConditionVariable");
				}
#endif

				/*
					https://msdn.microsoft.com/en-us/library/windows/desktop/ms686301(v=vs.85).aspx

					Condition variables are subject to spurious wakeups (those not associated with an explicit wake) and stolen wakeups (another thread manages to run before the woken thread).
					Therefore, you should recheck a predicate (typically in a while loop) after a sleep operation returns.
				*/
				if (value == SIGNALED || ret == 0)
				{
					break;
				}
			}

			numOfWaitingThreads--;
			bool isSignaled = (value != NOT_SIGNALED);
			if (isSignaled)
			{
				AutoResetIfNeed();
			} 

			::LeaveCriticalSection( &criticalSection );
			return isSignaled;
		}

	};

}


#endif
