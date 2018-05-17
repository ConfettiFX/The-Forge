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

#include "MTConfig.h"

#if MT_MSVC_COMPILER_FAMILY
#include <crtdefs.h>
#elif MT_GCC_COMPILER_FAMILY
#include <sys/types.h>
#include <stddef.h>
#else
#error Compiler is not supported
#endif

#if MT_MSVC_COMPILER_FAMILY
#define MT_NORETURN 
#elif MT_GCC_COMPILER_FAMILY
#define MT_NORETURN // [[ noreturn ]] 
#else
#error Can not define MT_NORETURN. Unknown platform.
#endif



#define MT_DEFAULT_ALIGN (16)

namespace MT
{
	// Memory allocator interface.
	//////////////////////////////////////////////////////////////////////////
	struct Memory
	{
		struct StackDesc
		{
			void* stackBottom;
			void* stackTop;

			char* stackMemory;
			size_t stackMemoryBytesCount;


			StackDesc()
				: stackBottom(nullptr)
				, stackTop(nullptr)
				, stackMemory(nullptr)
				, stackMemoryBytesCount(0)
			{
			}

			size_t GetStackSize()
			{
				return (char*)stackTop - (char*)stackBottom;
			}
		};


		static void* Alloc(size_t size, size_t align = MT_DEFAULT_ALIGN);
		static void Free(void* p);

		static StackDesc AllocStack(size_t size);
		static void FreeStack(const StackDesc & desc);
	};


	struct Diagnostic
	{
		MT_NORETURN static void ReportAssert(const char* condition, const char* description, const char* sourceFile, int sourceLine);
	};


}
