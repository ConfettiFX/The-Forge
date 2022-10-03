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


namespace MT
{

	// Hybrid spin wait
	//
	// http://www.1024cores.net/home/lock-free-algorithms/tricks/spinning
	//
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	class SpinWait
	{
		int32 iteration;

	public:

		static const int32 YIELD_CPU_THRESHOLD = 10;
		static const int32 YIELD_CPU_THRESHOLD2 = 20;
		static const int32 YIELD_THREAD_THRESHOLD = 40;
		static const int32 YIELD_SLEEP0_THRESHOLD = 200;


		SpinWait()
			: iteration(0)
		{
		}

		void Reset()
		{
			iteration = 0;
		}

		bool IsActive() const
		{
			return (iteration != 0);
		}

		int32 SpinOnce()
		{
			if (iteration <= YIELD_CPU_THRESHOLD)
			{
				MT::YieldProcessor();
			} else
			{
				if (iteration <= YIELD_CPU_THRESHOLD2)
				{
					for (int32 i = 0; i < 50; i++)
					{
						MT::YieldProcessor();
					}
				} else
				{
					if (iteration <= YIELD_THREAD_THRESHOLD)
					{
						MT::YieldThread();
					} else
					{
						if (iteration <= YIELD_SLEEP0_THRESHOLD)
						{
							MT::Thread::Sleep(0);
						} else
						{
							MT::Thread::Sleep(1);
						}
					}
				}
			}

			int32 retValue = iteration;
			if (iteration < INT_MAX)
			{
				iteration++;
			}
			return retValue;
		}
	};


	/*

	Brute force spin wait.  For testing purposes only!
	
	*/
	inline void SpinSleepMicroSeconds(uint32 microseconds)
	{
		int64 desiredTime = GetTimeMicroSeconds() + microseconds;
		for(;;)
		{
			int64 timeNow = GetTimeMicroSeconds();
			if (timeNow > desiredTime)
			{
				break;
			}
			YieldProcessor();
		}
	}

	/*

	Brute force spin wait. For testing purposes only!
	
	*/
	inline void SpinSleepMilliSeconds(uint32 milliseconds)
	{
		SpinSleepMicroSeconds(milliseconds * 1000);
	}

}
