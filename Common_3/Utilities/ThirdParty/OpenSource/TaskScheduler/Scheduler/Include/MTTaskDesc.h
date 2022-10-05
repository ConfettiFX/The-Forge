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

#include "MTTools.h"
#include "MTPlatform.h"
#include "MTArrayView.h"
#include "MTColorTable.h"
#include "MTStackRequirements.h"


namespace MT
{
	class FiberContext;
	typedef void (*TTaskEntryPoint)(FiberContext & context, const void* userData);
	typedef void (*TPoolTaskDestroy)(const void* userData);


	namespace internal
	{
		////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		// Task description
		////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		struct TaskDesc
		{
			//Task entry point
			TTaskEntryPoint taskFunc;

			//Task destroy func from pool (dtor call)
			TPoolTaskDestroy poolDestroyFunc;

			//Task user data (task data context)
			const void* userData;

			//Stack requirements for task
			MT::StackRequirements::Type stackRequirements;

			//Priority for task
			MT::TaskPriority::Type priority;

#ifdef MT_INSTRUMENTED_BUILD
			const mt_char* debugID;
			MT::Color::Type debugColor;
#endif

			TaskDesc()
				: taskFunc(nullptr)
				, poolDestroyFunc(nullptr)
				, userData(nullptr)
				, stackRequirements(MT::StackRequirements::INVALID)
			{
#ifdef MT_INSTRUMENTED_BUILD
				debugID = nullptr;
				debugColor = MT::Color::Blue;
#endif
			}

			TaskDesc(TTaskEntryPoint _taskFunc, const void* _userData, MT::StackRequirements::Type _stackRequirements, MT::TaskPriority::Type _priority)
				: taskFunc(_taskFunc)
				, poolDestroyFunc(nullptr)
				, userData(_userData)
				, stackRequirements(_stackRequirements)
				, priority(_priority)
			{
#ifdef MT_INSTRUMENTED_BUILD
				debugID = nullptr;
				debugColor = MT::Color::Blue;
#endif
			}

			TaskDesc(TTaskEntryPoint _taskFunc, TPoolTaskDestroy _poolDestroyFunc, const void* _userData, MT::StackRequirements::Type _stackRequirements)
				: taskFunc(_taskFunc)
				, poolDestroyFunc(_poolDestroyFunc)
				, userData(_userData)
				, stackRequirements(_stackRequirements)
			{
#ifdef MT_INSTRUMENTED_BUILD
				debugID = nullptr;
				debugColor = MT::Color::Blue;
#endif
			}

			bool IsValid()
			{
				return (taskFunc != nullptr);
			}
		};
	}

}
