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
#include "MTTypes.h"
#include "MTPlatform.h"

namespace MT
{
	template<class T>
	T Min(T a, T b)
	{
		return a < b ? a : b;
	}

	template<class T>
	T Max(T a, T b)
	{
		return a < b ? b : a;
	}

	template<class T>
	T Clamp(T val, T min, T max)
	{
		return Min(max, Max(min, val));
	}


	//////////////////////////////////////////////////////////////////////////
	class Timer
	{
		uint64 startMicroSeconds;
	public:
		Timer() : startMicroSeconds(MT::GetTimeMicroSeconds())
		{
		}

		uint32 GetPastMicroSeconds() const
		{
			return (uint32)(MT::GetTimeMicroSeconds() - startMicroSeconds);
		}

		uint32 GetPastMilliSeconds() const
		{
			return (uint32)((MT::GetTimeMicroSeconds() - startMicroSeconds) / 1000);
		}
	};



	//Compile time pow2 check
	//////////////////////////////////////////////////////////////////////////
	template< size_t N, size_t C = 1 >
	struct IsPow2Recurse
	{
		enum
		{
			result = IsPow2Recurse< N / 2, C * 2 >::result
		};
	};

	template< size_t C >
	struct IsPow2Recurse< 0, C >
	{
		enum
		{
			result = C
		};
	};


	template< size_t N >
	struct StaticIsPow2
	{
		enum
		{
			result = IsPow2Recurse< N - 1 >::result == N ? 1 : 0
		};
	};
	


}

