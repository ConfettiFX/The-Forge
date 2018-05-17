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


	/// \class ArrayView
	/// \brief Simple wrapper to work with raw memory as an array. Includes array bounds checking.
	template<class T>
	class ArrayView
	{
		T* data;
		size_t count;

	public:

		ArrayView()
		{
			data = nullptr;
			count = 0;
		}

		ArrayView(void* memoryChunk, size_t instanceCount)
			: data((T*)memoryChunk)
			, count(instanceCount)
		{
			MT_ASSERT(count == 0 || data, "Invalid data array");
		}

		~ArrayView()
		{
			data = nullptr;
			count = 0;
		}

		const T &operator[]( size_t i ) const
		{
			MT_ASSERT( i < Size(), "bad index" );
			return data[i];
		}

		T &operator[]( size_t i )
		{
			MT_ASSERT( i < Size(), "bad index" );
			return data[i];
		}

		size_t Size() const
		{
			return count;
		}

		bool IsEmpty() const
		{
			return count == 0;
		}

		T* GetRawData()
		{
			return data;
		}

		const T* GetRawData() const
		{
			return data;
		}

	};


}
