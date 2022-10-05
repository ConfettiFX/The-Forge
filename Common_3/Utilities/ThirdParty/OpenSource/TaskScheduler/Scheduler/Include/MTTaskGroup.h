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
	//Task group ID
	class TaskGroup
	{
		int16 id;

	public:

		static const int16 MT_MAX_GROUPS_COUNT = 256;

		enum PredefinedValues
		{
			DEFAULT = 0,
			INVALID = -1,
			ASSIGN_FROM_CONTEXT = -2
		};


		TaskGroup()
		{
			id = INVALID;
		}

		explicit TaskGroup(PredefinedValues v)
		{
			id = (int16)v;
		}

		explicit TaskGroup(int16 _id)
		{
			id = _id;
		}

		static TaskGroup Default()
		{
			return TaskGroup(DEFAULT);
		}

		TaskGroup & operator= (const PredefinedValues & v)
		{
			id = (int16)v;
			return *this;
		}

		bool operator== (const PredefinedValues & v) const
		{
			return (id == v);
		}

		bool operator== (const TaskGroup & other) const
		{
			return (id == other.id);
		}

		bool operator!= (const TaskGroup & other) const
		{
			return (id != other.id);
		}

		int GetValidIndex() const
		{
			MT_ASSERT(IsValid(), "Try to get invalid index");

			return id;
		}

		bool IsValid() const
		{
			if (id == INVALID)
				return false;

			if (id == ASSIGN_FROM_CONTEXT)
				return false;

			return (id >= 0 && id < MT_MAX_GROUPS_COUNT);
		}



	};



}
