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

#ifndef __MT_EVENT_KERNEL__
#define __MT_EVENT_KERNEL__


namespace MT
{
	//
	//
	//
	class Event
	{
		::MW_HANDLE eventHandle;

	public:

		MT_NOCOPYABLE(Event);

		Event()
		{
			static_assert(sizeof(Event) == sizeof(::MW_HANDLE), "sizeof(Event) is invalid");
			eventHandle = nullptr;
		}

		Event(EventReset::Type resetType, bool initialState)
		{
			eventHandle = nullptr;
			Create(resetType, initialState);
		}

		~Event()
		{
			CloseHandle(eventHandle);
			eventHandle = nullptr;
		}

		void Create(EventReset::Type resetType, bool initialState)
		{
			if (eventHandle != nullptr)
			{
				CloseHandle(eventHandle);
			}

			MW_BOOL bManualReset = (resetType == EventReset::AUTOMATIC) ? 0 : 1;
			MW_BOOL bInitialState = initialState ? 1 : 0;
			eventHandle = ::CreateEventW(nullptr, bManualReset, bInitialState, nullptr);
		}

		void Signal()
		{
			SetEvent(eventHandle);
		}

		void Reset()
		{
			ResetEvent(eventHandle);
		}

		bool Wait(uint32 milliseconds)
		{
			MW_DWORD res = WaitForSingleObject(eventHandle, milliseconds);
			return (res == MW_WAIT_OBJECT_0);
		}

	};

}


#endif
