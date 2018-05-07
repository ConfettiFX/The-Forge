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

#ifndef __MT_UTILS__
#define __MT_UTILS__


#include <sys/time.h>

namespace MT
{
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	__inline int64 GetTimeMicroSeconds()
	{
		struct timeval te;
		gettimeofday(&te, nullptr);
		return te.tv_sec * 1000000LL + te.tv_usec;
	}

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	__inline int64 GetTimeMilliSeconds()
	{
		return GetTimeMicroSeconds() / 1000;
	}

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	__inline int64 GetHighFrequencyTime()
	{
		return GetTimeMicroSeconds();
	}

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	__inline int64 GetFrequency()
	{
		return 1000000;
	}

}


#endif
